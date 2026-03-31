---
name: create-web-applications-unittest
description: Instructions for creating a unit test in the chrome/browser/web_applications and chrome/browser/ui/web_applications directories.
---

# Creating Web Applications Unittests

This skill guides the process of creating a unit test in the
`chrome/browser/web_applications` directory. Read
[the testing documentation](../../testing.md) for information about testing
infrastructure in the [WebAppProvider](../../../README.md) system.

## 1. Test Base Class & System Startup

Inherit from `web_app::WebAppTest`. This provides a profile with a
`FakeWebAppProvider` that is **not** automatically started. You must explicitly
start it by calling `test::AwaitStartWebAppProviderAndSubsystems(profile());` in
your `SetUp()` method.

## 2. Faking Dependencies

Fake out anything needed that isn't faked by default in the `FakeWebAppProvider`
to intercept calls to dependencies. Managers on the `FakeWebAppProvider` must
swap out managers **before** system startup.

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

See [Testing: Testing Infrastructure](../../testing.md#testing-infrastructure)
for a list of common fakes.

## 3. Test Data Setup / Mocking Inputs

Mock inputs and dependencies needed by your test.

- **WebContents or Network**: Use `FakeWebContentsManager` via
  `fake_web_contents_manager()`.
- **UI Interactions**: Use `FakeWebAppUiManager` via `fake_ui_manager()`.
- **Filesystem**: Use `TestFileUtils`.
- **WebApp state**: Install apps using `test::InstallDummyWebApp` or helpers in
  `web_app_install_test_utils.h`.

## 4. Execution & Waiting

Execute your command or trigger your action. Commands should have a built-in
callback to wait on using a utility like `base::TestFuture`. If this cannot
capture the waiting correctly, reference other waiting methods in
[the testing documentation](../../testing.md#common-issue-waiting).

## 5. Validate state & check metrics

When validating state, the best checks use the system's 'public' API or on the
dependencies (e.g. did the correct method get called on the FakeWebAppUiManager,
and does a scheduler operation return the right thing). It is acceptable to also
query internal state, like the `provider().registrar_unsafe()`, if no 'public'
API exists.

Most operations record UMA metrics, and thus a `base::HistogramTester` should be
used to verify them. Sometimes this is a handy way to validate specific edge
cases without needing to create test-only state inspection.

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
