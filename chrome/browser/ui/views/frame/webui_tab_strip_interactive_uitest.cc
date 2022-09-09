// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
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
#include "chrome/test/interaction/webui_interaction_test_util.h"
#include "content/public/test/browser_test.h"
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
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/window.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WebUITabStripInteractiveTest : public InProcessBrowserTest {
 public:
  WebUITabStripInteractiveTest() {
    feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~WebUITabStripInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList feature_override_;
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
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

  // IPH may cause a reveal. Stop it.
  auto lock =
      browser_view->GetFeaturePromoController()->BlockPromosForTesting();

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

namespace {

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMouseDragCompleteCustomEvent);

// Ends any drag currently in progress or that starts during this object's
// lifetime. Used to prevent test hangs at the end of a test before TearDown()
// is run because a spurious drag starts. See crbug.com/1352602 for discussion.
class DragEnder : public aura::client::DragDropClientObserver {
 public:
  explicit DragEnder(aura::client::DragDropClient* client) : client_(client) {
    if (client_->IsDragDropInProgress()) {
      PostCancel();
    } else {
      scoped_observation_.Observe(client_);
    }
  }

  ~DragEnder() override = default;

 private:
  void OnDragStarted() override {
    scoped_observation_.Reset();
    PostCancel();
  }

  void PostCancel() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DragEnder::CancelDrag, weak_ptr_factory_.GetWeakPtr()));
  }

  void CancelDrag() { client_->DragCancel(); }

  aura::client::DragDropClient* const client_;
  base::ScopedObservation<aura::client::DragDropClient,
                          aura::client::DragDropClientObserver>
      scoped_observation_{this};
  base::WeakPtrFactory<DragEnder> weak_ptr_factory_{this};
};

}  // namespace

// Test fixture with additional logic for drag/drop.
class WebUITabStripDragInteractiveTest : public WebUITabStripInteractiveTest {
 public:
  WebUITabStripDragInteractiveTest() = default;
  ~WebUITabStripDragInteractiveTest() override = default;

