# Sync Tab Context component

Component responsible for sync-ing tab context to the user's account and
across devices in a privacy-preserving manner leaning on strong encryption.

The main public API is exposed via [TabContextSyncService][1], offering a
platform-agnostic API to upload tab context via explicit function calls.
This functionality will be supported on desktop and mobile platforms.

[1]: /components/sync_tab_context/tab_context_sync_service.h
