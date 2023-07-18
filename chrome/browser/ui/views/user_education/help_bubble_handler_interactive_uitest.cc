// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
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
  HelpBubbleHandlerInteractiveUiTest() = default;
  ~HelpBubbleHandlerInteractiveUiTest() override = default;

  // Opens the side panel and instruments the Read Later WebContents as
  // kReadLaterWebContentsElementId.
  auto OpenReadingListSidePanel() {
    return Steps(
        // Remove delays in switching side panels to prevent possible race
        // conditions when selecting items from the side panel dropdown.
        Do([this]() {
          SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser())
              ->SetNoDelaysForTesting(true);
        }),
        // Click the Side Panel button and wait for the side panel to appear.
        PressButton(kSidePanelButtonElementId),
        WaitForShow(kSidePanelElementId), FlushEvents(),
        // Select the Reading List side panel and wait for the WebView to
        // appear.
        SelectDropdownItem(kSidePanelComboboxElementId,
                           static_cast<int>(SidePanelEntry::Id::kReadingList)),
        WaitForShow(kReadLaterSidePanelWebViewElementId),
        // Ensure that the Reading List side panel loads properly.
        InstrumentNonTabWebView(kReadLaterWebContentsElementId,
                                kReadLaterSidePanelWebViewElementId));
  }

  auto CloseSidePanel() {
    return Steps(EnsurePresent(kSidePanelElementId),
                 PressButton(kSidePanelButtonElementId),
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

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ElementBecomesHiddenOnSecondaryUISwap) {
  RunTestSequence(
      OpenReadingListSidePanel(),
      InAnyContext(WaitForShow(kAddCurrentTabToReadingListElementId)),
      SelectDropdownItem(kSidePanelComboboxElementId,
                         static_cast<int>(SidePanelEntry::Id::kBookmarks)),
      InAnyContext(WaitForHide(kAddCurrentTabToReadingListElementId)));
}
