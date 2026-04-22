# [Web Apps](../README.md) - Testing

The following tests are expected for writing code in this system:

- Unit tests
- Browser tests
- Integration tests

## Testing Infrastructure

The WebAppProvider system is architected in a way that attempts to allow tests
to swap out 'dependencies' for testing.

One way of doing this is through 'managers' on the WebAppProvider that wrap
dependencies. These can be swapped out using the `FakeWebAppProvider` to
facilitate easy testing. This **must** be done before system startup
(`test::AwaitStartWebAppProviderAndSubsystems(profile())`). This almost
exclusively used by unit tests, but browser tests can also use a
`FakeWebAppProvider` by using the `FakeWebAppProviderCreator` class, as long as
it calls `StartImpl()` on the FakeWebAppProvider immediately.

Examples of dependencies that can be swapped out on the `FakeWebAppProvider`,
and if it is done by default:

- `FakeWebContentsManager` (default): Used extensively when installing web apps
  to mock the manifest and page states that a real `WebContents` would provide.
- `FakeWebAppUiManager` (default): Helpful for faking UI surface responses (like
  update dialogs or launch operations).
- `FakeWebAppOriginAssociationManager`: Frequently used with
  `set_pass_through(true)` to instantly pass scope validation without actually
  fetching association files over the network.
- `FakeOsIntegrationManager` (default): Ensures that no os integration
  operations actually occur on the system, while the `current_os_integration()`
  states are still kept up-to-date. Provides a few testing hooks to fake some os
  integration fetching.
- `FakeExtensionsManager` (default): Fakes interaction with the extensions
  system and prevents tests from hanging by instantly fulfilling readiness
  checks that would otherwise wait for the real `ExtensionSystem`.
- `FakeWebAppDatabaseFactory` (default): Provides an in-memory database for
  testing.
- `TestFileUtils` (optional): Tracks file deletion operations directly,
  especially useful for intercepting OS integration cleanup paths without
  interacting with the real OS filesystem. Set via `SetFileUtils()`.

There are some other dependencies that are owned/faked with a different model:

- `WebAppProvider` provides testing hooks like `SetClockForTesting()` and
  `DisableDelayedPostStartupWorkForTesting()` prevents
  `WebAppProvider::DoDelayedPostStartupWork` from being called automatically.
- OS Integration faking also exists at a 'lower level' than the
  `WebAppProvider`, and is used by both unit tests and browsertests via the
  `OsIntegrationTestOverrideImpl`. The base class for both the unit tests and
  browser tests register this by default. Unit tests will need to call
  `fake_provider().UseRealOsIntegrationManager();` during setup to ensure that
  the real `OsIntegrationManager` is used, which then integrates with the
  `OsIntegrationTestOverride` system.
  - Example:
    `EXPECT_TRUE(os_integration_override().IsShortcutCreated(profile(), app_id, "App Name"));`

In rare cases, usually when making tests for systems or features that have the
WebAppProvider system as a dependency (like with isolated web apps), it can be
helpful to override the `WebAppCommandScheduler` with a custom test subclass to
verify that a specific operation is scheduled.

Finally, various testing utilities can be found in the following directories:

- `chrome/browser/web_applications/test/`
- `chrome/browser/ui/web_applications/test/`

## Logging and debugging

The unit and browser test base classes automatically print a snapshot of
chrome://web-app-internals (built
[here](/chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.cc))
to console on test failures. This can be a powerful debugging tool, especially
to see debug information about run commands or navigation captures. The command
line flag `--disable-web-app-internals-log` can be used to disable this logging.

