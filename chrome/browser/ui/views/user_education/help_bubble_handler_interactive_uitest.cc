// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadLaterWebContentsElementId);
constexpr char kUserEducationInternalsUrl[] =
    "chrome://internals/user-education";
}  // namespace

class HelpBubbleHandlerInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleHandlerInteractiveUiTest() {
  }
  ~HelpBubbleHandlerInteractiveUiTest() override = default;

  // Opens the side panel and instruments the Read Later WebContents as
  // kReadLaterWebContentsElementId.
  auto OpenReadingListSidePanel() {
    return Steps(
        // Remove delays in switching side panels to prevent possible race
        // conditions when selecting items from the side panel dropdown.
        Do([this]() {
          browser()
              ->GetFeatures()
              .side_panel_coordinator()
              ->SetNoDelaysForTesting(true);
        }),
        PressButton(kToolbarAppMenuButtonElementId),
        SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
        SelectMenuItem(BookmarkSubMenuModel::kReadingListMenuItem),
        SelectMenuItem(ReadingListSubMenuModel::kReadingListMenuShowUI),
        WaitForShow(kSidePanelElementId),
        WaitForShow(kReadLaterSidePanelWebViewElementId),
        // Ensure that the Reading List side panel loads properly.
        InstrumentNonTabWebView(kReadLaterWebContentsElementId,
                                kReadLaterSidePanelWebViewElementId));
  }

  auto OpenBookmarksSidePanel() {
    return Steps(
        PressButton(kToolbarAppMenuButtonElementId),
        SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
        SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkSidePanelItem),
        WaitForShow(kSidePanelElementId));
  }

  auto CloseSidePanel() {
    return Steps(EnsurePresent(kSidePanelElementId),
                 PressButton(kSidePanelCloseButtonElementId),
                 WaitForHide(kSidePanelElementId));
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ElementBecomesVisibleOnPageLoad) {
  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId, GURL(kUserEducationInternalsUrl)),
      InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ElementBecomesHiddenOnTabBackgrounded) {
  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId, GURL(kUserEducationInternalsUrl)),
      InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)),
      // This will add the new tab in the foreground.
      AddInstrumentedTab(kBrowserTabId2, GURL(chrome::kChromeUIBookmarksURL)),
      InAnyContext(WaitForHide(kWebUIIPHDemoElementIdentifier)));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ElementBecomesVisibleOnSecondaryUILoad) {
  RunTestSequence(
      OpenReadingListSidePanel(),
      InAnyContext(WaitForShow(kAddCurrentTabToReadingListElementId)));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ElementBecomesHiddenOnSecondaryUIHide) {
  RunTestSequence(
      OpenReadingListSidePanel(),
      InAnyContext(WaitForShow(kAddCurrentTabToReadingListElementId)),
      CloseSidePanel(),
      InAnyContext(WaitForHide(kAddCurrentTabToReadingListElementId)));
}

// This test is flaky on Mac; see: https://crbug.com/348242589
// Suspect that something in the async way the combo box works is causing this
// particular issue. Might be solved by programmatically switching panels.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ElementBecomesHiddenOnSecondaryUISwap \
  DISABLED_ElementBecomesHiddenOnSecondaryUISwap
#else
#define MAYBE_ElementBecomesHiddenOnSecondaryUISwap \
  ElementBecomesHiddenOnSecondaryUISwap
#endif
IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       MAYBE_ElementBecomesHiddenOnSecondaryUISwap) {
  RunTestSequence(
      OpenReadingListSidePanel(),
      InAnyContext(WaitForShow(kAddCurrentTabToReadingListElementId)),
      OpenBookmarksSidePanel(),
      InAnyContext(WaitForHide(kAddCurrentTabToReadingListElementId)));
}
