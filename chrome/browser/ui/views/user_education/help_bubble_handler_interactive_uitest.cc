// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/help_bubble_handler.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
}

class HelpBubbleHandlerInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleHandlerInteractiveUiTest() = default;
  ~HelpBubbleHandlerInteractiveUiTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ClickElementSendsEvent) {
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  RunTestSequence(
      // Need a valid non-internal URL to add to reading list.
      InstrumentTab(kPrimaryTabId), NavigateWebContents(kPrimaryTabId, url),

      // Open the read later side panel.
      PressButton(kSidePanelButtonElementId),
      SelectDropdownItem(kSidePanelComboboxElementId,
                         static_cast<int>(SidePanelEntry::Id::kReadingList)),

      // Click the "add to read later" button and verify that an "activated"
      // event is sent.
      InAnyContext(WaitForShow(kAddCurrentTabToReadingListElementId)),
      InSameContext(
          Steps(MoveMouseTo(kAddCurrentTabToReadingListElementId), ClickMouse(),
                WaitForActivate(kAddCurrentTabToReadingListElementId))));
}