Debug logging is available real-time via DVLOGs sprinkled throughout the system
in useful locations. Use the [logging system's](/base/logging.h) `--vmodule`
flag to enable this like `--vmodule=web_app*=1`.

## Unit tests

Unit tests are the fastest tests to execute and are expected to be used to test
most cases, especially error cases. They are usually built on the `WebAppTest`
base class, and use the `FakeWebAppProvider` to customize (or not) the
[dependencies](../README.md#external-dependencies) of the `WebAppProvider`
system. Some of these fakes are necessary for the unittest to function, as
things like `WebContents` and `Browser` are not fully functional in unit test
environments.

A common location for installation convenience methods for unittests is
`chrome/browser/web_applications/test/web_app_install_test_utils.h`.

See the [Writing Unit Tests](skills/create-web-applications-unittest/SKILL.md)
skill for a practical guide.

## Browser tests

Browser tests inside the system use the
[`WebAppBrowserTestBase`](https://source.chromium.org/search?q=WebAppBrowserTestBase)
base class. Most new features should use unit tests to cover all of the detailed
test cases, especially all of the failure cases, as these are easier to simulate
and run. However, it is important to have a few browser tests as integration
tests to ensure the functionality works end-to-end. These are also required if
there is logic to test that exists that cannot be reached without the 'ui'
state, like logic in the `WebAppTabHelper`, toolbar, dialogs, etc.

Notes:

- Useful helpers for browser tests live in
  [web_app_browsertest_util.h](chrome/browser/ui/web_applications/test/web_app_browsertest_util.h).
- The `PRE_` test functionality generally works with the `WebAppProvider`
  functionality (e.g. installed web apps stay installed in the profile) but some
  OS integration bits might break, as currently the `OsIntegrationTestOverride`
  system creates temporary directories outside of the profile directory, and
  thus are cleaned up. This is technically only a requirement of the app shim
  application on Mac, which currently needs to be in the user's home directory
  to be recognized by the OS.

See [Writing Browser Tests](skills/create-web-applications-browsertest/SKILL.md)
for a practical guide.

## Integration tests

We have a custom integration testing framework that we use due to the complexity
of our use-cases. See
[integration-testing-framework.md](integration-testing-framework.md) for more
information.

**It is a good idea to think about your integration tests early & figure out
your CUJs with the team. Having your CUJs and integration tests working early
greatly speeds up development & launch time.**

## Testing OS integration

It is very common to test OS integration. By default, OS integration is
suppressed if the test extends
[`WebAppTest`](https://source.chromium.org/search?q=web_app_test.h) or
[`WebAppBrowserTestBase`](https://source.chromium.org/search?q=WebAppBrowserTestBase).

End-to-end OS integration testing is facilitated using the
[`OsIntegrationTestOverride`](https://source.chromium.org/search?q=OsIntegrationTestOverride).
If OS integration CAN be tested in an automated way, this class will do so. If
not, the existence of this override will stub-out the OS integration at the
lowest level to test as much of our code as possible.

To verify that OS integration (like creating shortcuts) actually occurred during
a test, you can interrogate this override. It is available on both base classes:
`fake_os_integration()` for `WebAppTest` and `os_integration_override()` for
`WebAppBrowserTestBase`.

```cpp
EXPECT_TRUE(os_integration_override().IsShortcutCreated(
    profile(), app_id, "App Name"));
```

## Common issue: Waiting

Many operations that happen at higher levels than the commands/scheduling system
in the WebAppProvider require that tests wait for async operations to complete.

### `WebAppProvider` commands

[`WebAppCommandManager::AwaitAllCommandsCompleteForTesting`](https://source.chromium.org/search?q=AwaitAllCommandsCompleteForTesting)
will wait for all commands to complete. This will mostly handle all async tasks
in the local `WebAppProvider`.

### Observers

Tests can use observers to wait for a specific change to occur. The
`chrome/browser/web_applications/test/web_app_test_observers.h` file provides
several helpful utilities for this, such as:

- `WebAppTestInstallObserver`: Waits for an app to be installed.
- `WebAppTestUninstallObserver`: Waits for an app to be uninstalled.
- `WebAppTestManifestUpdatedObserver`: Waits for a manifest update.

This is an acceptable pattern, but it often requires the test to also wait for
the underlying commands handling that change to finish executing. To ensure the
system runs all queued tasks and is in a stable state after an observed event,
call:

```cpp
provider().command_manager().AwaitAllCommandsCompleteForTesting();
```

### Tabs & Browsers (Browser Tests)

Utilities in `chrome/test/base/ui_test_utils.h` are often helpful. Examples:

- `ui_test_utils::AllBrowserTabAddedWaiter`: Waits for a tab to be added
  anywhere (works for both app browser and regular browser).
- `ui_test_utils::BrowserCreatedObserver` or
  `ui_test_utils::BrowserDestroyedObserver`: Waits for a browser to be created
  or removed.

### Navigation & Loading (Browser Tests)

- `UrlLoadObserver` - Waits for given url to load anywhere.
- `content::TestNavigationObserver` - Waits for a navigation anywhere or in
  given WebContents. See StartWatchingNewWebContents to watch all web contents.

The web_applications system
[web_app_browsertest_util.h file](https://source.chromium.org/search?q=web_app_browsertest_util.h)
provides the best approximations of waiting for all web app "activity" to be at
least scheduled per `WebContents` (e.g., waiting for the tab helper to see
manifests & schedule an update):

- `test::WaitForLoadCompleteAndMaybeManifestSeen(WebContents&)`
- `test::CompletePageLoadForAllWebContents()`

## Common issue: External Dependency that isn't faked

Sometimes classes use a dependency that either doesn't work or isn't fake-able
in our system.

1. Can you just not depend on that? The best way is to remove the dependency
   entirely if possible.
2. If there is a way to easily fake the dependency that is already supported,
   then do that next. e.g. if it's a `KeyedService`, and the authors have a fake
   version you can use, then use that. See how it is used elsewhere.
3. Create a new interface for this new external dependency, put it on the
   `WebAppProvider`, and create a fake for it.
4. If all else fails, use a browser test.
