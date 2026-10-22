# Sync Invalidations

Invalidations are push messages from the Sync server saying that new data is
available for some data types, so the client can run `GetUpdates` right away
instead of waiting for the next poll (step 1 of the remote change workflow in
the [architecture overview](../README.md)). Polling continues regardless, so
invalidations reduce latency rather than being the only trigger for a sync
cycle. This directory implements the client side of that channel on top of FCM
(Firebase Cloud Messaging) via [`components/gcm_driver/`](../../gcm_driver/): it
registers the profile with FCM, hands incoming messages to listeners and tracks
which data types the device wants invalidations for.

Everything here runs on the UI thread, the sequence that owns the profile's
`KeyedService`s.

## How to use it

*   [`SyncInvalidationsService`](sync_invalidations_service.h) is the public
    interface, a per-profile `KeyedService` (implemented by
    [`SyncInvalidationsServiceImpl`](sync_invalidations_service_impl.h) and
    created by `SyncInvalidationsServiceFactory`, see
    [`chrome/browser/sync/sync_invalidations_service_factory.h`](/chrome/browser/sync/sync_invalidations_service_factory.h)).
    [`SyncServiceImpl`](../service/sync_service_impl.h) drives its lifecycle
    through `StartListening()`, `SetInterestedDataTypes()`, `StopListening()`
    and `StopListeningPermanently()`.
*   [`InvalidationsListener`](invalidations_listener.h): implement it and call
    `AddListener()` to receive incoming invalidation payloads. A listener added
    late gets the most recently buffered messages replayed, so invalidations
    that arrive while the engine is still starting are not lost. Current
    listeners include [`SyncEngineImpl`](../service/glue/sync_engine_impl.h),
    which hands the payload to the engine on the sync sequence, and
    [`SyncInternalsMessageHandler`](../../browser_sync/sync_internals_message_handler.h)
    for `chrome://sync-internals`.
*   [`FCMRegistrationTokenObserver`](fcm_registration_token_observer.h):
    implement it and call `AddTokenObserver()` to learn when the token returned
    by `GetFCMRegistrationToken()` is obtained, changes or is cleared.
*   [`InterestedDataTypesHandler`](interested_data_types_handler.h): the single
    handler, set with `SetInterestedDataTypesHandler()`, is told whenever the
    set of interested data types changes.
*   Subscribing to data types: the server only sends invalidations to a device
    for the data types it has declared interest in. The client declares them,
    together with its FCM registration token, in its own `DeviceInfo` entity
    ([`InvalidationSpecificFields`](../protocol/device_info_specifics.proto)).
    [`DeviceInfoSyncServiceImpl`](../../sync_device_info/device_info_sync_service_impl.h)
    implements both `InterestedDataTypesHandler` and
    `FCMRegistrationTokenObserver` for that purpose and commits a refreshed
    `DeviceInfo` whenever either changes.

## Design docs

No public design doc covers this directory specifically. Related public pages:

*   [Sync
    diagnostics](https://www.chromium.org/developers/design-documents/sync/diagnostics/):
    `chrome://sync-internals`, where received invalidations can be inspected.
*   [Sync design
    documents](https://www.chromium.org/developers/design-documents/sync/):
    index of the public Sync design docs.
