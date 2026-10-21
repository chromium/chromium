---
name: webapps-testing-review
description: >-
  Reviews Web Apps unit and browser tests (*_unittest.cc, *_browsertest.cc) in
  chrome/browser/web_applications/, chrome/browser/ui/web_applications/, and
  chrome/browser/ui/views/web_apps/ for awaiting command completion via
  base::test::TestFuture or test observers instead of RunUntilIdle()/sleeps, and
  for configuring FakeWebAppProvider subsystems before provider startup. Use when
  reviewing Web Apps unit tests, browser tests, or test support files.
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

### `WEBAPPS_CONFIGURE_FAKES_BEFORE_PROVIDER_STARTUP`: Configure `FakeWebAppProvider` subsystem managers before calling `AwaitStartWebAppProviderAndSubsystems()`

- **Severity**: ERROR
- **Why**: Every `FakeWebAppProvider::Set*()` setter and
  `UseRealOsIntegrationManager()` calls `CheckNotStartedAndDisconnect()`, which
  crashes immediately with `CHECK(!started_)` and message
  `"Attempted to set a WebAppProvider subsystem after Start() was called."` if
  invoked after `test::AwaitStartWebAppProviderAndSubsystems(profile())`.
- **Look for**: Calls to any `FakeWebAppProvider::Set*()` setter (such as
  `SetRegistrar`, `SetSyncBridge`, `SetIconManager`, `SetCommandManager`,
  `SetScheduler`, `SetWebContentsManager`, `SetOsIntegrationManager`,
  `SetWebAppUiManager`, `SetExtensionsManager`, `SetOriginAssociationManager`)
  or `UseRealOsIntegrationManager()` occurring after
  `test::AwaitStartWebAppProviderAndSubsystems(profile())` in `SetUp()` or test
  bodies.
- **Do not flag**: Configuring or swapping fake managers in the test fixture
  constructor, via `FakeWebAppProviderCreator`, or in `SetUp()` prior to calling
  `test::AwaitStartWebAppProviderAndSubsystems(profile())`, or mutating state on
  an already-registered fake manager (for example,
  `fake_web_contents_manager().GetOrCreatePageState(...)`) after startup.
