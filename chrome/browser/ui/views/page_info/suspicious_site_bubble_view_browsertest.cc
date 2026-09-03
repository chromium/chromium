// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/suspicious_site_bubble_view.h"

#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class SuspiciousSiteBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  SuspiciousSiteBubbleViewBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBubbleViewBrowserTest,
                       ShowsAndAnchorsBubble) {
  GURL test_url = embedded_test_server()->GetURL("a.test", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  ShowSuspiciousSiteBubble(web_contents());

  EXPECT_EQ(PageInfoBubbleViewBase::GetShownBubbleType(),
            PageInfoBubbleViewBase::BUBBLE_SUSPICIOUS_SITE);
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  ASSERT_NE(bubble, nullptr);
  EXPECT_TRUE(bubble->GetWidget()->IsVisible());

  EXPECT_TRUE(web_contents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_TRUE(browser()->GetTabStripModel()->IsTabBlocked(0));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBubbleViewBrowserTest, MarkAsSafeAction) {
  GURL test_url = embedded_test_server()->GetURL("a.test", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  safe_browsing::SuspiciousSiteWarningAllowlist allowlist(hcsm);
  EXPECT_FALSE(allowlist.IsSiteAllowedForHost("a.test"));

  ShowSuspiciousSiteBubble(web_contents());

  auto* bubble = static_cast<SuspiciousSiteBubbleView*>(
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  ASSERT_NE(bubble, nullptr);
  EXPECT_TRUE(web_contents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_TRUE(browser()->GetTabStripModel()->IsTabBlocked(0));

  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::ButtonTestApi(bubble->mark_as_safe_button_for_testing())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  waiter.Wait();

  EXPECT_TRUE(allowlist.IsSiteAllowedForHost("a.test"));
  EXPECT_EQ(PageInfoBubbleViewBase::GetShownBubbleType(),
            PageInfoBubbleViewBase::BUBBLE_NONE);
  EXPECT_FALSE(web_contents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_FALSE(browser()->GetTabStripModel()->IsTabBlocked(0));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBubbleViewBrowserTest,
                       BackToSafetyAction) {
  GURL first_url = embedded_test_server()->GetURL("safe.test", "/simple.html");
  GURL second_url =
      embedded_test_server()->GetURL("suspicious.test", "/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  ShowSuspiciousSiteBubble(web_contents());

  auto* bubble = static_cast<SuspiciousSiteBubbleView*>(
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  ASSERT_NE(bubble, nullptr);
  EXPECT_TRUE(web_contents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_TRUE(browser()->GetTabStripModel()->IsTabBlocked(0));

  content::TestNavigationObserver nav_observer(web_contents());
  views::test::ButtonTestApi(bubble->back_to_safety_button_for_testing())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  nav_observer.Wait();

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), first_url);
  EXPECT_FALSE(web_contents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_FALSE(browser()->GetTabStripModel()->IsTabBlocked(0));
}
