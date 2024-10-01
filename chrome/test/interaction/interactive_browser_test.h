// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/rectify_callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
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
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ui {
class TrackedElement;
}

class Browser;

// Provides interactive test functionality for Views.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see README.md for more information).
//
// This class is not a test fixture; it is a mixin that can be added to an
// existing browser test class using `InteractiveBrowserTestT<T>` - or just use
// `InteractiveBrowserTest`, which *is* a test fixture (preferred; see below).
class InteractiveBrowserTestApi : public views::test::InteractiveViewsTestApi {
 public:
  InteractiveBrowserTestApi();
  ~InteractiveBrowserTestApi() override;

  using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
  using StateChange = WebContentsInteractionTestUtil::StateChange;

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
  // TODO(b/273545898, b/273290598): when coverage is more robust, make this
  // automatic for all tests that touch WebUI.
  void EnableWebUICodeCoverage();

  // Takes a screenshot of the specified element, with name `screenshot_name`
  // (may be empty for tests that take only one screenshot) and `baseline_cl`,
  // which should be set to match the CL number when a screenshot should change.
  //
  // Currently, is somewhat unreliable for WebUI embedded in bubbles or dialogs
  // (e.g. Tab Search dropdown) but should work fairly well in most other cases.
  [[nodiscard]] MultiStep Screenshot(ElementSpecifier element,
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
      Browser*,
      // Specify a browser pointer that will be valid by the time the step
      // executes. Use std::ref() to wrap the pointer that will receive the
      // value.
      std::reference_wrapper<Browser*>>;

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

  // Instruments the WebContents held by `web_view` as `id`. Will wait for the
  // WebView to become visible if it is not.
  //
  // If `wait_for_ready` is true (default), the step will not complete until the
  // current page in the WebContents is fully loaded. (Note that this may not
  // cover dynamic loading of data; you may need to do a WaitForStateChange() to
  // be sure dynamic content is loaded).
  [[nodiscard]] MultiStep InstrumentNonTabWebView(ui::ElementIdentifier id,
                                                  ElementSpecifier web_view,
                                                  bool wait_for_ready = true);
  [[nodiscard]] MultiStep InstrumentNonTabWebView(
      ui::ElementIdentifier id,
      AbsoluteViewSpecifier web_view,
      bool wait_for_ready = true);

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
  [[nodiscard]] StepBuilder FocusWebContents(
      ui::ElementIdentifier webcontents_id);

  // Waits for the given `state_change` in `webcontents_id`. The sequence will
  // fail if the change times out, unless `expect_timeout` is true, in which
  // case the StateChange *must* timeout, and |state_change.timeout_event| must
  // be set.
  [[nodiscard]] static MultiStep WaitForStateChange(
      ui::ElementIdentifier webcontents_id,
      const StateChange& state_change,
      bool expect_timeout = false);

  // Required to keep from hiding inherited versions of these methods.
  using InteractiveViewsTestApi::EnsureNotPresent;
  using InteractiveViewsTestApi::EnsurePresent;

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

  // These are required so the following overloads don't hide the base class
  // variations.
  using InteractiveViewsTestApi::DragMouseTo;
  using InteractiveViewsTestApi::MoveMouseTo;

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Move the mouse to
  // the element's center point in screen coordinates.
  //
  // If the DOM element may be scrolled outside of the current viewport,
  // consider using ScrollIntoView(web_contents, where) before this verb.
  [[nodiscard]] MultiStep MoveMouseTo(ui::ElementIdentifier web_contents,
                                      const DeepQuery& where);

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Perform a drag
  // from the mouse's current location to the element's center point in screen
  // coordinates, and then if `release` is true, releases the mouse button.
  //
  // If the DOM element may be scrolled outside of the current viewport,
  // consider using ScrollIntoView(web_contents, where) before this verb.
  [[nodiscard]] MultiStep DragMouseTo(ui::ElementIdentifier web_contents,
                                      const DeepQuery& where,
                                      bool release = true);

  using InteractiveViewsTestApi::ScrollIntoView;

  // Scrolls the DOM element at `where` in instrumented WebContents
  // `web_contents` into view; see Instrument*(). The scrolling happens
  // instantaneously, without animation, and should be available on the next
  // render frame or call into the renderer.
  [[nodiscard]] StepBuilder ScrollIntoView(ui::ElementIdentifier web_contents,
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
  [[nodiscard]] StepBuilder ClickElement(
      ui::ElementIdentifier web_contents,
      const DeepQuery& where,
      ui_controls::MouseButton button = ui_controls::LEFT,
      ui_controls::AcceleratorState modifiers = ui_controls::kNoAccelerator);

 protected:
  explicit InteractiveBrowserTestApi(
      std::unique_ptr<internal::InteractiveBrowserTestPrivate>
          private_test_impl);

 private:
  static RelativePositionCallback DeepQueryToRelativePosition(
      const DeepQuery& query);

  // Possibly waits for `element_id` to be painted if it is a WebContents.
  [[nodiscard]] MultiStep MaybeWaitForPaint(ElementSpecifier element,
                                            const std::string& desc);

  Browser* GetBrowserFor(ui::ElementContext current_context,
                         BrowserSpecifier spec);

  internal::InteractiveBrowserTestPrivate& test_impl() {
    return static_cast<internal::InteractiveBrowserTestPrivate&>(
        private_test_impl());
  }
};

// Template for adding InteractiveBrowserTestApi to any test fixture which is
// derived from InProcessBrowserTest.
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
  requires std::derived_from<T, InProcessBrowserTest>
class InteractiveBrowserTestT : public T, public InteractiveBrowserTestApi {
 public:
  template <typename... Args>
  explicit InteractiveBrowserTestT(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~InteractiveBrowserTestT() override = default;

 protected:
  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    if (Browser* browser = T::browser()) {
      SetContextWidget(
          BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
    }
  }

  void TearDownOnMainThread() override {
    private_test_impl().DoTestTearDown();
    T::TearDownOnMainThread();
  }
};

// Convenience test fixture for interactive browser tests. This is the preferred
// base class for Kombucha tests unless you specifically need something else.
//
// Note that this test fixture attempts to set the context widget from the
// created `browser()` during `SetUpOnMainThread()`. If your derived test
// fixture does not create a browser during set up, you will need to manually
// `SetContextWidget()` before calling `RunTestSequence()`, or use
// `RunTestTestSequenceInContext()` instead.
//
// See README.md for usage.
using InteractiveBrowserTest = InteractiveBrowserTestT<InProcessBrowserTest>;

// Template definitions:

// static
template <typename T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::CheckJsResult(
    ui::ElementIdentifier webcontents_id,
    const std::string& function,
    T&& matcher) {
  return internal::JsResultChecker<T>::CheckJsResult(webcontents_id, function,
                                                     std::move(matcher));
}

// static
template <typename T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::CheckJsResultAt(
    ui::ElementIdentifier webcontents_id,
    const DeepQuery& where,
    const std::string& function,
    T&& matcher) {
  return internal::JsResultChecker<T>::CheckJsResultAt(
      webcontents_id, where, function, std::move(matcher));
}

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
