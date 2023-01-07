// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include "base/notreached.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_test_util.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {
constexpr char16_t kTabTitle[] = u"Test Tab 2";
constexpr char16_t kTabDomain[] = u"example.com";
constexpr char kTabUrl[] = "http://example.com/path/to/document.html";
}  // namespace

class TabHoverCardBubbleViewDialogBrowserTest
    : public DialogBrowserTest,
      public test::TabHoverCardTestUtil {
 public:
  ~TabHoverCardBubbleViewDialogBrowserTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    TabRendererData new_tab_data = TabRendererData();
    new_tab_data.title = kTabTitle;
    new_tab_data.last_committed_url = GURL(kTabUrl);
    GetTabStrip(browser())->AddTabAt(1, new_tab_data);

    SimulateHoverTab(browser(), 1);
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi())
      return false;

    TabStrip* const tab_strip = GetTabStrip(browser());
    Tab* const tab = tab_strip->tab_at(1);
    if (!tab) {
      NOTREACHED();
      return false;
    }
    TabHoverCardBubbleView* const hover_card = GetHoverCard(tab_strip);
    if (!hover_card) {
      NOTREACHED();
      return false;
    }

    EXPECT_EQ(kTabTitle, hover_card->GetTitleTextForTesting());
    EXPECT_EQ(kTabDomain, hover_card->GetDomainTextForTesting());
    EXPECT_EQ(tab, hover_card->GetAnchorView());
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewDialogBrowserTest,
                       InvokeUi_tab_hover_card) {
  set_baseline("3907325");
  ShowAndVerifyUi();
}
