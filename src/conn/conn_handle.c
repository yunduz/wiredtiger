/*-
 * Copyright (c) 2014-2015 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_connection_init --
 *	Structure initialization for a just-created WT_CONNECTION_IMPL handle.
 */
int
__wt_connection_init(WT_CONNECTION_IMPL *conn)
{
	//yunduz print
	// char tid[128];
	// __wt_thread_id(tid, sizeof(tid));
	// printf("--- __wt_connection_init tid: %s\n", tid);
	WT_SESSION_IMPL *session;
	u_int i;

	session = conn->default_session;

	for (i = 0; i < WT_HASH_ARRAY_SIZE; i++) {
		TAILQ_INIT(&conn->dhhash[i]);	/* Data handle hash lists */
		TAILQ_INIT(&conn->fhhash[i]);	/* File handle hash lists */
	}

	TAILQ_INIT(&conn->dhqh);		/* Data handle list */
	TAILQ_INIT(&conn->dlhqh);		/* Library list */
	TAILQ_INIT(&conn->dsrcqh);		/* Data source list */
	TAILQ_INIT(&conn->fhqh);		/* File list */
	TAILQ_INIT(&conn->collqh);		/* Collator list */
	TAILQ_INIT(&conn->compqh);		/* Compressor list */
	TAILQ_INIT(&conn->encryptqh);		/* Encryptor list */
	TAILQ_INIT(&conn->extractorqh);		/* Extractor list */

	TAILQ_INIT(&conn->lsmqh);		/* WT_LSM_TREE list */

	/* Setup the LSM work queues. */
	TAILQ_INIT(&conn->lsm_manager.switchqh);
	TAILQ_INIT(&conn->lsm_manager.appqh);
	TAILQ_INIT(&conn->lsm_manager.managerqh);

	/* Configuration. */
	WT_RET(__wt_conn_config_init(session));

	/* Statistics. */
	__wt_stat_init_connection_stats(&conn->stats);

	/* Locks. */
	WT_RET(__wt_spin_init(session, &conn->api_lock, "api"));
	WT_RET(__wt_spin_init(session, &conn->checkpoint_lock, "checkpoint"));
	WT_RET(__wt_spin_init(session, &conn->dhandle_lock, "data handle"));
	WT_RET(__wt_spin_init(session, &conn->encryptor_lock, "encryptor"));
	WT_RET(__wt_spin_init(session, &conn->fh_lock, "file list"));
	WT_RET(__wt_rwlock_alloc(session,
	    &conn->hot_backup_lock, "hot backup"));
	WT_RET(__wt_spin_init(session, &conn->reconfig_lock, "reconfigure"));
	WT_RET(__wt_spin_init(session, &conn->schema_lock, "schema"));
	WT_RET(__wt_spin_init(session, &conn->table_lock, "table creation"));
	WT_RET(__wt_calloc_def(session, WT_PAGE_LOCKS(conn), &conn->page_lock));
	for (i = 0; i < WT_PAGE_LOCKS(conn); ++i)
		WT_RET(
		    __wt_spin_init(session, &conn->page_lock[i], "btree page"));

	/* Setup the spin locks for the LSM manager queues. */
	WT_RET(__wt_spin_init(session,
	    &conn->lsm_manager.app_lock, "LSM application queue lock"));
	WT_RET(__wt_spin_init(session,
	    &conn->lsm_manager.manager_lock, "LSM manager queue lock"));
	WT_RET(__wt_spin_init(
	    session, &conn->lsm_manager.switch_lock, "LSM switch queue lock"));
	WT_RET(__wt_cond_alloc(
	    session, "LSM worker cond", 0, &conn->lsm_manager.work_cond));

	/*
	 * Generation numbers.
	 *
	 * Start split generations at one.  Threads publish this generation
	 * number before examining tree structures, and zero when they leave.
	 * We need to distinguish between threads that are in a tree before the
	 * first split has happened, and threads that are not in a tree.
	 */
	conn->split_gen = 1;

	/*
	 * Block manager.
	 * XXX
	 * If there's ever a second block manager, we'll want to make this
	 * more opaque, but for now this is simpler.
	 */
	WT_RET(__wt_spin_init(session, &conn->block_lock, "block manager"));
	for (i = 0; i < WT_HASH_ARRAY_SIZE; i++)
		TAILQ_INIT(&conn->blockhash[i]);/* Block handle hash lists */
	TAILQ_INIT(&conn->blockqh);		/* Block manager list */

	return (0);
}

