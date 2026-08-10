// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_tab_provider_desktop.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

class ContextHubTabProviderDesktopTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    provider_ = std::make_unique<ContextHubTabProviderDesktop>(profile());
  }

  void TearDown() override {
    provider_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  int64_t GetTabId(Browser* target_browser, int index) {
    content::WebContents* wc =
        target_browser->tab_strip_model()->GetWebContentsAt(index);
    return sessions::SessionTabHelper::IdForTab(wc).id();
  }

  std::unique_ptr<ContextHubTabProviderDesktop> provider_;
};

TEST_F(ContextHubTabProviderDesktopTest, GetTabs_ReturnsAllTabs) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  // Pin one tab and group another tab.
  browser()->tab_strip_model()->SetTabPinned(0, true);
  browser()->tab_strip_model()->AddToNewGroup({1});

  std::vector<content::WebContents*> tabs = provider_->GetTabs();
  EXPECT_EQ(tabs.size(), 3u);
}

TEST_F(ContextHubTabProviderDesktopTest,
       GetUngroupedTabs_ReturnsUngroupedTabsOnly) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  // Group one tab.
  browser()->tab_strip_model()->AddToNewGroup({0});

  std::vector<content::WebContents*> tabs = provider_->GetUngroupedTabs();
  EXPECT_EQ(tabs.size(), 2u);
}

TEST_F(ContextHubTabProviderDesktopTest,
       GetUngroupedTabs_ExcludesPinnedAndUnsupportedWindows) {
  AddTab(browser(), GURL("https://example.com/normal"));
  AddTab(browser(), GURL("https://example.com/pinned"));
  browser()->tab_strip_model()->SetTabPinned(0, true);

  // Window without tab group support (e.g. app window).
  std::unique_ptr<Browser> app_browser =
      CreateBrowser(profile(), Browser::TYPE_APP, /*hosted_app=*/true);
  AddTab(app_browser.get(), GURL("https://example.com/app"));

  std::vector<content::WebContents*> tabs = provider_->GetUngroupedTabs();
  ASSERT_EQ(tabs.size(), 1u);
  EXPECT_EQ(tabs[0]->GetVisibleURL(), GURL("https://example.com/normal"));

  app_browser->tab_strip_model()->CloseAllTabs();
}

TEST_F(ContextHubTabProviderDesktopTest, SwitchToTab_ActivatesTab) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);

  int64_t target_id = GetTabId(browser(), 2);
  provider_->SwitchToTab(target_id);

  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 2);
}

TEST_F(ContextHubTabProviderDesktopTest, GetTabs_MultipleWindows) {
  AddTab(browser(), GURL("https://example.com/w1_t1"));
  AddTab(browser(), GURL("https://example.com/w1_t2"));

  std::unique_ptr<Browser> second_browser =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, /*hosted_app=*/false);
  AddTab(second_browser.get(), GURL("https://example.com/w2_t1"));

  std::vector<content::WebContents*> tabs = provider_->GetTabs();
  EXPECT_EQ(tabs.size(), 3u);

  second_browser->tab_strip_model()->CloseAllTabs();
}

}  // namespace
}  // namespace context_hub
