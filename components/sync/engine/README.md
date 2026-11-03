# Sync Engine (`components/sync/engine/`)

This directory implements the core Sync protocol engine (part B of the
[architecture overview](../README.md)). Unless noted otherwise, classes in this
directory run on the dedicated **Sync sequence** (`SequencedTaskRunner`),
isolated from the UI thread and individual model sequences.

## Core Components

* [`SyncScheduler`](sync_scheduler.h) /
  [`SyncSchedulerImpl`](sync_scheduler_impl.h): Orchestrates when configuration,
  nudge, and poll sync cycles execute, tracking global throttling/backoff and
  per-datatype state via [`NudgeTracker`](cycle/nudge_tracker.h).
* [`Syncer`](syncer.h): Executes individual sync cycles (`GetUpdates` downloads
  and `Commit` uploads).
* [`ServerConnectionManager`](net/server_connection_manager.h) /
  [`SyncServerConnectionManager`](net/sync_server_connection_manager.h): Handles
  HTTP transport (`PostBuffer()`) and reports server connection status changes.
* [`DataTypeWorker`](data_type_worker.h): Bridges each connected data type
  between the Sync sequence and its model sequence.

## Authentication, Scheduling & Retry Hierarchy

Access token acquisition and retry behavior span the UI thread
([`SyncAuthManager`](../service/sync_auth_manager.h)) and the Sync sequence
([`SyncSchedulerImpl`](sync_scheduler_impl.h) and
[`SyncServerConnectionManager`](net/sync_server_connection_manager.h)):

1. **Per-request short retries**: On desktop, `IdentityManager` fetches access
   tokens directly; on mobile, the responsibility for fetching access tokens
   lies outside of Chrome (in the platform account manager). `IdentityManager`
   performs a bounded number of fast exponential retries (`< ~30s` total) before
   reporting a failure to `SyncAuthManager`, so brief transient network blips
   resolve at this layer without surfacing to Sync.
2. **Sync-side long-tail scheduling**: Unbounded recovery over minutes or hours
   after fast retries are exhausted (as well as cross-platform backoff on mobile
   platforms where token-service backoff is disabled) is owned by Sync's own
   retry timers (`SyncAuthManager` and `SyncSchedulerImpl`).
3. **Token rejection (`CONNECTION_AUTH_ERROR` / `HTTP 401`)**: When the Sync
   server rejects a request with `HTTP 401`—or when
   `SyncServerConnectionManager` rejects an expired or missing token locally
   with `HTTP 401` before sending a request—`SyncAuthManager` invalidates the
   cached token, and Sync-layer backoff rate-limits repeated retries.
4. **Local sync (`ignore_auth_credentials_`)**: Local sync operates without a
   signed-in account or access token.

## Auth & Transport Metrics

Key histograms emitted along the scheduling and HTTP transport path include
`Sync.URLFetchAccessToken`, `Sync.URLFetchResponse`, `Sync.CommitResponse`,
`Sync.PostedDataTypeCommitRequest`, and `Sync.PostedGetUpdatesOrigin`.

Some metrics along this path are emitted before an actual network request is
sent and may therefore be duplicated when a cycle is retried.
