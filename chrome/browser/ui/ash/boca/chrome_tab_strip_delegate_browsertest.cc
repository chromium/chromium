// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/boca/chrome_tab_strip_delegate.h"

#include "base/test/test_future.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace {
constexpr char kTabUrl1[] = "https://foo/1";
constexpr char kTabUrl2[] = "https://foo/2";
constexpr char kTabUrl3[] = "https://foo/3";

constexpr char kDefaultTitle[] = "foo";

}  // namespace

class ChromeTabStripDelegateBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  ChromeTabStripDelegateBrowserTest() = default;
  ChromeTabStripDelegateBrowserTest(const ChromeTabStripDelegateBrowserTest&) =
      delete;
  ChromeTabStripDelegateBrowserTest& operator=(
      const ChromeTabStripDelegateBrowserTest&) = delete;
  ~ChromeTabStripDelegateBrowserTest() override = default;
  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    delegate_ = std::make_unique<ChromeTabStripDelegate>();
  }

  Browser* CreateBrowser(const std::vector<GURL>& urls,
                         std::optional<size_t> active_url_index) {
    Browser::CreateParams params(Browser::TYPE_NORMAL, profile(),
                                 /*user_gesture=*/false);
    Browser* browser = Browser::Create(params);
    // Create a new tab and make sure the urls have loaded.
    for (const auto& url : urls) {
      // content::TestNavigationObserver navigation_observer(urls[i]);
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    return browser;
  }
  ChromeTabStripDelegate* delegate() { return delegate_.get(); }

  void ExpectTab(GURL url, std::string title, ash::TabInfo tab) {
    // Tab in test setup doesn't include title info.
    EXPECT_EQ(base::UTF8ToUTF16(title), tab.title);
    EXPECT_EQ(url, tab.url);
    EXPECT_GT(tab.last_access_timetick, base::TimeTicks());
  }

 private:
  std::unique_ptr<ChromeTabStripDelegate> delegate_;
};

IN_PROC_BROWSER_TEST_F(ChromeTabStripDelegateBrowserTest, GetTabListForWindow) {
  Browser* browser = CreateBrowser({GURL(kTabUrl1), GURL(kTabUrl2)},
                                   /*active_url_index=*/0);

  // Add tab in a new browser.
  CreateBrowser({GURL(kTabUrl3)}, /*active_url_index=*/1);

  auto* aura_window = browser->window()->GetNativeWindow();

  base::test::TestFuture<std::vector<ash::TabInfo>> future;
  auto tab_list = delegate()->GetTabsListForWindow(aura_window);

  ASSERT_EQ(2u, tab_list.size());
  ExpectTab(GURL(kTabUrl1), kDefaultTitle, tab_list[0]);
  ExpectTab(GURL(kTabUrl2), kDefaultTitle, tab_list[1]);

  // Verify last access time reflects the access order.
  EXPECT_GT(tab_list[1].last_access_timetick, tab_list[0].last_access_timetick);
}
