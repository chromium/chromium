# Dump Accessibility Tests for Chromium Views

This directory contains dump-based accessibility tests for **Chromium Views**
(`ui/views`), modeled after the web content dump accessibility tests
(e.g. `dump_accessibility_tree_browsertests` and
`dump_accessibility_events_browsertests`), but operating on **Views-backed
accessibility trees** instead of DOM / WebContents.

The core idea is identical to the web content system:

> A test produces a textual dump of accessibility state, which is then compared
> against a checked-in expectation file.
> This makes tests easy to reason about, easy to review, and easy to rebaseline.

The difference is **this system works with Chromium Views**. It shouldn't be used to validate web content.

---

## What these tests are for

These tests are designed to cover scenarios that *cannot* be expressed or
reliably validated through web content accessibility tests, including:

* Accessibility trees produced directly by `views::View`
* Focus, selection, and state changes originating from Views
* Widget and window-level accessibility behavior
* Platform accessibility bridges fed by Views-only trees
* Regressions in Views accessibility serialization and event routing

In short: **if the accessibility behavior originates in `ui/views`, it belongs
here.**

---

## How it works

This system intentionally mirrors the dump accessibility test framework used
for web content (see
`content/test/data/accessibility/readme.md`).

The core mechanics are identical:
tests generate a textual dump of accessibility state, which is compared against
checked-in expectation files, with support for filtering, platform-specific
outputs, and automatic rebaselining.

### Compiling the tests
```
autoninja -C out/Debug browser_tests
```
### Running the tests
```
out/Debug/browser_tests --gtest_filter="All/ButtonDumpAccessibilityEventsTest.*"
```
### Updating expectations
```
out/Debug/browser_tests --generate-accessibility-test-expectations \
  --gtest_filter="All/ButtonDumpAccessibilityEventsTest.*"
```

The key differences are:
* This is specifically for **Chromium Views**
* The expectation files should live in `chrome/test/data/accessibility/events`.
* Instead of having one large file where all tests are declared (like `content/browser/accessibility/dump_accessibility_events_browsertest.cc`), individual views are encouraged to have their own browser test file reusing the `DumpAccessibilityEventsViewsTestBase` class.

---

## Test Parameters

Each test runs with multiple parameter combinations:
* **Platform API type**: e.g., `kWinIA2`, `kWinUIA`, `kMac`, `kLinux`
* **ViewsAX state**: enabled or disabled

This ensures coverage of both the current production behavior (ViewsAX disabled)
and the new ViewsAX feature being developed.

---

## Skipping Tests for ViewsAX

Some tests may need to be conditionally skipped based on whether ViewsAX is
enabled or disabled. Use the provided macros instead of using `#<skip>`
in expectation files, which would skip the test for all configurations.

### Skip when ViewsAX is enabled

Use `SKIP_IF_VIEWS_AX_ENABLED()` when a test is not yet compatible with ViewsAX:

```cpp
IN_PROC_BROWSER_TEST_P(MyTest, SomeTest) {
  SKIP_IF_VIEWS_AX_ENABLED();
  // ... rest of test.
}
```

### Skip when ViewsAX is disabled

Use `SKIP_IF_VIEWS_AX_DISABLED()` when a test only applies to ViewsAX behavior:

```cpp
IN_PROC_BROWSER_TEST_P(MyTest, ViewsAXOnlyTest) {
  SKIP_IF_VIEWS_AX_DISABLED();
  // ... rest of test.
}
```

---

## Creating a New Test

1. Create a new browsertest file in the appropriate directory for the view
   being tested (e.g., `chrome/browser/ui/views/controls/button/` for button
   tests).

2. Inherit from `DumpAccessibilityEventsViewsTestBase`:

```cpp
class MyViewDumpAccessibilityEventsBrowserTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  void SetUpTestViews() override {
    // Set up your view hierarchy here.
  }
};
```

3. Write your test:

```cpp
IN_PROC_BROWSER_TEST_P(MyViewDumpAccessibilityEventsBrowserTest, MyThing) {
  // BEGIN_RECORDING_EVENTS_OR_SKIP checks for an expectation file on the
  // current platform. If none exists, the test is skipped. Otherwise, it
  // creates an EventRecordingSession that starts recording platform
  // accessibility events. Filters must be configured before this call.
  BEGIN_RECORDING_EVENTS_OR_SKIP("my-thing");

  // Perform the action that triggers accessibility events.
  my_view_->DoSomething();

  // Validation runs automatically when the EventRecordingSession goes out
  // of scope (end of the test body). Events are compared against the
  // expectation file and any mismatches are reported as test failures.
}
```

To capture only a specific portion of the test, call `StopAndCompare()`
on the session object:

```cpp
IN_PROC_BROWSER_TEST_P(MyViewDumpAccessibilityEventsBrowserTest, Partial) {
  DoSomeSetup();  // Not captured.

  BEGIN_RECORDING_EVENTS_OR_SKIP("my-partial-test");
  my_view_->DoSomething();  // Captured.
  event_recording_session_.StopAndCompare();

  DoSomeCleanup();  // Not captured, comparison already done.
}
```

Alternatively, use a scope block to control the session lifetime:

```cpp
IN_PROC_BROWSER_TEST_P(MyViewDumpAccessibilityEventsBrowserTest, Scoped) {
  DoSomeSetup();  // Not captured.

  {
    BEGIN_RECORDING_EVENTS_OR_SKIP("my-scoped-test");
    my_view_->DoSomething();  // Captured.
  }  // Session destroyed here, comparison runs automatically.

  DoSomeCleanup();  // Not captured.
}
```

4. Add the `INSTANTIATE_TEST_SUITE_P` macro:

```cpp
INSTANTIATE_TEST_SUITE_P(
    All,
    MyViewDumpAccessibilityEventsBrowserTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());
```

5. Create expectation files in `chrome/test/data/accessibility/events/`:
   * `my-thing-expected-win.txt` (Windows IA2)
   * `my-thing-expected-uia-win.txt` (Windows UIA)
   * `my-thing-expected-mac.txt` (macOS)
   * `my-thing-expected-auralinux.txt` (Linux)

   You only need to create files for the platforms you want the test to run on.
   On platforms without an expectation file, the test is automatically skipped.

6. Add your test file to `chrome/test/BUILD.gn` in the appropriate section.