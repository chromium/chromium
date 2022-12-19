# Interactive Testing API: "Kombucha"

**Kombucha** is a group of powerful test mix-ins that let you easily and
concisely write interactive tests.

The current API version is 1.55. All future 1.x versions are guaranteed to
either be backwards-compatible with existing tests, or the authors will update
the API calls for you.

[TOC]

## Getting Started

There are two ways to write a Kombucha-based interaction test:
1. Alias or inherit from one of our pre-configured test fixtures (preferred):
    - [InteractiveTest](/ui/base/interaction/interactive_test.h)
    - [InteractiveViewsTest](/ui/views/interaction/interactive_views_test.h)
    - [InteractiveBrowserTest](/chrome/test/interaction/interactive_browser_test.h)
2. Have your test fixture inherit the appropriate Kombucha API class:
    - [InteractiveTestApi](/ui/base/interaction/interactive_test.h)
    - [InteractiveViewsTestApi](/ui/views/interaction/interactive_views_test.h)
    - [InteractiveBrowserTestApi](/chrome/test/interaction/interactive_browser_test.h)

If you go the latter route, please see
[Custom Test Fixtures](#custom-test-fixtures) below.

## Using the Kombucha API

***Note:** Throughout this section, unless otherwise specified, all methods are
present in `InteractiveTestApi`. If a method is introduced in
`InteractiveViewsTestApi`, it will have **[Views]** next to it; if it's
introduced in `InteractiveBrowserTestApi`, it will have **[Browser]** next to it
instead.*

### Test Sequences

The primary entry point for any test is `RunTestSequence()` [Views] or
`RunTestSequenceInContext()`. (For more information on `ElementContext`, see the
[Interaction Library Documentation](/ui/base/interaction/README.md).)

`RunTestSequence()` is designed to accept any number of steps. You will use the
provided palette of test verbs and checks that the API provides, or [create your
own verbs](#custom-verbs) through generator methods. The steps are executed in
order until either the final step completes, which is considered success, or a
step fails (or the test times out), which are considered failures.

Example:

```cpp
class MyDialogTest : public InteractiveBrowserTest { ... };

IN_PROC_BROWSER_TEST_F(MyDialogTest, Apply) {
  RunTestSequence(
      // Open my dialog.
      PressButton(kShowMyDialogButtonId),
      WaitForShow(kMyDialogId),
      // Validate the caption of the Apply button.
      CheckViewProperty(kApplyButtonId, &LabelButton::GetText, u"Apply"),
      // Press the button and verify that the state is applied.
      PressButton(kApplyButtonId),
      // This is a custom verb created for this test suite.
      VerifyChangesApplied());
}
```

### Test Verbs

Kombucha *test verbs* are methods that you can call in your test which generate
test steps. Most verbs take an `ElementSpecifier` - either an ID or a name -
describing which UI element the verb should be applied to, but not all do. Some
verbs, like `Check()` and `Do()` don't care about specific elements.

Verbs fall into a number of different categories:
- **Do** performs an action you specify.
- **Check** verbs ensure that some condition is true; if it is not, the test
  fails. Some *Check* verbs use `Matcher`s, some use callbacks, etc. Examples
  include:
    - `Check()`
    - `CheckResult()`
    - `CheckElement()`
    - `CheckView()` [Views]
    - `CheckViewProperty()` [Views]
    - `Screenshot` [Browser] - compares the target against Skia Gold in pixel
      tests. See [Handling Incompatibilities](#handling-incompatibilities) for
      how to handle this in non-pixel tests.
- **WaitFor** verbs ensure that the given UI event happens or condition becomes
  true before proceeding. Examples:
    - `WaitForShow()`
    - `WaitForHide()`
    - `WaitForActivated()`
    - `WaitForEvent()`
- **After** verbs allow you to take some action (specified as a callback) when a
  given event takes place or condition becomes true. The callback can be a full
  `InteractionSequence::StepStartCallback` or it can omit any number of leading
  arguments; try to be as concise as possible. Examples:
    - `AfterShow()`
    - `AfterHide()`
    - `AfterActivated()`
    - `AfterEvent()`
- **With** verbs get the specified element and perform the specified action.
  Unlike the above verbs, they will not wait; the element must exist when the step
  triggers or the test will fail.
    - `WithElement()`
    - `WithView()` [Views]
- **Ensure** verbs check the presence or absence of an element after allowing
  all pending events to settle. They are not compatible with `InAnyContext()`
  for technical reasons, and therefore, take an `in_any_context` parameter.
  There are also versions that look for a DOM element in an
  [instrumented WebContents](#webcontents-instrumentation) [Browser].
    - `EnsurePresent()`
    - `EnsureNotPresent()`
- **Action** verbs simulate input to specific UI elements. You can often specify
  the type of input you want to simulate (keyboard, mouse, etc.) but you don't
  have to. Some of these (`ActivateSurface()`, `SendAccelerator()`) may flake in
  environments where the test fixture is not running as the only process, so
  prefer to use those in interactive_ui_tests. Examples:
    - `PressButton()`
    - `SelectMenuItem()`
    - `SelectTab()`
    - `SelectDropdownItem()`
    - `EnterText()`
    - `SendAccelerator()`
    - `Confirm()`
    - `DoDefaultAction()`
    - `ActivateSurface()`
      - ActivateSurface is not always reliable on Linux with the Wayland window
        manager; see [Handling Incompatibilities](#handling-incompatibilities)
        for how to correctly deal with this.
- **Mouse** verbs simulate mouse input to the entire application, and are
  therefore only reliable in test fixtures that run as exclusive processes (e.g.
  interactive_browser_tests). Examples include:
    - `MoveMouseTo()` [Views]
    - `DragMouseTo()` [Views]
    - `ClickMouse()` [Views]
    - `ReleaseMouseButton()` [Views]
- **Name** verbs assign a string name to some UI element which may not be known
  ahead of time, so that it can be referenced later in the test. Examples
  include:
    - `NameView()` [Views]
    - `NameChildView()` [Views]
    - `NameChildViewByType()` [Views]
    - `NameDescendantView()` [Views]
    - `NameDescendantViewByType()` [Views]
    - `NameViewRelative()` [Views]
- **WebContents** verbs either dynamically
  [instrument WebContents](#webcontents-instrumentation), navigate them, or wait
  for them to navigate or change state.
    - `InstrumentTab()` [Browser]
    - `InstrumentNextTab()` [Browser]
    - `AddInstrumentedTab()` [Browser]
    - `InstrumentNonTabWebView()` [Browser]
    - `NavigateWebContents()` [Browser]
    - `WaitForWebContentsReady()` [Browser]
    - `WaitForWebContentsNavigation()` [Browser]
    - `WaitForStateChange()` [Browser]
- **Javascript** verbs execute javascript in an
  [instrumented WebContents](#webcontents-instrumentation), or verify a result
  from calling a javascript function. The `*At()` methods take a
  [DeepQuery](#specifying-dom-elements) and operate on a specific DOM element
  (possibly in a Shadow DOM), while the non-at methods operate at global scope.
  If you are not sure if the target element exists or the condition is true yet,
  use `WaitForStateChange()` instead. Examples:
   - `ExecuteJs()` [Browser]
   - `ExecuteJsAt()` [Browser]
   - `CheckJsResult()` [Browser]
   - `CheckJsResultAt()` [Browser]
- **Utility** verbs modify how the test sequence is executed.
   - `FlushEvents()` - ensures that the next step happens on a fresh
     message loop rather than being able to chain successive steps.
   - `SetOnIncompatibleAction()` changes what the sequence will do when faced
     with an action that cannot be executed on the current
     build, environment, or platform. See
     [Handling Incompatibilities](#handling-incompatibilities) for more
     information and best practices.

Example with mouse input:
```cpp
// Navigate a page, click Back, and verify that the page navigates back
// correctly.
RunTestSequence(
    NavigateWebContents(kActiveTabId, kTargetUrl),
    MoveMouseTo(kBackButtonId),
    ClickMouse(),
    WaitForWebContentsNavigation(kPreviousUrl));
```

Example with a named element:
```cpp
RunTestSequence(
    // Identify the first child view of the button container.
    NameChildView(kDialogButtons, kFirstButton, 0),
    // Verify that this is the OK button.
    CheckViewProperty(kFirstButton,
                      &LabelButton::GetText,
                      l10n_util::GetStringUTF16(IDS_OK)),
    // Press the button.
    PressButton(kFirstButton));
```

### Modifiers

A modifier wraps around a step or steps and change their behavior.

- **InAnyContext** allows the modified verb to find an element outside the test's default
  `ElementContext`. Unlike the other modifiers, there are a number of limitations on its use:
  - It should not be used with `FlushEvents`, most `Ensure`, or any `Activate`,
    `Event`, or `Mouse` verbs.
    - This is a shortcoming in the underlying framework that will be fixed in the future.
  - It should not be used with named elements, which can already be found in any context.
  - For unsupported verbs, it is best to either use `InSameContext()` or `InContext()` instead.
  - Example:

```cpp
RunTestSequence(
    // This button might be in a different window!
    InAnyContext(PressButton(kMyButton)),
    InAnyContext(CheckView(kMyButton, ensure_pressed)));
```

- **InSameContext** allows the modified verb (or verbs) to find an element in the same context
  as the previous step.
  - Has no effect on `EnsurePresent()` or `EnsureNotPresent()` when the `in_any_context`
    parameter is set to true.
  - Example:
```cpp
RunTestSequence(
    InAnyContext(WaitForShow(kMyButton)),
    InSameContext(PressButton(kMyButton)));
```

- **InContext** allows the modified verb (or verbs) to execute in the specified context instead of
  the default context for the sequence.
  - Has no effect on `EnsurePresent()` or `EnsureNotPresent()` when the `in_any_context` parameter
    is set to true.
  - Example:

```cpp
Browser* const incognito = CreateIncognitoBrowser();
RunTestSequence(
  /* Do stuff in primary browser context here */
  /* ... */
  InContext(incognito->window()->GetElementContext(), Steps(
    PressButton(kAppMenuButton),
    WaitForShow(kDownloadsMenuItemElementId))));
```

### Handling Incompatibilities

Sometimes a test won't run on a specific build bot or in a specific environment
due to a known incompatibility (as opposed to something legitimately failing).
Current known incompatibilities include:
 - `ActivateSurface()` does not work on the `linux-wayland` buildbot unless the
   surface is already active, due to vanilla Wayland not supporting programmatic
   window activation.
 - `Screenshot()` only works in specific pixel test jobs on the `win-rel`
   buildbot.

Normally, if you know that the test won't run on an entire platform (i.e. you
can use `BUILDFLAG()` to differentiate) you should disable or skip the tests in
the usual way. But if the distinction is finer-grained (as with the above verbs)
The `SetOnIncompatibleAction()` verb and `OnIncompatibleAction` enumeration are
provided.
 - `OnIncompatibleAction::kFailTest` is the default option; if a step fails
   because of a known incompatibility, the test will fail, and an error message
   will be printed.
 - `OnIncompatibleAction::kSkipTest` immediately skips the test as soon as an
   incompatibility is detected. Use this option when you know the rest of the
   test will fail and the test results are invalid. A warning will be printed.
 - `OnIncompatibleAction::kHaltTest` immediately halts the sequence but does not
   fail or skip the test. Use this option when all of the steps leading up to
   the incompatible one are valid and you want to preserve any non-fatal errors
   that may have occurred. A warning will still be printed.
 - `OnIncompatibleAction::kIgnoreAndContinue` skips the problematic step, prints
   a warning, and continues the test as if nothing happened. Use this option
   when the step is incidental to the test, such as taking a screenshot in the
   middle of a sequence.

***Do not use `SetOnIncompatibleAction()` unless:***
 1. You know the test will fail due to a known incompatibility.
 2. The test cannot be disabled or skipped using a simple `BUILDFLAG()` check.

Note that you *must* specify a non-empty `reason` when calling
`SetOnIncompatibleAction()` with any argument except `kFailTest`. This string
will be printed out as part of the warning that is produced if the step fails.

### WebContents Instrumentation

A feature of `InteractiveBrowserTestApi` that it borrows from
[WebContentsInteractoinTestUtil](/chrome/test/interaction/webcontents_interaction_test_util.h)
is the ability to *instrument* a `WebContents`. This does the following:
- Assigns the entire `WebContents` a unique `ElementIdentifier`.
- Enables a number of page navigation verbs, such as `NavigateWebContents()`
  and `WaitForWebContentsReady()`.
- Allows the execution of arbitrary JS in the WebContents.
- Allows waiting for a specific condition in the DOM of the `WebContents` via
  `WaitForStateChange()`.

You may call **Instrument** verbs during a test sequence.
- `InstrumentTab()` instruments an existing tab.
- `InstrumentNextTab()` instruments the next tab to be added to or opened in the
  specified browser.
- `AddInstrumentedTab()` adds a new tab to a browser and instruments it.
- `InstrumentNonTabWebContents()` instruments a piece of primary or secondary UI
  that uses a `WebView` and is not a tab (e.g. the tablet tabstrip or Tab Search
  dialog).

#### Specifying DOM Elements

Certain verbs that operate on instrumented WebContents take a `DeepQuery`, which
provides a path to a DOM element in the WebContents. A `DeepQuery` is a sequence
of one or more element selectors, as taken by the JavaScript `querySelector()`
method. A `DeepQuery` works as follows:

```js
let cur = document;
for (let selector of deepQuery) {
  if (cur.shadowRoot)
    cur = cur.shadowRoot;
  cur = cur.querySelector(selector);
}  
```

If at any point the selector fails, the target DOM element is determined not to
exist. Often, this fails the test, but might not in all cases. 

### Automatic Conversion

The following convenience methods are provided to convert a `TrackedElement*` to
a more specific object, primarily used in callbacks supplied to `WithElement()`
or one of the **After** verbs:
- `AsView<T>` - converts the element to a view of the specific type; fails if it
  is not
- `AsInstrumentedWebContents()` - converts the element to an instrumented
  `WebContents`; fails if it is not

Example:
```cpp
  WithElement(kComboBoxId, base::BindOnce([](ui::TrackedElement* el){
    // Note to self: we should probably just have a verb for this.
    AsView<ComboBox>(el)->SelectItem(1);
  })),
```

### Custom Verbs

Sometimes you will have some common step or check (or set of steps and checks)
that you want to duplicate across a number of different test cases in your test
fixture. You can create a custom verb, which is just a method that returns a
`StepBuilder` or `MultiStep`. This method can combine existing verbs with steps
you create yourself, in any combination. To combine multiple steps, use the
`Steps()` method.

Here's an example of a very common custom verb pattern:

```cpp
// My test fixture class with a custom verb.
class MyHistoryTest : public InteractiveBrowserTest {

  // This custom verb will be used across multiple test cases.
  auto OpenHistoryPageInNewTab() {
    return Steps(
        Do(base::BindLambdaForTesting(
            [this](){ InstrumentNextTab(browser(), kHistoryPageTab); }))
        PressButton(kNewTabButton),
        PressButton(kAppMenuButton),
        SelectMenuItem(kHistoryMenuItem),
        SelectMenuItem(kOpenHistoryPageMenuItem),
        WaitForWebContentsNavigation(kHistoryPageTabId,
                                     chrome::kHistoryPageUrl));
  }
};

// An example test case.
IN_PROC_BROWSER_TEST_F(MyHistoryTest, NavigateTwoPagesAndCheckHistory) {
  InstrumentTab(browser(), kPrimaryTabId);
  RunTestSequence(
    WaitForWebContentsReady(kPrimaryTabId),
    NavigateWebContents(kPrimaryTabId, kUrl1),
    NavigateWebContents(kPrimaryTabId, kUrl2),
    // The custom verb sits happily in the action sequence.
    OpenHistoryPageInNewTab(),
    // We'll hand-wave the implementation of this method for now.
    WaitForStateChange(kHistoryPageTabId,
                       HistoryEntriesPopulated(kUrl1, kUrl2)));
}
```

### Custom Callbacks and Checks

Another common pattern is having a check that you want to perform over and over;
for example, checking that a histogram entry was added. This can absolutely be
done through a custom verb, however, perhaps you instead want to use it in an
`AfterShow()` step. In this case you can create a function that binds and
returns the appropriate callback.

```cpp
class MyDialogTest : public InteractiveBrowserTest {
  auto ExpectHistogramCount(const char* histogram_name, size_t expected_count) {
    return base::BindLambdaForTesting([histogram_name, expected_count, this](){
      EXPECT_EQ(expected_count, GetHistogramCount(histogram_name));
    });
  }
};

IN_PROC_BROWSER_TEST_F(MyDialogTest, ShowIncrementsHistogram) {
  RunTestSequence(
    PressButton(kShowMyDialogButtonId),
    AfterShow(kMyDialogContentsId,
              ExpectHistogramCount(kDialogShownHistogramName, 1)));
}
```

This could have also been implemented with a `WaitForShow()` and a custom verb
with a `Check()` or `CheckResult()`. Whether you use a custom callback or a
custom verb is up to you; do whatever makes your test easiest to read!

## Custom Test Fixtures

Most Kombucha tests will derive directly from either `InteractiveViewsTest` or
`InteractiveBrowserTest`.

If your test needs to derive from a different/custom test fixture class but you
would still like access to the Kombucha API, you can have your fixture inherit
directly from one of the *TestApi classes above. (This happens most commonly
when you are adding Kombucha tests to a large library of existing feature
tests.)

You will then need to insert the following calls:
- In `SetUp()` (or `SetUpOnMainThread()` for browser tests), you will need to
  call `private_test_impl().DoTestSetUp()`.
- In `TearDown()` (or `TearDownOnMainThread()` for browser tests), you will
  need to call `private_test_impl().DoTestTearDown()`.

For tests deriving from `InteractiveViewsTestApi` or any of its subclasses, you
will also need to call `SetContextWidget()` sometime before you call
`RunTestSequence()`.

See the implementations of any of the convenience test fixtures listed in
[Getting Started](#getting-started) for examples.

Failure to do the above may cause your test to malfunction, or some test verbs
not to work.

Example:

```cpp
class MyTestFixture
    : public MyCustomBrowserTest,  // descends from InProcessBrowserTest
      public InteractiveBrowserTestApi {
 public:
  void SetUpOnMainThread() override {
    MyCustomTestBase::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    // It's safest to do this here; you can still call
    // RunTestSequenceInContext() if you need a different context (e.g. an
    // incognito browser window).
    SetContextWidget(
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
  }

  void TearDownOnMainThread() override {
    // Optional, but polite:
    SetContextWidget(nullptr);
    private_test_impl().DoTestTearDown();
    MyCustomTestBase::TearDownOnMainThread();
  }
};
```

## Helper Classes

Kombucha helper classes are older, lower-level APIs that have been repurposed
to support interactive testing:
- `InteractionTestUtil`, `InteractionTestUtilView`,
  `InteractionTestUtilBrowser` - provide common UI functionality like pressing
  buttons, selecting menu items, and taking screenshots.
- `InteractionTestUtilMouse` - provides a way to inject mouse input, including
  clicking and dragging, into interactive tests.
- `WebContentsInteractionTestUtil` - provides a way to gain control of a
  WebContents, inject code, trigger and wait for navigation, and check and wait
  for changes in the DOM.

You should only rarely have to use these classes directly; if you do, it's
likely that Kombucha is missing some common verb that would cover your use case.
Please reach out to us!
