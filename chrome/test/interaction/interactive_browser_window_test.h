// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_WINDOW_TEST_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_WINDOW_TEST_H_

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/rectify_callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test_internal.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/interactive_test_definitions.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

// Provides interactive test functionality for desktop browsers.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see README.md for more information).
//
// This class is not a test fixture; it is a mixin that can be added to an
// existing browser test class using `InteractiveBrowserTestT<T>` - or just use
// `InteractiveBrowserTest`, which *is* a test fixture (preferred; see below).
class InteractiveBrowserWindowTestApi
    : virtual public ui::test::InteractiveTestApi {
 public:
  InteractiveBrowserWindowTestApi();
  ~InteractiveBrowserWindowTestApi() override;

  using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
  using StateChange = WebContentsInteractionTestUtil::StateChange;

  // Since we enforce a 1:1 correspondence between ElementIdentifiers and
  // WebContents defaulting to ContextMode::kAny prevents accidentally missing
  // the correct context, which is a common mistake that causes tests to
  // mysteriously time out looking in the wrong place.
  static constexpr ui::InteractionSequence::ContextMode
      kDefaultWebContentsContextMode =
          ui::InteractionSequence::ContextMode::kAny;

  // Shorthand to convert a tracked element into a instrumented WebContents.
  // The element should be a TrackedElementWebContents.
  static WebContentsInteractionTestUtil* AsInstrumentedWebContents(
      ui::TrackedElement* el);

  // Manually enable WebUI code coverage (slightly experimental). Call during
  // `SetUpOnMainThread()` or in your test body before `RunTestSequence()`.
  //
  // Has no effect if the `--devtools-code-coverage` command line flag isn't
  // set. This will cause tests to run longer (possibly timing out) and is not
  // compatible with all WebUI pages. Use liberally, but at your own risk.
  //
  // TODO(https://crbug.com/273545898, https://crbug.com/273290598): when
  // coverage is more robust, make this automatic for all tests that touch
  // WebUI.
  void EnableWebUICodeCoverage();

  // Takes a screenshot of the specified element, with name `screenshot_name`
  // (may be empty for tests that take only one screenshot) and `baseline_cl`,
  // which should be set to match the CL number when a screenshot should change.
  //
  // If `clip_rect` is specified, it is the rectangle in `element`'s local
  // bounds to capture, otherwise all of `element` will be captured.
  //
  // Currently, is somewhat unreliable for WebUI embedded in bubbles or dialogs
  // (e.g. Tab Search dropdown) but should work fairly well in most other cases.
  template <typename T = std::optional<gfx::Rect>>
  [[nodiscard]] MultiStep Screenshot(ElementSpecifier element,
                                     const std::string& screenshot_name,
                                     const std::string& baseline_cl,
                                     T&& clip_rect = std::nullopt);

  // Takes a screenshot of a specific element `where` inside a WebContents
  // `webcontents_id`. See `Screenshot()` for more information.
  [[nodiscard]] MultiStep ScreenshotWebUi(ElementSpecifier element,
                                          const DeepQuery& where,
                                          const std::string& screenshot_name,
                                          const std::string& baseline_cl);

  // As `Screenshot()` but takes a screenshot of the entire surface (widget,
  // WebUI, etc.) containing `element_in_surface`. See `Screenshot()` for more
  // information.
  [[nodiscard]] MultiStep ScreenshotSurface(ElementSpecifier element_in_surface,
                                            const std::string& screenshot_name,
                                            const std::string& baseline_cl);

  struct CurrentBrowser {};
  struct AnyBrowser {};

  // Specifies which browser to use when instrumenting a tab.
  using BrowserSpecifier = std::variant<
      // Use the browser associated with the context of the current test step;
      // if unspecified use the default context for the sequence.
      CurrentBrowser,
      // Find a tab in any browser.
      AnyBrowser,
      // Specify a browser that is known at the time the sequence is created.
      // The browser must persist until the step executes.
      BrowserWindowInterface*,
      // Specify a browser pointer that will be valid by the time the step
      // executes. Use std::ref() to wrap the pointer that will receive the
      // value.
      std::reference_wrapper<BrowserWindowInterface*>>;

  // Instruments tab `tab_index` in `in_browser` as `id`. If `tab_index` is
  // unspecified, the active tab is used.
  //
  // Does not support AnyBrowser; you must specify a browser.
  //
  // If `wait_for_ready` is true (default), the step will not complete until the
  // current page in the WebContents is fully loaded.
  [[nodiscard]] MultiStep InstrumentTab(
      ui::ElementIdentifier id,
      std::optional<int> tab_index = std::nullopt,
      BrowserSpecifier in_browser = CurrentBrowser(),
      bool wait_for_ready = true);

  // Instruments the next tab in `in_browser` as `id`.
  [[nodiscard]] StepBuilder InstrumentNextTab(
      ui::ElementIdentifier id,
      BrowserSpecifier in_browser = CurrentBrowser());

  // Opens a new tab for `url` and instruments it as `id`. The tab is inserted
  // at `at_index` if specified, otherwise the browser decides.
  //
  // Does not support AnyBrowser; you must specify a browser.
  [[nodiscard]] MultiStep AddInstrumentedTab(
      ui::ElementIdentifier id,
      GURL url,
      std::optional<int> tab_index = std::nullopt,
      BrowserSpecifier in_browser = CurrentBrowser());

  // Instruments an inner webview of an already-instrumented WebContents of any
  // type; the resulting element will be present only when the parent element is
  // *and* the inner webview is loaded and ready.
  //
  // Do not use with iframe; just instrument the primary contents and use
  // `DeepQuery` instead.
  [[nodiscard]] MultiStep InstrumentInnerWebContents(
      ui::ElementIdentifier inner_id,
      ui::ElementIdentifier outer_id,
      size_t inner_contents_index,
      bool wait_for_ready = true);

  // Removes instrumentation for the WebContents with identifier `id`.
  // `fail_if_not_instrumented` defines what happens if `id` is not in use;
  // if true, crashes the test. If false, ignores and continues.
  [[nodiscard]] StepBuilder UninstrumentWebContents(
      ui::ElementIdentifier id,
      bool fail_if_not_instrumented = true);

  // These convenience methods wait for page navigation/ready. If you specify
  // `expected_url`, the test will fail if that is not the loaded page. If you
  // do not, there is no step start callback and you can add your own logic.
  //
  // Note that because `webcontents_id` is expected to be globally unique, these
  // actions have SetFindElementInAnyContext(true) by default (otherwise it's
  // really easy to forget to add InAnyContext() and have your test not work.
  [[nodiscard]] static StepBuilder WaitForWebContentsReady(
      ui::ElementIdentifier webcontents_id,
      std::optional<GURL> expected_url = std::nullopt);
  [[nodiscard]] static StepBuilder WaitForWebContentsNavigation(
      ui::ElementIdentifier webcontents_id,
      std::optional<GURL> expected_url = std::nullopt);

  // Waits for the instrumented WebContents with the given `webcontents_id` to
  // be painted at least once. If a WebContents is not painted when some events
  // are being sent, they may not be routed correctly. Likewise, a screenshot of
  // a WebContents won't be guaranteed to have any content without being painted
  // first. Because paint often happens quickly, this can lead to tests that
  // mostly pass but cause flakes due to hidden race conditions on slower
  // machines and test-bots.
  //
  // Note: waiting for contentful paint isn't automatic when instrumenting,
  // specifically because not all pages actually paint - empty pages, background
  // pages, and pages loaded in WebViews that are hidden or zero size do not
  // paint. They are also not required for all interactions. Where possible,
  // verbs provided in this API that do require at least one paint will include
  // this verb conditionally to avoid problems.
  [[nodiscard]] static StepBuilder WaitForWebContentsPainted(
      ui::ElementIdentifier webcontents_id);

  // This convenience method navigates the page at `webcontents_id` to
  // `new_url`, which must be different than its current URL. The sequence will
  // not proceed until navigation completes, and will fail if the wrong URL is
  // loaded.
  [[nodiscard]] static MultiStep NavigateWebContents(
      ui::ElementIdentifier webcontents_id,
      GURL new_url);

  // Raises the surface containing `webcontents_id` and focuses the WebContents
  // as if a user had interacted directly with it. This is useful if you want
  // the WebContents to e.g. respond to accelerators.
  //
  // Note that this is shorthand for:
  //  - WaitForWebContentsPainted(webcontents_id)
  //  - ActivateSurface(webcontents_id)
  //  - FocusElement(webcontents_id)
  //
  // The last is there to prevent any input from being swallowed before it is
  // sent to the contents.
  [[nodiscard]] MultiStep FocusWebContents(
      ui::ElementIdentifier webcontents_id);

  // Waits for the given `state_change` in `webcontents_id`. The sequence will
  // fail if the change times out, unless `expect_timeout` is true, in which
  // case the StateChange *must* timeout, and |state_change.timeout_event| must
  // be set.
  //
  // Generally, you are better off using WaitForJsResult[At] instead of a raw
  // WaitForStateChange.
  [[nodiscard]] static MultiStep WaitForStateChange(
      ui::ElementIdentifier webcontents_id,
      const StateChange& state_change,
      bool expect_timeout = false);

  // Ensures that there is an element at path `where` in `webcontents_id`.
  // Unlike InteractiveTestApi::EnsurePresent, this verb can be inside an
  // InAnyContext() block.
  [[nodiscard]] static StepBuilder EnsurePresent(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where);

  // Ensures that there is no element at path `where` in `webcontents_id`.
  // Unlike InteractiveTestApi::EnsurePresent, this verb can be inside an
  // InAnyContext() block.
  [[nodiscard]] static StepBuilder EnsureNotPresent(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where);

  // Similar to EnsureNotPresent, but succeeds if the element is either not
  // present, or present and not visible.
  [[nodiscard]] static StepBuilder EnsureNotVisible(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where);

  // How to execute JavaScript when calling ExecuteJs() and ExecuteJsAt().
  enum class ExecuteJsMode {
    // Ensures that the code sent to the renderer completes without error before
    // the next step can proceed. If an error occurs, the test fails.
    //
    // This is the default.
    kWaitForCompletion,
    // Sends the code to the renderer for execution, but does not wait for a
    // response. If an error occurs, it may appear in the log, but the test
    // will not detect it and will not fail.
    //
    // Use this mode if the code you are injecting will prevent the renderer
    // from communicating the result back to the browser process.
    kFireAndForget,
  };

  // Execute javascript `function`, which should take no arguments, in
  // WebContents `webcontents_id`.
  //
  // You can use this method to call an existing function with no arguments in
  // the global scope; to do that, specify only the name of the method (e.g.
  // `myMethod` rather than `myMethod()`).
  [[nodiscard]] static StepBuilder ExecuteJs(
      ui::ElementIdentifier webcontents_id,
      const std::string& function,
      ExecuteJsMode mode = ExecuteJsMode::kWaitForCompletion);

  // Execute javascript `function`, which should take a single DOM element as an
  // argument, with the element at `where`, in WebContents `webcontents_id`.
  [[nodiscard]] static StepBuilder ExecuteJsAt(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where,
      const std::string& function,
      ExecuteJsMode mode = ExecuteJsMode::kWaitForCompletion);

  // Returns a matcher that matches truthy values.
  //
  // Use this if you don't want to compare specifically to "true", but just want
  // to know that a value isn't null/false/empty/zero.
  [[nodiscard]] static auto IsTruthy() { return internal::IsTruthyMatcher(); }

  // Executes javascript `function`, which should take no arguments and return a
  // value, in WebContents `webcontents_id`, and fails if the result is not
  // truthy.
  //
  // If `function` instead returns a promise, the result of the promise is
  // evaluated for truthiness. If the promise rejects, CheckJsResult() fails.
  [[nodiscard]] static StepBuilder CheckJsResult(
      ui::ElementIdentifier webcontents_id,
      const std::string& function);

  // Executes javascript `function`, which should take no arguments and return a
  // value, in WebContents `webcontents_id`, and fails if the result does not
  // match `matcher`, which can be a literal or a testing::Matcher.
  //
  // Note that only the following types are supported:
  //  - string (for literals, you may pass a const char*)
  //  - bool
  //  - int
  //  - double (will also match integer return values)
  //  - base::Value (required if you want to match a list or dictionary)
  //
  // You must pass a literal or Matcher that matches the type returned by the
  // javascript function. If your function could return either an integer or a
  // floating-point value, you *must* use a double.
  //
  // If `function` instead returns a promise, the result of the promise is
  // evaluated against `matcher`. If the promise rejects, CheckJsResult() fails.
  template <typename T>
  [[nodiscard]] static StepBuilder CheckJsResult(
      ui::ElementIdentifier webcontents_id,
      const std::string& function,
      T&& matcher);

  // Executes javascript `function`, which should take a single DOM element as
  // an argument and returns a value, in WebContents `webcontents_id` on the
  // element specified by `where`, and fails if the result is not truthy.
  //
  // If `function` instead returns a promise, the result of the promise is
  // evaluated for truthiness. If the promise rejects, CheckJsResultAt() fails.
  [[nodiscard]] static StepBuilder CheckJsResultAt(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where,
      const std::string& function);

  // Executes javascript `function`, which should take a single DOM element as
  // an argument and returns a value, in WebContents `webcontents_id` on the
  // element specified by `where`, and fails if the result does not match
  // `matcher`, which can be a literal or a testing::Matcher.
  //
  // If `function` instead returns a promise, the result of the promise is
  // evaluated against `matcher`. If the promise rejects, CheckJsResultAt()
  // fails.
  //
  // See notes on CheckJsResult() for what values and Matchers are supported.
  template <typename T>
  [[nodiscard]] static StepBuilder CheckJsResultAt(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where,
      const std::string& function,
      T&& matcher);

  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kDefaultWaitForJsResultEvent);
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kDefaultWaitForJsResultAtEvent);

  // Polls `webcontents_id` until the result of `function` matches `value`.
  //
  // It is not necessary to specify `event` unless you are waiting for results
  // in parallel (then each event must be unique).
  template <typename M>
  [[nodiscard]] MultiStep WaitForJsResult(
      ui::ElementIdentifier webcontents_id,
      const std::string& function,
      M&& value,
      bool continue_across_navigation = false,
      ui::CustomElementEventType event = kDefaultWaitForJsResultEvent);

  // Polls element at `where` in `webcontents_id` until the element exists and
  // the result of calling `function` on it matches `value`.
  //
  // It is not necessary to specify `event` unless you are waiting for results
  // in parallel (then each event must be unique).
  template <typename M>
  [[nodiscard]] MultiStep WaitForJsResultAt(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where,
      const std::string& function,
      M&& value,
      bool element_must_be_present_at_start = false,
      bool continue_across_navigation = false,
      ui::CustomElementEventType event = kDefaultWaitForJsResultAtEvent);

  // Polls `webcontents_id` until the result of `function` is truthy.
  //
  // Equivalent to `WaitForJsResult(webcontents_id, function, IsTruthy())`.
  [[nodiscard]] MultiStep WaitForJsResult(ui::ElementIdentifier webcontents_id,
                                          const std::string& function);

  // Polls element at `where` in `webcontents_id` until the element exists and
  // the result of calling `function` on it is truthy.
  //
  // Equivalent to:
  // `WaitForJsResultAt(webcontents_id, where function, IsTruthy())`.
  [[nodiscard]] MultiStep WaitForJsResultAt(
      ui::ElementIdentifier webcontents_id,
      const DeepQuery& where,
      const std::string& function);

  // Scrolls the DOM element at `where` in instrumented WebContents
  // `web_contents` into view; see Instrument*(). The scrolling happens
  // instantaneously, without animation, and should be available on the next
  // render frame or call into the renderer.
  [[nodiscard]] StepBuilder ScrollIntoView(ui::ElementIdentifier web_contents,
                                           const DeepQuery& where);

  // Waits until the intersection of the element's bounds and the window bounds
  // are nonempty.
  [[nodiscard]] MultiStep WaitForElementVisible(
      ui::ElementIdentifier web_contents,
      const DeepQuery& where);

  // Simulates clicking on an HTML element by injecting the click event directly
  // into the DOM. You can specify the mouse button and additional modifier
  // keys (default is left-click, no modifiers).
  //
  // Normally, clicking with buttons other than the left mouse button generates
  // an auxclick event rather than a click event. However, injecting auxclick
  // does not e.g. trigger navigation when clicking a link, so in all these
  // cases, vanilla click events are sent, which should be handled normally for
  // backwards-compatibility reasons.
  //
  // Note that if your WebUI will close in response to this action, you should
  // set `execute_mode` to `kFireAndForget` to avoid failure to receive
  // confirmation.
  [[nodiscard]] StepBuilder ClickElement(
      ui::ElementIdentifier web_contents,
      const DeepQuery& where,
      ui_controls::MouseButton button = ui_controls::LEFT,
      ui_controls::AcceleratorState modifiers = ui_controls::kNoAccelerator,
      ExecuteJsMode execute_mode = ExecuteJsMode::kWaitForCompletion);

  // Convenience version of `ClickElement()` (see above) for
  // default-left-clicking when you need to specify the execution mode.
  [[nodiscard]] inline StepBuilder ClickElement(
      ui::ElementIdentifier web_contents,
      const DeepQuery& where,
      ExecuteJsMode execute_mode) {
    return ClickElement(web_contents, where, ui_controls::LEFT,
                        ui_controls::kNoAccelerator, execute_mode);
  }

 protected:
  // Specifies how the `reference_element` should be used (or not) to generate a
  // target point for a mouse move.
  using RelativePositionCallback =
      base::OnceCallback<gfx::Point(ui::TrackedElement* reference_element)>;

  static RelativePositionCallback DeepQueryToRelativePosition(
      const DeepQuery& query);

  internal::InteractiveBrowserTestPrivate& browser_test_impl() {
    return *test_impl_;
  }

 private:
  // Common logic for WaitForJsResult[At].
  template <typename M>
  [[nodiscard]] MultiStep WaitForJsResultCommon(
      ui::ElementIdentifier webcontents_id,
      StateChange::Type type,
      const std::string& function,
      const DeepQuery& where,
      M&& value,
      bool continue_across_navigation,
      ui::CustomElementEventType event);

  // Possibly waits for `element_id` to be painted if it is a WebContents.
  [[nodiscard]] MultiStep MaybeWaitForPaint(ElementSpecifier element);

  // Waits for the user to dismiss `element` if in interactive mode
  // (command-line flag `--test-launcher-interactive`).
  [[nodiscard]] static StepBuilder MaybeWaitForUserToDismiss(
      ElementSpecifier element);

  BrowserWindowInterface* GetBrowserWindowFor(
      ui::ElementContext current_context,
      BrowserSpecifier spec);

  const raw_ptr<internal::InteractiveBrowserTestPrivate> test_impl_;
};