/*
 * __wt_connection_destroy --
 *	Destroy the connection's underlying WT_CONNECTION_IMPL structure.
 */
int
__wt_connection_destroy(WT_CONNECTION_IMPL *conn)
{
	// yunduz print
	// printf("--- __wt_connection_destroy\n");

	//yunduz print 
	// uint64_t combined_counter = RLU_RELAXED_GET_COUNTER_VAL_UINT64_T(conn->stats.p_rlu_cursor_next, 0);
	// printf("yunduz: combined conn counter = %" PRIu64 "\n", combined_counter);
	// printf("yunduz: combined conn counter original = %" PRIu64 "\n", conn->stats.cursor_next.v);


	WT_DECL_RET;
	WT_SESSION_IMPL *session;
	u_int i;

	/* Check there's something to destroy. */
	if (conn == NULL)
		return (0);

	session = conn->default_session;

	// //yunduz rlu
	// combined_counter = RLU_RELAXED_GET_COUNTER_VAL_UINT64_T(session->dhandle->stats.p_rlu_cursor_next, 0);
	// printf("yunduz: combined dsrc counter = %" PRIu64 "\n", combined_counter);
	// printf("yunduz: combined dsrc counter original = %" PRIu64 "\n", session->dhandle->stats.cursor_next.v);
	printf("yunduz: combined conn cursor_next = %" PRIu64 "\n", WT_CONN_STAT(session, cursor_next));
	// printf("yunduz: combined conn rwlock_write = %" PRIu64 "\n", WT_CONN_STAT(session, rwlock_write));
	// printf("yunduz: combined conn cursor_create = %" PRIu64 "\n", WT_CONN_STAT(session, cursor_create));
	// printf("yunduz: combined conn block_read = %" PRIu64 "\n", WT_CONN_STAT(session, block_read));
	printf("yunduz: conn cursor next strict = %d\n", TEST_STRICT_RLU_CONN_CURSOR_NEXT_STAT(session));

	/*
	 * Close remaining open files (before discarding the mutex, the
	 * underlying file-close code uses the mutex to guard lists of
	 * open files.
	 */
	WT_TRET(__wt_close(session, &conn->lock_fh));

	/* Remove from the list of connections. */
	__wt_spin_lock(session, &__wt_process.spinlock);
	TAILQ_REMOVE(&__wt_process.connqh, conn, q);
	__wt_spin_unlock(session, &__wt_process.spinlock);

	/* Configuration */
	__wt_conn_config_discard(session);		/* configuration */

	__wt_conn_foc_discard(session);			/* free-on-close */

	__wt_spin_destroy(session, &conn->api_lock);
	__wt_spin_destroy(session, &conn->block_lock);
	__wt_spin_destroy(session, &conn->checkpoint_lock);
	__wt_spin_destroy(session, &conn->dhandle_lock);
	__wt_spin_destroy(session, &conn->encryptor_lock);
	__wt_spin_destroy(session, &conn->fh_lock);
	WT_TRET(__wt_rwlock_destroy(session, &conn->hot_backup_lock));
	__wt_spin_destroy(session, &conn->reconfig_lock);
	__wt_spin_destroy(session, &conn->schema_lock);
	__wt_spin_destroy(session, &conn->table_lock);
	for (i = 0; i < WT_PAGE_LOCKS(conn); ++i)
		__wt_spin_destroy(session, &conn->page_lock[i]);
	__wt_free(session, conn->page_lock);

	/* Free allocated memory. */
	__wt_free(session, conn->cfg);
	__wt_free(session, conn->home);
	__wt_free(session, conn->error_prefix);
	__wt_free(session, conn->sessions);

	//yunduz rlu
	// RLU_THREAD_FINISH(conn->main_rlu_thread_data);
	// __wt_free(session, conn->main_rlu_thread_data);
	// printf("RLU_THREAD_FINISH\n");
	// printf("Freed main_rlu_thread_data\n");

	__wt_free(NULL, conn);
	return (ret);
}
