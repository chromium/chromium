// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

using InteractionTestUtilBrowserUiTest = InteractiveBrowserTest;

// This test checks that we can attach to a WebUI that is embedded in a tab.
// TODO(crbug.com/1376747): This test fails on the Win bot.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CompareScreenshot_TabWebUI DISABLED_CompareScreenshot_TabWebUI
#else
#define MAYBE_CompareScreenshot_TabWebUI CompareScreenshot_TabWebUI
#endif
IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserUiTest,
                       MAYBE_CompareScreenshot_TabWebUI) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDownloadsPageElementId);

  RunTestSequence(
      InstrumentTab(kDownloadsPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
      WaitForWebContentsNavigation(kDownloadsPageElementId,
                                   GURL("chrome://downloads")),
      // This adds a callback that calls
      // InteractionTestUtilBrowser::CompareScreenshot().
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(kDownloadsPageElementId, std::string(), "3924454"));
}

// This test checks that we can attach to a WebUI that is not embedded in a tab.
// TODO(crbug.com/1354017): Re-enable this test
IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserUiTest,
                       DISABLED_CompareScreenshot_SecondaryWebUI) {
  // This will capture the tab search page when it is displayed.
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabSearchPageElementId);
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

  RunTestSequence(
      PressButton(kTabSearchButtonElementId),
      // Wait for the tab search bubble to appear and instrument its WebUI.
      AfterShow(kTabSearchBubbleElementId,
                base::BindLambdaForTesting([&](ui::InteractionSequence*,
                                               ui::TrackedElement* element) {
                  auto* const bubble_view =
                      views::AsViewClass<WebUIBubbleDialogView>(
                          element->AsA<views::TrackedElementViews>()->view());
                  tab_search_page =
                      WebContentsInteractionTestUtil::ForNonTabWebView(
                          bubble_view->web_view(), kTabSearchPageElementId);
                })),
      // Wait for the tab search page to appear, and then ensure it is
      // rendered at an appropriate size.
      AfterShow(kTabSearchPageElementId,
                base::BindLambdaForTesting(
                    [&](ui::InteractionSequence*, ui::TrackedElement* element) {
                      tab_search_page->SendEventOnWebViewMinimumSize(
                          kMinimumBubbleSize, kTabDataDisplayedEvent,
                          kTabSearchItemQuery, kMinimumEntrySize);
                    })),
      // With both the bubble and data at nonzero size, it should be safe to
      // take a screenshot.
      AfterEvent(
          kTabSearchPageElementId, kTabDataDisplayedEvent,
          base::BindLambdaForTesting([&](ui::InteractionSequence* sequence,
                                         ui::TrackedElement* element) {
            const auto result = InteractionTestUtilBrowser::CompareScreenshot(
                element, std::string(), "3664291");
            EXPECT_THAT(
                result,
                testing::AnyOf(ui::test::ActionResult::kSucceeded,
                               ui::test::ActionResult::kKnownIncompatible));
          })));
}
