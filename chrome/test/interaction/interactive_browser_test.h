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
#include "chrome/browser/ui/browser.h"
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

  // Retrieves an instrumented WebContents with identifier `id`, or null if the
  // contents has not been instrumented.
  WebContentsInteractionTestUtil* GetInstrumentedWebContents(
      ui::ElementIdentifier id);

  // Instruments an existing tab in `browser`. If `tab_index` is not specified,
  // the active tab is instrumented.
  WebContentsInteractionTestUtil* InstrumentTab(
      Browser* browser,
      ui::ElementIdentifier id,
      absl::optional<int> tab_index = absl::nullopt);

  // Instruments the next tab to open in `browser`, or if not specified, in any
  // browser.
  WebContentsInteractionTestUtil* InstrumentNextTab(
      absl::optional<Browser*> browser,
      ui::ElementIdentifier id);

  // Instruments a non-tab `web_view`.
  WebContentsInteractionTestUtil* InstrumentNonTabWebView(
      views::WebView* web_view,
      ui::ElementIdentifier id);

  [[nodiscard]] StepBuilder Screenshot(ElementSpecifier element,
                                       const std::string& screenshot_name,
                                       const std::string& baseline);

  // These convenience methods wait for page navigation/ready. If you specify
  // `expected_url`, the test will fail if that is not the loaded page. If you
  // do not, there is no step start callback and you can add your own logic.
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

  // These are required so the following overloads don't hide the base class
  // variations.
  using InteractiveViewsTestApi::DragMouseTo;
  using InteractiveViewsTestApi::MoveMouseTo;

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Move the mouse to
  // the element's center point in screen coordinates.
  [[nodiscard]] MultiStep MoveMouseTo(ElementSpecifier web_contents,
                                      DeepQuery where);

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Perform a drag
  // from the mouse's current location to the element's center point in screen
  // coordinates, and then if `release` is true, releases the mouse button.
  [[nodiscard]] MultiStep DragMouseTo(ElementSpecifier web_contents,
                                      DeepQuery where,
                                      bool release = true);

 protected:
  explicit InteractiveBrowserTestApi(
      std::unique_ptr<internal::InteractiveBrowserTestPrivate>
          private_test_impl);

 private:
  static RelativePositionCallback DeepQueryToRelativePosition(DeepQuery query);

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

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
