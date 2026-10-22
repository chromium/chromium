---
name: webapps-testing-review
description: >-
  Reviews Web Apps tests: unit, browser, and interactive UI tests
  (*_unittest.cc, *_browsertest.cc, *_interactive_uitest.cc) in
  chrome/browser/web_applications/, chrome/browser/ui/web_applications/, and
  chrome/browser/ui/views/web_apps/, and the web apps sync integration tests in
  chrome/browser/sync/test/integration/web_apps/. It checks command completion
  waits, FakeWebAppProvider setup before provider start, FakeWebContentsManager
  page state, test install helpers, and OS integration test overrides. Use when
  a CL adds or changes these test files.
---

### `WEBAPPS_AWAIT_COMMANDS_INSTEAD_OF_IDLE_OR_SLEEP`: Wait on command completion via `base::test::TestFuture` (or test observers) instead of `RunUntilIdle()` or sleeps

- **Severity**: WARNING
- **Why**: `RunUntilIdle()` and fixed timers do not wait for `WebAppCommand`s
  that are blocked on `PartitionedLockManager` locks or on a `WebContents` load,
  which is a primary source of flaky Web App tests.
- **Look for**: `base::RunLoop().RunUntilIdle()`,
  `task_environment()->RunUntilIdle()`, `base::PlatformThread::Sleep`, or
  fixed-delay `PostDelayedTask` used to wait for a web app command, install,
  update, uninstall, or launch to finish.
- **Fix**: Primarily pass `future.GetCallback()` from a
  `base::test::TestFuture<...>` directly to the command or `scheduler()` call
  (when it accepts a completion callback) and wait via `future.Wait()` /
  `future.Get()`. When no direct completion callback is available on the
  triggered flow, use `WebAppPageWaiter::WaitAndFlushCommands()`, a
  `WebAppTestInstallObserver` / `WebAppTestUninstallObserver` /
  `WebAppTestManifestUpdatedObserver`, or
  `provider().command_manager().AwaitAllCommandsCompleteForTesting()`.
- **Do not flag**: `RunUntilIdle()` used for non-`WebAppProvider` work (such as
  flushing a `MockCallback`), or `base::test::TestFuture` /
  `base::RunLoop::Run()` driven by an explicit completion callback.

### `WEBAPPS_USE_FAKE_PAGE_STATE_NOT_DATA_RETRIEVER`: Set page state through `FakeWebContentsManager`, not new `FakeDataRetriever` or `TestWebAppUrlLoader` uses

- **Severity**: WARNING
- **Why**: `FakeWebAppProvider` sends all `WebContents` access to
  `FakeWebContentsManager`. Fake page state then drives URL loads, manifests,
  and icons together. Custom data retrievers and URL loaders couple tests to
  internal call sequences and drift from real behavior. Tests can schedule
  commands through `provider().scheduler()`, or get a loader or retriever from
  `fake_web_contents_manager().CreateUrlLoader()` and `CreateDataRetriever()`.
- **Look for**: New uses of `TestWebAppUrlLoader`, `FakeDataRetriever`, or
  `MockDataRetriever` in unit tests, for example `SetManifest()` or
  `SetNextLoadUrlResult()` calls. Direct construction of a command or job with
  these fakes, or calls to `SetUrlLoaderForTesting()` or
  `SetDataRetrieverForTesting()`, instead of `provider().scheduler()` or
  `fake_web_contents_manager()`.
- **Do not flag**: Any use of `FakeWebContentsManager` (through
  `fake_web_contents_manager()`), also after provider start. Existing uses of
  `TestWebAppUrlLoader`, `FakeDataRetriever`, or `MockDataRetriever` that the CL
  does not change. The files that define these fakes or the `Set*ForTesting()`
  methods, for example `test/fake_data_retriever.cc`,
  `test/fake_web_contents_manager.cc`, and `jobs/install_placeholder_job.cc`.

### `WEBAPPS_INSTALL_TEST_APPS_WITH_TEST_UTILS`: Install test web apps with the test install helpers, not `ScopedRegistryUpdate::CreateApp()`

- **Severity**: WARNING
- **Why**: `WebAppRegistryUpdate::CreateApp()` writes a raw `WebApp` object into
  the registry. It skips install commands, locks, icon writes, OS integration,
  and install observers. `web_app_registry_update.h` and
  `test/web_app_test_utils.h` forbid `CreateApp()`, `test::CreateWebApp()`,
  `test::CreateWebAppFromSyncProto()`, and `test::CreateRandomWebApp()` for
  installs in tests.
- **Look for**: Calls to `WebAppRegistryUpdate::CreateApp()` (through
  `ScopedRegistryUpdate`, for example `sync_bridge_unsafe().BeginUpdate()`) or
  `test::CreateWebApp()`, `test::CreateWebAppFromSyncProto()`, or
  `test::CreateRandomWebApp()` that install an app in a unit test or a browser
  test.
- **Do not flag**: `test::InstallDummyWebApp()`, `test::InstallWebApp()`,
  `test::InstallWebAppWithoutOsIntegration()`, `InstallWebAppFromPage()`,
  `InstallWebAppFromPageGetBrowser()`, `InstallWebAppFromManifest()`,
  `test::InstallPwaForCurrentUrl()`, `IsolatedWebAppBuilder`, or a scheduler
  command. The registrar, sync bridge, database, proto utils, and `WebApp` unit
  tests (`web_app_registrar_unittest.cc`, `web_app_sync_bridge_unittest.cc`,
  `web_app_database*_unittest.cc`, `web_app_proto_utils_unittest.cc`, and
  `web_app_unittest.cc`). `UpdateApp()` or `DeleteApp()` calls through
  `ScopedRegistryUpdate` that change an app that the test installed before, for
  example in `single_client_web_apps_sync_generated_icon_fix_sync_test.cc`. The
  helper definitions in `test/web_app_test_utils.cc`.