// Template for adding InteractiveBrowserWindowTestApi to any test fixture which
// is derived from InProcessBrowserTest.
//
// If you don't need to derive from some existing test class, prefer to use
// InteractiveBrowserTest.
//
// Note that this test fixture attempts to set the context widget from the
// created `browser()` during `SetUpOnMainThread()`. If your derived test
// fixture does not create a browser during set up, you will need to manually
// `SetContextWidget()` before calling `RunTestSequence()`, or use
// `RunTestTestSequenceInContext()` instead.
//
// See README.md for usage.
template <typename T>
  requires std::derived_from<T, testing::Test>
class InteractiveBrowserWindowTestT : public T,
                                      public InteractiveBrowserWindowTestApi {
 public:
  template <typename... Args>
  explicit InteractiveBrowserWindowTestT(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~InteractiveBrowserWindowTestT() override = default;

 protected:
  virtual BrowserWindowInterface* GetBrowserWindow() = 0;

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    private_test_impl().set_default_context(
        BrowserElements::From(GetBrowserWindow())->GetContext());
  }

  void TearDownOnMainThread() override {
    private_test_impl().DoTestTearDown();
    T::TearDownOnMainThread();
  }
};

// Template definitions:

template <typename T>
InteractiveBrowserWindowTestApi::MultiStep
InteractiveBrowserWindowTestApi::Screenshot(ElementSpecifier element,
                                            const std::string& screenshot_name,
                                            const std::string& baseline_cl,
                                            T&& clip_rect) {
  StepBuilder builder;
  builder.SetDescription("Compare Screenshot");
  builder.SetElement(element);
  builder.SetStartCallback(base::BindOnce(
      [](InteractiveBrowserWindowTestApi* test, std::string screenshot_name,
         std::string baseline_cl, std::remove_cvref_t<T> clip_rect,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        ScreenshotOptions options;
        options.region = ui::test::internal::UnwrapArgument<T>(clip_rect);
        const auto result = InteractionTestUtilBrowser::CompareScreenshot(
            el, screenshot_name, baseline_cl, options);
        test->private_test_impl().HandleActionResult(seq, el, "Screenshot",
                                                     result);
      },
      base::Unretained(this), screenshot_name, baseline_cl,
      std::forward<T>(clip_rect)));

  auto steps = Steps(MaybeWaitForPaint(element), std::move(builder),
                     MaybeWaitForUserToDismiss(element));
  AddDescriptionPrefix(steps, base::StrCat({"Screenshot( \"", screenshot_name,
                                            "\", \"", baseline_cl, "\" )"}));
  return steps;
}

