// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_piece.h"
#include "base/test/rectify_callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test_internal.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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

namespace views {
class ViewsDelegate;
}

class Browser;

// Provides interactive test functionality for Views.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see README.md for more information).
//
// This class is not a test fixture; your test fixture can inherit from it to
// import all of the test API it provides. You will need to call
// private_test_impl().DoTestSetUp() in your SetUp() method and
// private_test_impl().DoTestTearDown() in your TearDown() method and you must
// call SetContextWidget() before running your test sequence. For this reason,
// we provide a convenience class, InteractiveBrowserTest, below, which is
// pre-configured to handle all of this for you.
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

  // Takes a screenshot of the specified element, with name `screenshot_name`
  // (may be empty for tests that take only one screenshot) and `baseline`,
  // which should be set to match the CL number when a screenshot should change.
  //
  // Currently, is somewhat unreliable for WebUI embedded in bubbles or dialogs
  // (e.g. Tab Search dropdown) but should work fairly well in most other cases.
  [[nodiscard]] StepBuilder Screenshot(ElementSpecifier element,
                                       const std::string& screenshot_name,
                                       const std::string& baseline);

  struct CurrentBrowser {};
  struct AnyBrowser {};

  // Specifies which browser to use when instrumenting a tab.
  using BrowserSpecifier = absl::variant<
      // Use the browser associated with the context of the current test step;
      // if unspecified use the default context for the sequence.
      CurrentBrowser,
      // Find a tab in any browser.
      AnyBrowser,
      // Specify a browser that is known at the time the sequence is created.
      // The browser must persist until the step executes.
      Browser*,
      // Specify a browser that will be valid by the time the step executes
      // (i.e is set in a previous step callback) but not at the time the test
      // sequence is built. The browser will be read from the target variable,
      // which must point to a valid browser.
      Browser**>;

  // Instruments tab `tab_index` in `in_browser` as `id`. If `tab_index` is
  // unspecified, the active tab is used.
  //
  // Does not support AnyBrowser; you must specify a browser.
  //
  // If `wait_for_ready` is true (default), the step will not complete until the
  // current page in the WebContents is fully loaded.
  [[nodiscard]] MultiStep InstrumentTab(
      ui::ElementIdentifier id,
      absl::optional<int> tab_index = absl::nullopt,
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
      absl::optional<int> tab_index = absl::nullopt,
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
      absl::optional<GURL> expected_url = absl::nullopt);
  [[nodiscard]] static StepBuilder WaitForWebContentsNavigation(
      ui::ElementIdentifier webcontents_id,
      absl::optional<GURL> expected_url = absl::nullopt);

  // This convenience method navigates the page at `webcontents_id` to
  // `new_url`, which must be different than its current URL. The sequence will
  // not proceed until navigation completes, and will fail if the wrong URL is
  // loaded.
  [[nodiscard]] static MultiStep NavigateWebContents(
      ui::ElementIdentifier webcontents_id,
      GURL new_url);

  // Waits for the given `state_change` in `webcontents_id`. The sequence will
  // fail if the change times out, unless `expect_timeout` is true, in which
  // case the StateChange *must* timeout, and |state_change.timeout_event| must
  // be set.
  [[nodiscard]] static MultiStep WaitForStateChange(
      ui::ElementIdentifier webcontents_id,
      StateChange state_change,
      bool expect_timeout = false);

  // Required to keep from hiding inherited versions of these methods.
  using InteractiveViewsTestApi::EnsureNotPresent;
  using InteractiveViewsTestApi::EnsurePresent;

  // Ensures that there is an element at path `where` in `webcontents_id`.
  // Unlike InteractiveTestApi::EnsurePresent, this verb can be inside an
  // InAnyContext() block.
  [[nodiscard]] static StepBuilder EnsurePresent(
      ui::ElementIdentifier webcontents_id,
      DeepQuery where);

  // Ensures that there is no element at path `where` in `webcontents_id`.
  // Unlike InteractiveTestApi::EnsurePresent, this verb can be inside an
  // InAnyContext() block.
  [[nodiscard]] static StepBuilder EnsureNotPresent(
      ui::ElementIdentifier webcontents_id,
      DeepQuery where);

  // Execute javascript `function`, which should take no arguments, in
  // WebContents `webcontents_id`.
  [[nodiscard]] static StepBuilder ExecuteJs(
      ui::ElementIdentifier webcontents_id,
      const std::string& function);

  // Execute javascript `function`, which should take a single DOM element as an
  // argument, with the element at `where`, in WebContents `webcontents_id`.
  [[nodiscard]] static StepBuilder ExecuteJsAt(
      ui::ElementIdentifier webcontents_id,
      DeepQuery where,
      const std::string& function);

  // Executes javascript `function`, which should take no arguments and return a
  // value, in WebContents `webcontents_id`, and fails if the result is not
  // truthy.
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
  template <typename T>
  [[nodiscard]] static StepBuilder CheckJsResult(
      ui::ElementIdentifier webcontents_id,
      const std::string& function,
      T&& matcher);

  // Executes javascript `function`, which should take a single DOM element as
  // an argument and returns a value, in WebContents `webcontents_id` on the
  // element specified by `where`, and fails if the result is not truthy.
  [[nodiscard]] static StepBuilder CheckJsResultAt(
      ui::ElementIdentifier webcontents_id,
      DeepQuery where,
      const std::string& function);

  // Executes javascript `function`, which should take a single DOM element as
  // an argument and returns a value, in WebContents `webcontents_id` on the
  // element specified by `where`, and fails if the result does not match
  // `matcher`, which can be a literal or a testing::Matcher.
  //
  // See notes on CheckJsResult() for what values and Matchers are supported.
  template <typename T>
  [[nodiscard]] static StepBuilder CheckJsResultAt(
      ui::ElementIdentifier webcontents_id,
      DeepQuery where,
      const std::string& function,
      T&& matcher);

  // These are required so the following overloads don't hide the base class
  // variations.
  using InteractiveViewsTestApi::DragMouseTo;
  using InteractiveViewsTestApi::MoveMouseTo;

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Move the mouse to
  // the element's center point in screen coordinates.
  [[nodiscard]] StepBuilder MoveMouseTo(ElementSpecifier web_contents,
                                        DeepQuery where);

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Perform a drag
  // from the mouse's current location to the element's center point in screen
  // coordinates, and then if `release` is true, releases the mouse button.
  [[nodiscard]] StepBuilder DragMouseTo(ElementSpecifier web_contents,
                                        DeepQuery where,
                                        bool release = true);

 protected:
  explicit InteractiveBrowserTestApi(
      std::unique_ptr<internal::InteractiveBrowserTestPrivate>
          private_test_impl);

 private:
  static RelativePositionCallback DeepQueryToRelativePosition(DeepQuery query);

  Browser* GetBrowserFor(ui::ElementContext current_context,
                         BrowserSpecifier spec);

  internal::InteractiveBrowserTestPrivate& test_impl() {
    return static_cast<internal::InteractiveBrowserTestPrivate&>(
        private_test_impl());
  }
};

// Test fixture for browser tests that supports the InteractiveBrowserTestApi
// convenience methods.
//
// All things being equal, if you want to write an interactive browser test,
// you should probably alias or derive from this class.
//
// See README.md for usage.
class InteractiveBrowserTest : public InProcessBrowserTest,
                               public InteractiveBrowserTestApi {
 public:
  InteractiveBrowserTest();
  ~InteractiveBrowserTest() override;

  // |views_delegate| is used for tests that want to use a derived class of
  // ViewsDelegate to observe or modify things like window placement and Widget
  // params.
  explicit InteractiveBrowserTest(
      std::unique_ptr<views::ViewsDelegate> views_delegate);

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
};

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
    DeepQuery where,
    const std::string& function,
    T&& matcher) {
  return internal::JsResultChecker<T>::CheckJsResultAt(
      webcontents_id, where, function, std::move(matcher));
}

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
