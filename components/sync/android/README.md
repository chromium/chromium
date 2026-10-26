# Sync Android Bridge

This directory exposes Chromium Sync's [service layer](../README.md) to Android
Java code such as settings UI and sign-in flows. It provides the
`org.chromium.components.sync` Java package and the JNI bridge that forwards
calls to the native [`SyncService`](../service/sync_service.h) and
[`SyncUserSettings`](../service/sync_user_settings.h) interfaces and propagates
state changes back to Java listeners.

Everything here runs on the UI thread, the same sequence as the native
`SyncService`.

## How to use it

*   [`SyncService`](java/src/org/chromium/components/sync/SyncService.java) is
    the public Java interface, mirroring
    [`syncer::SyncService`](../service/sync_service.h) and
    [`syncer::SyncUserSettings`](../service/sync_user_settings.h) (implemented
    by
    [`SyncServiceImpl`](java/src/org/chromium/components/sync/SyncServiceImpl.java)
    and [`SyncServiceAndroidBridge`](sync_service_android_bridge.h), and
    obtained via
    [`SyncServiceFactory.getForProfile()`](/chrome/browser/sync/android/java/src/org/chromium/chrome/browser/sync/SyncServiceFactory.java);
    tests can inject
    [`FakeSyncServiceImpl`](/chrome/browser/sync/test/android/java/src/org/chromium/chrome/browser/sync/FakeSyncServiceImpl.java)
    via `SyncServiceFactory.setInstanceForTesting()`). It exposes transport
    state, active and user-selected data types, auth and user-actionable errors,
    passphrase and trusted-vault state, and local-data queries and migration
    triggers.
*   [`SyncService.SyncStateChangedListener`](java/src/org/chromium/components/sync/SyncService.java):
    implement it and call `addSyncStateChangedListener()` to learn whenever the
    underlying sync state changes (`SyncServiceObserver::OnStateChanged()`).
*   [`LocalDataDescription`](java/src/org/chromium/components/sync/LocalDataDescription.java)
    and [`Passphrase`](java/src/org/chromium/components/sync/Passphrase.java):
    value and helper types used with `SyncService` — the Java mirror of
    [`syncer::LocalDataDescription`](../service/local_data_description.h)
    returned by `getLocalDataDescriptions()` (with JNI domain-summary formatting
    in [`local_data_description.cc`](../service/local_data_description.cc)), and
    static predicates for `@PassphraseType` values.
*   [`SyncFeatureMap`](java/src/org/chromium/components/sync/SyncFeatureMap.java):
    the `FeatureMap` accessor for querying Sync `base::Feature` flags exposed in
    [`sync_feature_map.cc`](sync_feature_map.cc).
*   [`ExplicitPassphrasePlatformClient`](java/src/org/chromium/components/sync/ExplicitPassphrasePlatformClient.java):
    a package-private `ServiceLoaderUtil` hook called from
    [`SyncServiceImpl`](../service/sync_service_impl.cc) to share
    explicit-passphrase [`NigoriKey`](../protocol/nigori_specifics.proto)
    material with external sync clients on the device.
*   Passing `SyncService` across JNI: Java code passing a `SyncService` to its
    own `@NativeMethods` can annotate the parameter with
    `@JniType("syncer::SyncService*")` to receive the native pointer directly
    (conversion in
    [`sync_service_android_bridge.h`](sync_service_android_bridge.h)). The
    `@IntDef` enums used on `SyncService` (`DataType`, `UserSelectableType`,
    `PassphraseType`, `TransportState`, `UserActionableError`) are generated
    from the corresponding C++ enums in [`../base/`](../base/) and
    [`sync_service.h`](../service/sync_service.h).

## Design docs

No public design doc covers this directory specifically. Related public pages:

*   [Sync
    diagnostics](https://www.chromium.org/developers/design-documents/sync/diagnostics/):
    `chrome://sync-internals`, whose node dump is also exposed to Java tests
    through `SyncService.getAllNodes()`.
*   [Sync design
    documents](https://www.chromium.org/developers/design-documents/sync/):
    index of the public Sync design docs.
