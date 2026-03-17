---
name: create-web-applications-unittest
description: Instructions for creating a unit test in the chrome/browser/web_applications directory, usually involving installed web applications or web app/PWA functionality.
---

# Creating Web Applications Unittests

This skill guides the process of creating a unit test in the
chrome/browser/web_applications directory. First, read
[/docs/webapps/README.md](/docs/webapps/README.md) to understand how the
WebAppProvider system works.

## 1. Test Base Class & System Startup

Most tests in `chrome/browser/web_applications` should inherit from
`web_app::WebAppTest`. By default this utilizes a `TestingProfile` that
automatically uses a `FakeWebAppProvider` instead of the normal
`WebAppProvider`.

The `FakeWebAppProvider` does *not* automatically start the Web App system. You
must explicitly start it by calling
`test::AwaitStartWebAppProviderAndSubsystems(profile());` in your `SetUp()`
method.

## 2. Faking Dependencies, for Input & Verification

To test without triggering actual downstream operations or without needing, say,
a full `WebContents` and network stack running, the `FakeWebAppProvider`
provides a way to swap out production managers with fake equivalents. This
**must** be done before system startup
(`test::AwaitStartWebAppProviderAndSubsystems(profile())`). Some of these fake
managers are created & used by the FakeWebAppProvider by default.

Examples:

- `FakeWebContentsManager` (default): Used extensively when installing web apps
  to mock the manifest and page states that a real `WebContents` would provide.
- `FakeWebAppUiManager` (default): Helpful for faking UI surface responses (like
  update dialogs or launch operations).
- `FakeWebAppOriginAssociationManager`: Frequently used with
  `set_pass_through(true)` to instantly pass scope validation without actually
  fetching association files over the network.
- `OsIntegrationTestOverrideImpl`: Accessible via
  `WebAppTest::fake_os_integration()`, this becomes useful if
  `fake_provider().UseRealOsIntegrationManager();` is called before
  `AwaitStartWebAppProviderAndSubsystems` in the test setup.
- `FakeOsIntegrationManager` (default): Ensures that no os integration
  operations actually occur on the system, and provides a few testing hooks to
  fake some os integration fetching.
- `FakeExtensionsManager` (default): Fakes interaction with the extensions
  system.
- `FakeWebAppDatabaseFactory` (default): Provides an in-memory database for
  testing.
- others: `WebAppProvider` provides testing hooks like `SetClockForTesting()`
  and `DisableDelayedPostStartupWorkForTesting()` prevents
  `WebAppProvider::DoDelayedPostStartupWork` from being called automatically.

Example:

```cpp
void SetUp() override {
  WebAppTest::SetUp();

  // 1. Swap managers
  fake_provider().SetOriginAssociationManager(
        std::make_unique<FakeWebAppOriginAssociationManager>());

  // 2. Start the system
  test::AwaitStartWebAppProviderAndSubsystems(profile());
}
```

## 3. Test Data Setup / Mocking Inputs

Mock inputs and dependencies needed by the operation or system being tested.

- **WebContents or Network**: If the operation operates on web contents or
  fetches information from the network, use the `FakeWebContentsManager`
  (usually via `WebAppTest::fake_web_contents_manager()`) to set up manifest
  data, page state, and loaded icons before kicking off a fetch manifest
  command.
- **UI Interactions**: Use `FakeWebAppUiManager` to respond to user dialog
  prompts (e.g., accepting a PWA install dialog).
- **Filesystem**: Use `MockFileUtilsWrapper` for file system interactions and
  assertions. The most common use-case here is faking that our disk is full to
  cause a filesystem error for an operation, verifying that the failure is
  handled correctly. It is generally OK for non-error tests to simply use the
  real filesystem.
- **WebApp state**:
  - Testing helpers exist in
    `chrome/browser/web_applications/test/web_app_install_test_utils.h` for
    installing apps in various configurations.
  - Modifying the database to get the web app in necessary input states can be
    done via operations on the `provider().scheduler()` if there a convenient
    operation exists. Otherwise it can be done via a `ScopedRegistryUpdate`
    created via `provider().sync_bridge_unsafe().BeginUpdate()`.

*(Note, `test::InstallDummyWebApp` provides a simpler wrapper if you only need
the app to exist)*.

## 4. Observers and Command Completion

Tests will occasionally use observers to wait for a specific change to occur.
The `chrome/browser/web_applications/test/web_app_test_observers.h` file
provides several helpful utilities for this, such as:

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

## 5. Validate state & check metrics

When validating state, the best checks use the system's 'public' API or on the
dependencies (e.g. did the correct method get called on the FakeWebAppUiManager,
and does a scheduler operation return the right thing). It is acceptable to also
query internal state, like the `provider().registrar_unsafe()`, if no 'public'
API exists.

Most operations should record UMA metrics. If this is the case, use a
`base::HistogramTester` to verify the right metrics were emitted. Sometimes this
is also a handy way to validate specific edge cases were hit without needing to
create test-only state inspection.

## Example Test Fixture

Example text fixture that simply runs a hypothetical command `MyWebAppCommand`
and validates the result.

```cpp
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/scheduler/my_web_app_command_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
// ...

class MyWebAppCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    // 1. Set up fake managers if needed, or customize database state.
    // e.g. fake_provider().Set...

    // 2. Start the WebApp subsystem.
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  // base::HistogramTester histogram_tester_;
};

TEST_F(MyWebAppCommandTest, SuccessCase) {
  // 1. Set up state. (e.g. test::InstallDummyWebApp(...))

  // 2. Run the operation.
  base::test::TestFuture<MyWebAppCommandResult> future;
  provider().scheduler().MyWebAppCommand(
      // Input args
      // ...
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(MyWebAppCommandResult::kSuccess, future.Get<MyWebAppCommandResult>());

  // 3. Verify state
  // e.g. EXPECT_EQ(provider().registrar_unsafe().Get..., ....);
  // EXPECT_THAT(histogram_tester_.GetAllSamples("HistogramName"),
  //             BucketsAre(Bucket(1, 0), Bucket(2, 10), Bucket(3, 5)));
}
```
