# Web App Graceful Corruption Recovery Design

Editor: dmurph@google.com Last Modified: 2026-04-08

## Objective

Prevent browser crashes when the Web App Database fails to open or is detected
as corrupted (e.g., due to a browser downgrade). Instead of crashing, present an
error dialog to the user and gracefully recover by clearing local web app state
and starting fresh.

## 1. Corruption Detection & Error Propagation

Currently, `WebAppDatabase::MigrateDatabase()` checks the database version
against the expected version and calls `CHECK_EQ` on mismatches. This causes the
browser to crash upon a downgrade. Instead of crashing:

- `MigrateDatabase` will return a `WebAppDatabaseOpenResult` if the database
  version is higher than expected or if parsing fails catastrophically.
- `WebAppDatabase::OnAllDataAndMetadataRead` will capture this result.
- **Crucially**, even on corruption, `ParseProtobufs` will extract the
  `webapps::AppId`s and their corresponding `GURL` start urls from `state.apps`
  that were successfully parsed (even partially). This list of IDs and URLs is
  necessary to clean up OS integration and other app data later.

## 2. Delaying Initialization & Showing UI

Currently, `WebAppSyncBridge::Init` passes an `initialized_callback` to
`WebAppDatabase::OpenDatabase`. If an error occurs, `WebAppDatabase` should NOT
silently swallow `initialized_callback` and only run `error_callback_`. Instead,
the initialization flow will be adjusted:

- `WebAppDatabase::OnDatabaseOpened` and `OnAllDataAndMetadataRead` will invoke
  a specific `RegistryOpenedCallback` signature that now includes a
  `WebAppDatabaseOpenResult` and the
  `std::vector<std::pair<webapps::AppId, GURL>>` extracted from the corrupt DB.
- `WebAppSyncBridge::Init` passes these values up to
  `WebAppProvider::OnSyncBridgeReady`.
- `WebAppProvider` intercepts the error:
  - Halts standard initialization of subsystems.
  - Calls `ShowProfileErrorDialog(ProfileErrorType::DB_WEB_APP_DATA, ...)` to
    inform the user asynchronously.
  - Immediately posts a background task to proceed with the graceful cleanup
    asynchronously while the dialog is visible. This ensures we don't block the
    UI thread and cleanup begins without waiting for user interaction.

## 3. Graceful Cleanup

When corruption is handled in `WebAppProvider`, we perform a comprehensive
background cleanup by calling `RemoveWebAppJob::RemoveForCorruptDatabase`. To
de-risk crashes on startup (which could cause infinite crash loops if the
cleanup itself crashes), the sequence of operations is ordered to clear the
database first:

1. **Database Deletion**: We immediately issue a command to clear the Web App
   LevelDB using `syncer::DataTypeStore::DeleteAllDataAndMetadata` on the store.
2. **Non-Database App Data**: Once the database deletion completes, we execute
   the following cleanup steps in parallel (synchronized using
   `base::ConcurrentClosures`):
   - **OS Integration**: Iterate through the extracted `webapps::AppId`s and
     call
     `provider.os_integration_manager().Synchronize(app_id, ..., {.force_unregister_os_integration = true})`
     to remove OS shortcuts, file handlers, etc.
   - **OS Resources Directory**: Call
     `provider.file_utils()->DeleteFileRecursively(...)` for the directory
     returned by
     `GetOsIntegrationResourcesDirectoryForApp(profile_path, app_id, start_url)`
     to wipe leftover shortcut metadata/icons from the filesystem. This is done
     after `Synchronize` completes for each app. We explicitly use the
     provider's `file_utils()` abstraction so that file system deletions can be
     intercepted and verified in unit tests via `TestFileUtils`.
   - **Icon Data**: Call `provider.icon_manager().DeleteData(app_id, ...)` to
     remove on-disk icons.
   - **Translation Data**: Call
     `provider.translation_manager().DeleteTranslations(app_id, ...)` to remove
     downloaded translations.
3. **Preferences Cleanup**: Once all parallel tasks complete, we perform the
   final cleanup on prefs:
   - Clear the `prefs::kWebAppsPreferences` dictionary entirely to wipe
     app-specific guardrails.
   - Clear the `prefs::kWebAppsDailyMetrics` dictionary to prevent stale
     reporting.
   - Set `prefs::kShouldGarbageCollectStoragePartitions` to `true` to ensure
     isolated storage used by corrupted Isolated Web Apps is garbage collected
     on the next startup.