// static
template <typename T>
ui::InteractionSequence::StepBuilder
InteractiveBrowserWindowTestApi::CheckJsResult(
    ui::ElementIdentifier webcontents_id,
    const std::string& function,
    T&& value) {
  return std::move(
      CheckElement(
          webcontents_id,
          [function,
           value = ui::test::internal::MatcherTypeFor<T>(
               std::forward<T>(value))](ui::TrackedElement* el) mutable {
            std::string error_msg;
            base::Value result =
                el->AsA<TrackedElementWebContents>()->owner()->Evaluate(
                    function, &error_msg);
            if (!error_msg.empty()) {
              LOG(ERROR) << "CheckJsResult() failed: " << error_msg;
              return false;
            }

            auto m = internal::MakeValueMatcher(std::move(value));
            return ui::test::internal::MatchAndExplain("CheckJsResult()", m,
                                                       result);
          })
          .SetContext(kDefaultWebContentsContextMode)
          .SetDescription(base::StringPrintf("CheckJsResult(\"\n%s\n\")",
                                             function.c_str())));
}

// static
template <typename T>
ui::InteractionSequence::StepBuilder
InteractiveBrowserWindowTestApi::CheckJsResultAt(
    ui::ElementIdentifier webcontents_id,
    const DeepQuery& where,
    const std::string& function,
    T&& value) {
  return std::move(
      CheckElement(
          webcontents_id,
          [where, function,
           value = ui::test::internal::MatcherTypeFor<T>(
               std::forward<T>(value))](ui::TrackedElement* el) mutable {
            const auto full_function = base::StringPrintf(
                R"(
            (el, err) => {
              if (err) {
                throw err;
              }
              return (%s)(el);
            }
          )",
                function.c_str());
            std::string error_msg;
            base::Value result =
                el->AsA<TrackedElementWebContents>()->owner()->EvaluateAt(
                    where, full_function, &error_msg);
            if (!error_msg.empty()) {
              LOG(ERROR) << "CheckJsResult() failed: " << error_msg;
              return false;
            }

            auto m = internal::MakeValueMatcher(std::move(value));
            return ui::test::internal::MatchAndExplain("CheckJsResultAt()", m,
                                                       result);
          })
          .SetContext(kDefaultWebContentsContextMode)
          .SetDescription(base::StringPrintf(
              "CheckJsResultAt( %s, \"\n%s\n\")",
              internal::InteractiveBrowserTestPrivate::DeepQueryToString(where)
                  .c_str(),
              function.c_str())));
}

