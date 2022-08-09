// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interaction_sequence_browser_util.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabSearchPageElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kDownloadsMoreActionsVisibleEventType);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMouseMoveCustomEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMouseDownCustomEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMouseUpCustomEvent);
}  // namespace

class InteractionSequenceBrowserUtilInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  InteractionSequenceBrowserUtilInteractiveUiTest() = default;
  ~InteractionSequenceBrowserUtilInteractiveUiTest() override = default;
  InteractionSequenceBrowserUtilInteractiveUiTest(
      const InteractionSequenceBrowserUtilInteractiveUiTest&) = delete;
  void operator=(const InteractionSequenceBrowserUtilInteractiveUiTest&) =
      delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(
    InteractionSequenceBrowserUtilInteractiveUiTest,
    NavigateMenuAndBringUpDownloadsPageThenOpenMoreActionsMenu) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto download_page = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kPrimaryTabPageElementId);
  auto test_util = CreateInteractionTestUtil();
  const ui::ElementContext context = browser()->window()->GetElementContext();
  const InteractionSequenceBrowserUtil::DeepQuery kButtonQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-icon-button#moreActions"};
  const InteractionSequenceBrowserUtil::DeepQuery kDialogQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-action-menu#moreActionsMenu", "dialog#dialog"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->PressButton(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(AppMenuModel::kDownloadsMenuItem)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->SelectMenuItem(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kPrimaryTabPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             auto* const contents =
                                 element->AsA<TrackedElementWebPage>()
                                     ->owner()
                                     ->web_contents();
                             ASSERT_EQ(GURL("chrome://downloads/"),
                                       contents->GetURL());
                             ASSERT_NE(nullptr, contents->GetWebUI());
                             EXPECT_FALSE(
                                 download_page
                                     ->EvaluateAt(kDialogQuery, "el => el.open")
                                     .GetBool());
                             download_page->EvaluateAt(kButtonQuery,
                                                       "el => el.click()");
                             InteractionSequenceBrowserUtil::StateChange change;
                             change.where = kDialogQuery;
                             change.event =
                                 kDownloadsMoreActionsVisibleEventType;
                             change.test_function = "el => el.open";
                             download_page->SendEventOnStateChange(change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kDownloadsMoreActionsVisibleEventType)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This test checks that we can attach to a WebUI that isn't embedded in a tab.
IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilInteractiveUiTest,
                       OpenTabSearchMenuAndAccessWebUI) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  std::unique_ptr<InteractionSequenceBrowserUtil> tab_search_page;
  auto test_util = CreateInteractionTestUtil();
  const ui::ElementContext context = browser()->window()->GetElementContext();

  // Poke into the doc to find something that's not at the top level, just to
  // verify we can.
  const InteractionSequenceBrowserUtil::DeepQuery kTabSearchListQuery = {
      "tab-search-app", "#tabsList"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->PressButton(element);
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kTabSearchBubbleElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        auto* const web_view =
                            views::AsViewClass<WebUIBubbleDialogView>(
                                element->AsA<views::TrackedElementViews>()
                                    ->view())
                                ->web_view();
                        tab_search_page =
                            InteractionSequenceBrowserUtil::ForNonTabWebView(
                                web_view, kTabSearchPageElementId);
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             std::string not_found;
                             EXPECT_TRUE(tab_search_page->Exists(
                                 kTabSearchListQuery, &not_found))
                                 << "Not found: " << not_found;
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This test checks that when a WebUI is hidden, its element goes away.
IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilInteractiveUiTest,
                       OpenTabSearchMenuAndTestVisibility) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  std::unique_ptr<InteractionSequenceBrowserUtil> tab_search_page;
  auto test_util = CreateInteractionTestUtil();
  const ui::ElementContext context = browser()->window()->GetElementContext();
  base::raw_ptr<WebUIBubbleDialogView> bubble_view = nullptr;

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->PressButton(element);
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kTabSearchBubbleElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        bubble_view = views::AsViewClass<WebUIBubbleDialogView>(
                            element->AsA<views::TrackedElementViews>()->view());
                        tab_search_page =
                            InteractionSequenceBrowserUtil::ForNonTabWebView(
                                bubble_view->web_view(),
                                kTabSearchPageElementId);
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Hide the ancestor view. This should hide the
                             // whole chain and cause the element to be
                             // destroyed.
                             bubble_view->SetVisible(false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kTabSearchPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Verify we've also disposed of the element
                             // itself:
                             EXPECT_EQ(nullptr,
                                       tab_search_page->current_element_);
                             // Show the ancestor view. This should recreate the
                             // WebUI element.
                             bubble_view->SetVisible(true);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchPageElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This test checks that we can attach to a WebUI that is embedded in a tab.
IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilInteractiveUiTest,
                       CompareScreenshot_TabWebUI) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto download_page = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kPrimaryTabPageElementId);
  auto test_util = CreateInteractionTestUtil();
  const ui::ElementContext context = browser()->window()->GetElementContext();

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->PressButton(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(AppMenuModel::kDownloadsMenuItem)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->SelectMenuItem(element);
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kPrimaryTabPageElementId)
                  .SetTransitionOnlyOnEvent(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(
                            InteractionSequenceBrowserUtil::CompareScreenshot(
                                element, std::string(), "3654539"));
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilInteractiveUiTest,
                       GetElementBoundsInScreen) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto download_page = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kPrimaryTabPageElementId);
  auto test_util = CreateInteractionTestUtil();
  const ui::ElementContext context = browser()->window()->GetElementContext();
  const InteractionSequenceBrowserUtil::DeepQuery kButtonQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-icon-button#moreActions"};
  const InteractionSequenceBrowserUtil::DeepQuery kDialogQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-action-menu#moreActionsMenu", "dialog#dialog"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->PressButton(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(AppMenuModel::kDownloadsMenuItem)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->SelectMenuItem(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kPrimaryTabPageElementId)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             auto* const contents =
                                 element->AsA<TrackedElementWebPage>()
                                     ->owner()
                                     ->web_contents();
                             ASSERT_EQ(GURL("chrome://downloads/"),
                                       contents->GetURL());
                             ASSERT_NE(nullptr, contents->GetWebUI());
                             const gfx::Rect element_rect =
                                 download_page->GetElementBoundsInScreen(
                                     kButtonQuery);
                             EXPECT_FALSE(element_rect.IsEmpty());
                             const gfx::Rect window_rect =
                                 browser()->window()->GetBounds();
                             EXPECT_TRUE(window_rect.Contains(element_rect))
                                 << "Expected window rect "
                                 << window_rect.ToString()
                                 << " to contain element rect "
                                 << element_rect.ToString();
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilInteractiveUiTest,
                       UseElementBoundsInScreenToSendInput) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto download_page = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kPrimaryTabPageElementId);
  auto test_util = CreateInteractionTestUtil();
  const ui::ElementContext context = browser()->window()->GetElementContext();
  const InteractionSequenceBrowserUtil::DeepQuery kButtonQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-icon-button#moreActions"};
  const InteractionSequenceBrowserUtil::DeepQuery kDialogQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-action-menu#moreActionsMenu", "dialog#dialog"};

  // This is just a convenience function for a common task in a couple of steps.
  auto send_custom_event = [&](ui::CustomElementEventType event_type) {
    auto* const target =
        ui::ElementTracker::GetElementTracker()->GetUniqueElement(
            kPrimaryTabPageElementId, browser()->window()->GetElementContext());
    ASSERT_NE(nullptr, target);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(target,
                                                                  event_type);
  };

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->PressButton(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(AppMenuModel::kDownloadsMenuItem)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->SelectMenuItem(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kPrimaryTabPageElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        auto* const contents =
                            element->AsA<TrackedElementWebPage>()
                                ->owner()
                                ->web_contents();
                        ASSERT_EQ(GURL("chrome://downloads/"),
                                  contents->GetURL());
                        ASSERT_NE(nullptr, contents->GetWebUI());
                        const gfx::Rect element_rect =
                            download_page->GetElementBoundsInScreen(
                                kButtonQuery);
                        EXPECT_FALSE(element_rect.IsEmpty());
                        const gfx::Point target = element_rect.CenterPoint();

                        display::Screen* const screen =
                            display::Screen::GetScreen();
                        display::Display display =
                            screen->GetDisplayNearestPoint(target);

                        // Move mouse to the location we calculated for the
                        // button on screen.
                        EXPECT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
                            target.x(), target.y(),
                            base::BindLambdaForTesting([&]() {
                              send_custom_event(kMouseMoveCustomEvent);
                            })));
                      }))
                  .Build())
          // Once the mouse has moved, press the left mouse button.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kMouseMoveCustomEvent)
                       .SetElementID(kPrimaryTabPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(
                                 ui_controls::SendMouseEventsNotifyWhenDone(
                                     ui_controls::LEFT, ui_controls::DOWN,
                                     base::BindLambdaForTesting([&]() {
                                       send_custom_event(kMouseDownCustomEvent);
                                     })));
                           }))
                       .Build())
          // Release the left mouse button.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kMouseDownCustomEvent)
                       .SetElementID(kPrimaryTabPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(
                                 ui_controls::SendMouseEventsNotifyWhenDone(
                                     ui_controls::LEFT, ui_controls::UP,
                                     base::BindLambdaForTesting([&]() {
                                       send_custom_event(kMouseUpCustomEvent);
                                     })));
                           }))
                       .Build())
          // Once the left mouse button has been released, the menu should
          // appear, so wait for it.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                           kMouseUpCustomEvent)
                  .SetElementID(kPrimaryTabPageElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
                            ui_controls::LEFT, ui_controls::UP,
                            base::BindLambdaForTesting([&]() {
                              send_custom_event(kMouseUpCustomEvent);
                            })));
                        InteractionSequenceBrowserUtil::StateChange change;
                        change.where = kDialogQuery;
                        change.event = kDownloadsMoreActionsVisibleEventType;
                        change.test_function = "el => el.open";
                        download_page->SendEventOnStateChange(change);
                      }))
                  .Build())
          // If the menu appears as expected, the test is complete.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kDownloadsMoreActionsVisibleEventType)
                       .SetElementID(kPrimaryTabPageElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