This ensures that if a crash happens during the more complex OS integration or
file deletion steps, the database is already cleared, and the browser will not
attempt to run the recovery flow again on the next startup.

## 4. Re-initialization

After the cleanup tasks complete, `WebAppProvider` will recreate or reset the
`WebAppSyncBridge` and `WebAppDatabase` instances, and restart the
`StartSyncBridge()` flow. A new `DataTypeStore` will be created natively because
the old one was wiped, resulting in a clean, empty state.

## 5. Metrics

To accurately track and report the occurrence and causes of Web App Database
corruption, a new UMA metric will be introduced:

- **Histogram**: `WebApp.Database.OpenResult`
- **Enum Values** (defined in
  `tools/metrics/histograms/metadata/webapps/enums.xml` as
  `WebAppDatabaseOpenResult`):
  - `0`: `Success`
  - `1`: `OpenError` (Triggered when the sync system's LevelDB fails to open).
  - `2`: `ReadError` (Triggered when the sync system's LevelDB fails to read).
  - `3`: `DowngradeDetected` (Triggered when the DB version is higher than the
    expected version, which causes a failure in `MigrateDatabase`).

## 6. Unittest Strategy

To verify this behavior without causing flakes, we should create a new unit test
in `chrome/browser/web_applications/web_app_database_unittest.cc`:

- **Simulate Corruption**: Inject a mock `FakeWebAppDatabaseFactory` that has
  pre-populated LevelDB data with a high metadata version (e.g., version 999) to
  emulate a downgrade before `WebAppProvider` starts.
- **Verification**:
  - Call `test::AwaitStartWebAppProviderAndSubsystems(profile())`
  - Assert that `ShowProfileErrorDialog` is invoked.
  - Assert that OS integration cleanup is triggered for the salvaged apps. (e.g.
    `FakeOsIntegrationManager` or `TestFileUtils` checks).
  - Verify exact file cleanup via `TestFileUtils`: Build a dynamic array of
    gMock matchers (e.g. `FilePathContainsPath`) to ensure that, for each
    salvaged `app_id`, paths corresponding to both the
    `Manifest Resources/<app_id>` directory and the `_crx_<app_id>` OS resources
    directory are explicitly tracked as deleted using `testing::IsSupersetOf`.
  - Wait for the background cleanup task to complete.
  - Verify that the `DataTypeStore` has been cleared.
  - Verify that `prefs::kWebAppsPreferences` is empty and
    `prefs::kShouldGarbageCollectStoragePartitions` is true.
  - Assert that `WebAppProvider` successfully completes `OnRegistryReady` with
    an empty web app registry, proving the system recovered and started fresh.

## 7. Alternatives Considered

### Handling Corruption inside WebAppDatabase or WebAppSyncBridge

**Why it was rejected**: It might seem simpler to have `WebAppDatabase` detect
the corruption, immediately delete the `DataTypeStore`, and let the startup
continue seamlessly. However, `WebAppDatabase` only has scope over the LevelDB.
If it silently deleted the database and continued, the browser would start with
an empty web app registry, leaving behind "orphaned" OS integrations (shortcuts,
file handlers), icon files on disk, and translation data. By bubbling the error
up to `WebAppProvider`, we have access to all the necessary subsystem managers
(like `OsIntegrationManager` and `WebAppIconManager`) and the `Profile` (to
clear preferences) so we can perform a comprehensive, system-wide cleanup.

### Reusing `RemoveWebAppJob` for Cleanup

Instead of manually calling the individual subsystem managers (e.g.,
`os_integration_manager_->Synchronize`, `icon_manager_->DeleteData`), we
considered reusing `RemoveWebAppJob` to perform the cleanup, since it already
contains the robust logic to completely uninstall a web app.

**Pros:**

- Prevents duplicating the uninstallation sequence.
- We wouldn't have to keep both code locations in sync if the uninstallation
  logic changes in the future.

**Cons:**

- `RemoveWebAppJob` is designed to run within the `WebAppCommandManager` and
  requires an `AllAppsLock`.
- The `WebAppCommandManager` does not start scheduling jobs until the
  `WebAppProvider` has fully completed initialization (e.g., `OnRegistryReady`).
  Since the database corruption intercepts initialization and halts it, the
  command manager will never start, and the job would never run.
- `RemoveWebAppJob` relies on the `WebAppRegistrar` to verify the app exists,
  fetch isolation data, and check user uninstall sources. In a corruption
  scenario, the `WebAppRegistrar` is completely empty. `GetAppById` will return
  null, and the job will instantly abort.
- `RemoveWebAppJob` calls `sync_bridge().BeginUpdate()` to remove the app from
  the database, but the `WebAppSyncBridge` is broken and uninitialized.

**Conclusion:** We cannot reuse the `RemoveWebAppJob` class instance or schedule
it through the command manager because the entire web app system state is
fundamentally broken during a corruption event. We must perform a "raw" cleanup
by bypassing the standard locks and calling the un-initialized subsystem
managers directly, using only the raw `app_id` and `start_url` strings salvaged
from the corrupted database protobufs. However, to keep the uninstallation logic
organized and close to its related code, we encapsulate this raw cleanup logic
inside a static method: `RemoveWebAppJob::RemoveForCorruptDatabase`.

## 8. Implementation Challenges & Lessons Learned

During the implementation and testing of this recovery flow, we encountered two
significant issues that highlighted complexities in our test environment and
subsystem dependencies:

### 1. Test Environment Contamination via Global Factories

Initially, `RemoveWebAppJob::RemoveForCorruptDatabase` used the global
`DataTypeStoreServiceFactory::GetForProfile()` to retrieve the store to wipe the
corrupted database. However, `FakeWebAppProvider` uses
`FakeWebAppDatabaseFactory` which provides a separate in-memory store for unit
tests. By wiping the global store instead of the mocked in-memory store, the
test database remained corrupted. This caused the provider to continually detect
the corruption and trigger an endless downgrade recovery loop on startup.

**Lesson Learned**: All cleanup logic must strictly utilize the
`provider.database_factory()` to ensure it respects the
`FakeWebAppDatabaseFactory` during unit testing, avoiding cross-contamination
with the global profile state.

### 2. Extension System Test Hangs

The cleanup process sets `prefs::kShouldGarbageCollectStoragePartitions` to
true, which schedules a background task on the next startup to garbage collect
isolated storage partitions. When `GarbageCollectStoragePartitionsCommand` runs,
it explicitly waits for the extension system to be ready. It originally achieved
this by calling `extensions::OnExtensionSystemReady` directly. In unit tests,
the global `ExtensionSystem` KeyedService is not fully initialized, causing the
command (and subsequently the test suite) to hang indefinitely.

**Lesson Learned**: To prevent tests from hanging on uninitialized global state,
all web app code that depends on the extension system MUST go through the
`ExtensionsManager` abstraction. By updating the command to use
`lock_->extensions_manager().OnExtensionSystemReady()` and ensuring
`FakeWebAppProvider` supplies a `FakeExtensionsManager` (which defaults to a
"ready" state), the test suite immediately fulfills the readiness check without
needing complex manual setup.

### 3. DataTypeStore Lifecycle in Asynchronous Cleanup

During the implementation of `RemoveWebAppJob::RemoveForCorruptDatabase`, we
discovered that the `DataTypeStore` instance was being destroyed prematurely.
Specifically, when calling `DeleteAllDataAndMetadata`, the operation is
asynchronous. If the `std::unique_ptr<DataTypeStore>` goes out of scope before
the deletion callback is invoked, the underlying task is cancelled, and the
database is not wiped.

**Lesson Learned**: When performing asynchronous operations on `DataTypeStore`,
ensure that the store instance is kept alive (e.g., by moving it into the
callback's closure) until the operation completes.

### 4. UI Thread Deadlock in Tests during Recovery

The browser test `WebAppDatabaseRecoveryBrowserTest` initially timed out due to
a UI thread deadlock. The test blocked on `WebAppProvider::GetForTest` waiting
for the provider to be ready. However, the provider's recovery flow posts tasks
to the UI thread (e.g., for OS integration cleanup and database deletion). Since
the UI thread was blocked waiting for initialization, these tasks could not run,
causing a deadlock.

**Lesson Learned**: In tests that block on provider initialization while
asynchronous recovery tasks are expected to run,
`base::RunLoop::Type::kNestableTasksAllowed` must be used to allow the UI thread
to process nested tasks.

### 5. Asynchronous Flow Control with `ConcurrentClosures` and `.Then()`

In `RemoveWebAppJob::RemoveForCorruptDatabase`, we needed to perform multiple
asynchronous cleanups in parallel (OS integration, icons, translations) and then
execute a final cleanup step (prefs) after all of them finished.

**Lesson Learned**: `base::ConcurrentClosures` is an excellent tool for waiting
on multiple parallel async operations without manual counter management.
Chaining the final step using `.Then()` on the completion closure makes the
control flow clean and declarative.