template <typename M>
InteractiveBrowserWindowTestApi::MultiStep
InteractiveBrowserWindowTestApi::WaitForJsResultCommon(
    ui::ElementIdentifier webcontents_id,
    StateChange::Type type,
    const std::string& function,
    const DeepQuery& where,
    M&& value,
    bool continue_across_navigation,
    ui::CustomElementEventType event) {
  StateChange change;
  change.type = type;
  change.test_function = function;
  change.event = event;
  change.continue_across_navigation = continue_across_navigation;
  change.where = where;

  auto context = private_test_impl().CreateAdditionalContext();
  auto expected = ui::test::internal::MatcherTypeFor<M>(std::forward<M>(value));
  using X = decltype(expected);
  change.check_callback = base::BindRepeating(
      [](const X& expected, AdditionalContext context,
         const base::Value& actual) {
        auto m = internal::MakeConstValueMatcher(expected);
        std::ostringstream oss;
        oss << "Expected ";
        m.DescribeTo(&oss);
        oss << "; last known value: " << actual;
        context.Set(oss.str());
        return m.Matches(actual);
      },
      std::move(expected), context);

  return Steps(
      WaitForStateChange(webcontents_id, change),
      Do([context]() mutable { context.Clear(); })
          // Preserve the context of the previous step so that
          // `InSameContext()` on subsequent steps remains valid.
          .SetContext(ui::InteractionSequence::ContextMode::kFromPreviousStep));
}

