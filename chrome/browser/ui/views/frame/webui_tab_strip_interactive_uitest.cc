// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/scoped_observation.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/window.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
class WebUITabStripTestHelper {
 public:
  WebUITabStripTestHelper() {
    feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~WebUITabStripTestHelper() = default;

 private:
  base::test::ScopedFeatureList feature_override_;
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
};
}  // namespace

class WebUITabStripInteractiveTest : public InProcessBrowserTest {
 public:
  WebUITabStripInteractiveTest() = default;
  ~WebUITabStripInteractiveTest() override = default;

 private:
  WebUITabStripTestHelper helper_;
};

// Regression test for crbug.com/1027375.
IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest,
                       CanTypeInOmniboxAfterTabStripClose) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  ui_test_utils::FocusView(browser(), VIEW_ID_OMNIBOX);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  OmniboxViewViews* const omnibox =
      browser_view->toolbar()->location_bar()->omnibox_view();
  omnibox->SetUserText(u"");

  container->SetVisibleForTesting(true);
  RunScheduledLayouts();

  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Make sure the tab strip's contents are fully loaded.
  views::WebView* const container_web_view = container->web_view_for_testing();
  ASSERT_TRUE(WaitForLoadStop(container_web_view->GetWebContents()));

  // Click in tab strip then in Omnibox.
  base::RunLoop click_loop_1;
  ui_test_utils::MoveMouseToCenterAndPress(
      container_web_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, click_loop_1.QuitClosure());
  click_loop_1.Run();

  base::RunLoop click_loop_2;
  ui_test_utils::MoveMouseToCenterAndPress(omnibox, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           click_loop_2.QuitClosure());
  click_loop_2.Run();

  // The omnibox should still be focused and should accept keyboard input.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  EXPECT_EQ(u"a", omnibox->GetText());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest,
                       EventInTabContentClosesContainer) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  // Open the tab strip
  container->SetVisibleForTesting(true);
  RunScheduledLayouts();

  base::RunLoop click_loop;
  ui_test_utils::MoveMouseToCenterAndPress(
      browser_view->contents_web_view(), ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, click_loop.QuitClosure());
  click_loop.Run();

  // Make sure it's closed (after the close animation).
  container->FinishAnimationForTesting();
  EXPECT_FALSE(container->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest,
                       EventInContainerDoesNotClose) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  // Open the tab strip
  container->SetVisibleForTesting(true);
  RunScheduledLayouts();

  base::RunLoop click_loop;
  ui_test_utils::MoveMouseToCenterAndPress(container, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           click_loop.QuitClosure());
  click_loop.Run();

  // Make sure it stays open. The FinishAnimationForTesting() call
  // should be a no-op.
  container->FinishAnimationForTesting();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Regression test for crbug.com/1112028
IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest, CanUseInImmersiveMode) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  chromeos::ImmersiveFullscreenControllerTestApi immersive_test_api(
      chromeos::ImmersiveFullscreenController::Get(browser_view->GetWidget()));
  immersive_test_api.SetupForTest();

  ImmersiveModeController* const immersive_mode_controller =
      browser_view->immersive_mode_controller();
  immersive_mode_controller->SetEnabled(true);

  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  EXPECT_FALSE(immersive_mode_controller->IsRevealed());

  // Try opening the tab strip.
  container->SetVisibleForTesting(true);
  RunScheduledLayouts();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());

  // Tapping in the tab strip shouldn't hide the toolbar.
  base::RunLoop click_loop_1;
  ui_test_utils::MoveMouseToCenterAndPress(container, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           click_loop_1.QuitClosure());
  click_loop_1.Run();

  // If the behavior is correct, this call will be a no-op.
  container->FinishAnimationForTesting();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());

  // Interacting with the toolbar should also not close the container.
  base::RunLoop click_loop_2;
  ui_test_utils::MoveMouseToCenterAndPress(
      browser_view->toolbar()->reload_button(), ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, click_loop_2.QuitClosure());
  click_loop_2.Run();

  container->FinishAnimationForTesting();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
}

// Test fixture with additional logic for drag/drop.
class WebUITabStripDragInteractiveTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebUITabStripDragInteractiveTest() = default;
  ~WebUITabStripDragInteractiveTest() override = default;

 private:
  WebUITabStripTestHelper helper_;
};

// Touch mode parameter, only supported by the test framework on Ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         WebUITabStripDragInteractiveTest,
                         testing::Bool());
#else
INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         WebUITabStripDragInteractiveTest,
                         testing::Values(false));
#endif

