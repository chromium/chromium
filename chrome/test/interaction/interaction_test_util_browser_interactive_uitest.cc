// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

using InteractionTestUtilBrowserUiTest = InProcessBrowserTest;

// This test checks that we can attach to a WebUI that is embedded in a tab.
IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserUiTest,
                       CompareScreenshot_TabWebUI) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);

  auto download_page = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kPrimaryTabPageElementId);
  InteractionTestUtilBrowser test_util;
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
                           [&](ui::TrackedElement* element) {
                             test_util.PressButton(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(AppMenuModel::kDownloadsMenuItem)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util.SelectMenuItem(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kPrimaryTabPageElementId)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::TrackedElement* element) {
                             EXPECT_TRUE(
                                 InteractionTestUtilBrowser::CompareScreenshot(
                                     element, std::string(), "3654539"));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This test checks that we can attach to a WebUI that is not embedded in a tab.
IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserUiTest,
                       // TODO(crbug.com/1354017): Re-enable this test
                       DISABLED_CompareScreenshot_SecondaryWebUI) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabSearchPageElementId);

  InteractionTestUtilBrowser test_util;

  // This will capture the tab search page when it is displayed.
  std::unique_ptr<WebContentsInteractionTestUtil> tab_search_page;

  // Need to wait for the tab items to actually show up in the tab list (this
  // can be asynchronous).
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTabDataDisplayedEvent);
  const WebContentsInteractionTestUtil::DeepQuery kTabSearchItemQuery{
      "tab-search-app", "tab-search-item"};

  // We expect a tab search bubble with a single tab listed to be somewhere
  // north of 150 DIP tall, and 300 DIP wide, but this value gives us a nice
  // cushion in case styling changes.
  constexpr gfx::Size kMinimumBubbleSize(200, 120);
  // Similarly, we underestimate entry size.
  constexpr gfx::Size kMinimumEntrySize(200, 20);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          // Press the Tab Search button.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util.PressButton(element);
                           })))
          // Wait for the tab search bubble to appear and instrument its WEbUI.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kTabSearchBubbleElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        auto* const bubble_view =
                            views::AsViewClass<WebUIBubbleDialogView>(
                                element->AsA<views::TrackedElementViews>()
                                    ->view());
                        tab_search_page =
                            WebContentsInteractionTestUtil::ForNonTabWebView(
                                bubble_view->web_view(),
                                kTabSearchPageElementId);
                      })))
          // Wait for the tab search page to appear, and then ensure it is
          // rendered at an appropriate size.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             tab_search_page->SendEventOnWebViewMinimumSize(
                                 kMinimumBubbleSize, kTabDataDisplayedEvent,
                                 kTabSearchItemQuery, kMinimumEntrySize);
                           })))
          // With both the bubble and data at nonzero size, it should be safe to
          // take a screenshot.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kTabDataDisplayedEvent)
                       .SetElementID(kTabSearchPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(
                                 InteractionTestUtilBrowser::CompareScreenshot(
                                     element, std::string(), "3664291"));
                           })))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