template <typename M>
InteractiveBrowserWindowTestApi::MultiStep
InteractiveBrowserWindowTestApi::WaitForJsResult(
    ui::ElementIdentifier webcontents_id,
    const std::string& function,
    M&& value,
    bool continue_across_navigation,
    ui::CustomElementEventType event) {
  auto steps = WaitForJsResultCommon(
      webcontents_id, StateChange::Type::kConditionTrue, function, DeepQuery(),
      std::forward<M>(value), continue_across_navigation, event);

  std::ostringstream prefix;
  prefix << "WaitForJsResult(" << webcontents_id << ") with function\n"
         << function << "\n";
  AddDescriptionPrefix(steps, prefix.str());
  return steps;
}

template <typename M>
InteractiveBrowserWindowTestApi::MultiStep
InteractiveBrowserWindowTestApi::WaitForJsResultAt(
    ui::ElementIdentifier webcontents_id,
    const DeepQuery& where,
    const std::string& function,
    M&& value,
    bool element_must_be_present_at_start,
    bool continue_across_navigation,
    ui::CustomElementEventType event) {
  auto steps =
      WaitForJsResultCommon(webcontents_id,
                            element_must_be_present_at_start
                                ? StateChange::Type::kConditionTrue
                                : StateChange::Type::kExistsAndConditionTrue,
                            function, where, std::forward<M>(value),
                            continue_across_navigation, event);

  std::ostringstream prefix;
  prefix << "WaitForJsResultAt(" << webcontents_id << ", " << where
         << ") with function\n"
         << function << "\n";
  AddDescriptionPrefix(steps, prefix.str());
  return steps;
}

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_WINDOW_TEST_H_