 protected:
  // Performs a drag by sending mouse events.
  //
  // Moves the cursor to `start` and begins a drag to `end` in screen
  // coordinates (but does not release the mouse button). When the mouse reaches
  // `end`, an event is sent.
  //
  // This can probably be turned into a common utility method for testing things
  // that happen in the middle of a drag.
  void PerformDragWithoutRelease(gfx::Point start,
                                 gfx::Point end,
                                 ui::ElementIdentifier target_id) {
    using WeakPtr = base::WeakPtr<WebUITabStripDragInteractiveTest>;
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        start.x(), start.y(),
        base::BindOnce(
            [](WeakPtr test, gfx::Point end, ui::ElementIdentifier target_id) {
              if (!test)
                return;
              ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
                  ui_controls::LEFT, ui_controls::DOWN,
                  base::BindOnce(
                      [](WeakPtr test, gfx::Point end,
                         ui::ElementIdentifier target_id) {
                        if (!test)
                          return;
                        ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
                            end.x(), end.y(),
                            base::BindOnce(&WebUITabStripDragInteractiveTest::
                                               SendCustomEvent,
                                           test, target_id,
                                           kMouseDragCompleteCustomEvent)));
                      },
                      test, end, target_id)));
            },
            weak_ptr_factory_.GetWeakPtr(), end, target_id)));
  }

  void EndPendingDrag() {
    // First, send a mouse-up to end the drag.
    ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP);

    // Second, due to an interaction between the Linux Ash simulator and certain
    // Chrome builds, intermittently, a drag operation can start spuriously
    // after this sequence. Unfortunately, this happens between here and the
    // TearDown() method, which soft-locks the test (see crbug.com/1352602 for
    // discussion). Install an observer to detect if this happens and cancel the
    // drag.
    auto* const drag_client = aura::client::GetDragDropClient(
        browser()->window()->GetNativeWindow()->GetRootWindow());
    drag_ender_ = std::make_unique<DragEnder>(drag_client);
  }

 private:
  // Convenience method to locate and send a custom event of type `event_type`
  // on the element with identifier `id`.
  void SendCustomEvent(ui::ElementIdentifier id,
                       ui::CustomElementEventType event_type) {
    auto* const target =
        ui::ElementTracker::GetElementTracker()->GetUniqueElement(
            id, browser()->window()->GetElementContext());
    ASSERT_NE(nullptr, target);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(target,
                                                                  event_type);
  }

  std::unique_ptr<DragEnder> drag_ender_;
  base::WeakPtrFactory<WebUITabStripDragInteractiveTest> weak_ptr_factory_{
      this};
};

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
IN_PROC_BROWSER_TEST_F(WebUITabStripDragInteractiveTest, CloseTabDuringDrag) {
  // Add a second tab and set up an object to instrument that tab.
  ASSERT_TRUE(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabElementId);
  std::unique_ptr<WebUIInteractionTestUtil> first_tab =
      WebUIInteractionTestUtil::ForExistingTabInBrowser(browser(),
                                                        kFirstTabElementId, 0);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);
  std::unique_ptr<WebUIInteractionTestUtil> second_tab =
      WebUIInteractionTestUtil::ForExistingTabInBrowser(browser(),
                                                        kSecondTabElementId, 1);

  // The WebUI for the tabstrip will be instrumented only after it is guaranteed
  // to have been created.
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUiTabStripElementId);
  std::unique_ptr<WebUIInteractionTestUtil> tab_strip;

  // This is the DeepQuery path to the second tab element in the WebUI tabstrip.
  // If the structure of the WebUI page changes greatly, it may need to be
  // modified to reflect a new page structure.
  const WebUIInteractionTestUtil::DeepQuery kSecondTabQuery{
      "tabstrip-tab-list", "tabstrip-tab + tabstrip-tab"};

  // Some custom events used to advance the test sequence.
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTabPopulatedCustomEvent);

  // These are needed to determine the sequence didn't fail. They're boilerplate
  // and will probably be exchanged in the future for a smarter version of
  // InteractionSequence::RunSynchronouslyForTesting().
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // This object contains the sequence of expected stets in the test.
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetContext(browser()->window()->GetElementContext())
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Wait until the second tab has fully loaded. This is advisable since
          // later the destruction of the tab needs to be observed.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kSecondTabElementId))

          // Click the tab counter button to display the WebUI tabstrip and
          // make sure the tabstrip appears.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kTabCounterButtonElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        const auto test_util = CreateInteractionTestUtil();
                        test_util->PressButton(element);

                        // The WebUI tabstrip can be created dynamically, so
                        // wait until the button is pressed and the browser is
                        // re-laid-out to bind the associated WebUI.
                        auto* const browser_view =
                            BrowserView::GetBrowserViewForBrowser(browser());
                        browser_view->GetWidget()->LayoutRootViewIfNecessary();
                        auto* const web_view = browser_view->webui_tab_strip()
                                                   ->web_view_for_testing();
                        tab_strip = WebUIInteractionTestUtil::ForNonTabWebView(
                            web_view, kWebUiTabStripElementId);
                      })))

          // Wait for the WebUI tabstrip to become fully loaded, and then wait
          // for the tab data to load and render.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUiTabStripElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        // At this point the new tab has been fully loaded and
                        // its onLoad() called.
                        EXPECT_EQ(2, browser()->tab_strip_model()->count());

                        // It takes a while for tab data to be filled out in the
                        // tabstrip. Before it is fully loaded the tabs have
                        // zero visible size, so wait until they are the
                        // expected size.
                        WebUIInteractionTestUtil::StateChange change;
                        change.event = kTabPopulatedCustomEvent;
                        change.where = kSecondTabQuery;
                        change.type = WebUIInteractionTestUtil::StateChange::
                            Type::kExistsAndConditionTrue;
                        change.test_function =
                            "el => (el.getBoundingClientRect().width > 0)";
                        tab_strip->SendEventOnStateChange(std::move(change));
                      })))

          // Now that the tab is properly rendered, drag it out of the tabstrip.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kTabPopulatedCustomEvent)
                       .SetElementID(kWebUiTabStripElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             // Starting point of drag is the center of the
                             // second tab in the WebUI tabstrip.
                             const gfx::Point start =
                                 tab_strip
                                     ->GetElementBoundsInScreen(kSecondTabQuery)
                                     .CenterPoint();

                             // Endpoint is center of the main webcontents, so
                             // guaranteed to be outside the tabstrip.
                             const gfx::Point end = browser()
                                                        ->tab_strip_model()
                                                        ->GetActiveWebContents()
                                                        ->GetContainerBounds()
                                                        .CenterPoint();

                             // Perform but do not complete the drag.
                             PerformDragWithoutRelease(start, end,
                                                       kWebUiTabStripElementId);
                           })))

          // Wait for the drag to finish and close the tab without releasing the
          // mouse and actually ending the drag.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kMouseDragCompleteCustomEvent)
                       .SetElementID(kWebUiTabStripElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             LOG(WARNING) << "Drag test: mouse move completed.";
                             // For WebUI tab drag, the tab isn't actually
                             // removed from the tabstrip until the drag
                             // completes.
                             EXPECT_EQ(2,
                                       browser()->tab_strip_model()->count());

                             // Close the new tab.
                             second_tab->Execute("() => window.close()");
                             LOG(WARNING)
                                 << "Drag test: waiting for window to close.";
                           })))

          // Wait for the dragged tab to be closed, verify it is closed, and
          // release the mouse to finish the drag.
          //
          // SetTransitionOnlyOnEvent(true) means the test will fail if the tab
          // goes away before this step is queued; it will only succeed if the
          // tab disappears specifically in response to the previous step.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kSecondTabElementId)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             LOG(WARNING)
                                 << "Drag test: window successfully closed.";
                             // The tab should now be removed from the tabstrip
                             // because it was closed; the drag has not yet
                             // finished.
                             EXPECT_EQ(1,
                                       browser()->tab_strip_model()->count());

                             // Be sure to clean up from the drag.
                             EndPendingDrag();
                           })))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