// Regression test for crbug.com/1286203.
//
// The original bug was a UAF that happened when a tab closed itself (e.g. via
// javascript) during a drag from the WebUI tabstrip; not all references to the
// tab were properly cleaned up.
//
// There is already a proposed regression test for this bug using existing
// technology; see:
//   https://chromium-review.googlesource.com/c/chromium/src/+/3588859
//
// This is a proof-of-concept for regression testing using InteractionSequence,
// which demonstrates that:
//  - tests can be written without arbitrary (and often flaky) delays
//  - tests can be end-to-end interacting with both native and WebUI code
//  - tests can be written to reproduce very specific test cases
//
// This framework can be used to handle many similar types of bugs, for both
// WebUI and Views elements. These tests, while more verbose, can be made very
// specific and are declarative and event-driven. This particular test performs
// the following steps:
//  1. opens a second tab in the browser
//  2. clicks the tab counter button to open the WebUI tabstrip
//  3. drags the second tab out of the WebUI tabstrip
//  4. without finishing the drag, closes the tab via script
//  5. verifies the tab actually closed
//  6. completes the drag
//
// This sequence of events would crash without the associated bugfix. More
// detail is provided in the actual test sequence.

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/40883259): Flaky on linux-chromeos-chrome. Reenable
// this test when the flakiness will be resolved.
#define MAYBE_CloseTabDuringDragDoesNotCrash \
  DISABLED_CloseTabDuringDragDoesNotCrash
#else
#define MAYBE_CloseTabDuringDragDoesNotCrash CloseTabDuringDragDoesNotCrash
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(WebUITabStripDragInteractiveTest,
                       MAYBE_CloseTabDuringDragDoesNotCrash) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUiTabStripElementId);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // This is the DeepQuery path to the second tab element in the WebUI tabstrip.
  // modified to reflect a new page structure.
  const DeepQuery kSecondTabQuery{"tabstrip-tab-list",
                                  "tabstrip-tab + tabstrip-tab"};

  // It takes a while for tab data to be filled out in the tabstrip. Before it
  // is fully loaded the tabs have zero visible size, so wait until they are the
  // expected size.
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTabPopulatedCustomEvent);
  StateChange tab_populated_change;
  tab_populated_change.event = kTabPopulatedCustomEvent;
  tab_populated_change.where = kSecondTabQuery;
  tab_populated_change.type = StateChange::Type::kExistsAndConditionTrue;
  tab_populated_change.test_function =
      "el => (el.getBoundingClientRect().width > 0)";

  // Provide a way to get a reasonable target for a tab drag that is guaranteed
  // to be outside the tabstrip.
  auto get_point_not_in_tabstrip = base::BindLambdaForTesting([browser_view]() {
    return browser_view->contents_web_view()->bounds().CenterPoint();
  });

  // Close a tab from within its own javascript.
  auto close_tab = base::BindRepeating([](ui::TrackedElement* tab) {
    AsInstrumentedWebContents(tab)->Execute("() => window.close()");
  });

  auto get_tab_count = base::BindRepeating(
      [](Browser* browser) { return browser->tab_strip_model()->count(); },
      base::Unretained(browser()));

  auto get_tabstrip_webview =
      base::BindLambdaForTesting([browser_view]() -> views::View* {
        // The WebUI tabstrip can be created dynamically, so wait until the
        // browser is re-laid-out to bind the associated WebUI.
        browser_view->GetWidget()->LayoutRootViewIfNecessary();
        return browser_view->webui_tab_strip()->web_view_for_testing();
      });

  RunTestSequence(
      // Toggle touch mode to send either mouse or touch events.
      Check([this]() { return mouse_util().SetTouchMode(GetParam()); }),
      AddInstrumentedTab(kSecondTabElementId, GURL("about:blank")),
      // Click the counter button and then wait for the WebUI tabstrip to
      // appear.
      PressButton(kToolbarTabCounterButtonElementId),
      InstrumentNonTabWebView(kWebUiTabStripElementId, get_tabstrip_webview),
      // Verify there are two tabs.
      CheckResult(get_tab_count, 2),
      // Wait for the WebUI tabstrip contents to populate.
      WaitForStateChange(kWebUiTabStripElementId, tab_populated_change),
      // Now that the tab is properly rendered, drag it out of the tabstrip.
      MoveMouseTo(kWebUiTabStripElementId, kSecondTabQuery),
      // Drag to the center of the main web contents pane, which should be
      // sufficiently outside the tabstrip. Do not release the drag.
      DragMouseTo(get_point_not_in_tabstrip, /* release =*/false),
      // The tab is not removed from the tabstrip until the drag completes.
      // Verify the count and close the tab.
      CheckResult(get_tab_count, 2),
      WithElement(kSecondTabElementId, close_tab),
      // Wait for the dragged tab to be closed, and verify the tab count is
      // updated.
      //
      // Transition only on event means the test will fail if the tab goes
      // away before this step is queued; it will only succeed if the tab
      // disappears specifically in response to the previous step.
      WaitForHide(kSecondTabElementId, /* transition_only_on_event =*/true),
      CheckResult(get_tab_count, 1),
      // Finish the drag to clean up.
      ReleaseMouse());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
