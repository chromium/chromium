// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "base/base_switches.h"
#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// Helper classes used to track tabs and navigations.
class NavigationCounter : public content::WebContentsObserver {
 public:
  explicit NavigationCounter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    // Ignore warmup navigation.
    if (base::EndsWith(handle->GetURL().path(), "warmup.html")) {
      return;
    }
    finish_count_++;
  }

  int finish_count() const { return finish_count_; }

 private:
  int finish_count_ = 0;
};

class TabAdditionObserver : public TabListInterfaceObserver {
 public:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override {
    counter_ = std::make_unique<NavigationCounter>(tab->GetContents());
  }

  NavigationCounter* counter() { return counter_.get(); }

 private:
  std::unique_ptr<NavigationCounter> counter_;
};

}  // namespace

class NavigateAndroidBrowserTest : public BrowserWindowAndroidBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    BrowserWindowAndroidBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    std::vector<BrowserWindowInterface*> windows =
        GetAllBrowserWindowInterfaces();
    ASSERT_EQ(1u, windows.size());
    browser_window_ = windows[0];

    tab_list_ = TabListInterface::From(browser_window_);
    ASSERT_EQ(1, tab_list_->GetTabCount());

    web_contents_ = tab_list_->GetActiveTab()->GetContents();
    ASSERT_TRUE(web_contents_);
  }

 protected:
  GURL StartAtURL(const std::string& url_path) {
    const GURL url = embedded_test_server()->GetURL(url_path);
    CHECK(content::NavigateToURL(web_contents_, url));
    CHECK_EQ(url, web_contents_->GetLastCommittedURL());
    return url;
  }

  // Helper to add tabs to the current window.
  void CreateTabs(int count) {
    for (int i = 0; i < count; ++i) {
      NavigateParams params(browser_window_, GURL("about:blank"),
                            ui::PAGE_TRANSITION_TYPED);
      params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
      Navigate(&params);
    }
  }

  BrowserWindowInterface* CreateIncognitoBrowserWindow() {
    Profile* incognito_profile =
        GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    BrowserWindowCreateParams create_params(BrowserWindowInterface::TYPE_NORMAL,
                                            *incognito_profile, false);
    base::test::TestFuture<BrowserWindowInterface*> future;
    CreateBrowserWindow(std::move(create_params), future.GetCallback());
    BrowserWindowInterface* incognito_window = future.Get();
    EXPECT_TRUE(incognito_window);
    EXPECT_TRUE(incognito_window->GetProfile()->IsOffTheRecord());
    return incognito_window;
  }

  raw_ptr<BrowserWindowInterface> browser_window_;
  raw_ptr<TabListInterface> tab_list_;
  raw_ptr<content::WebContents> web_contents_;
};

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest, Disposition_CurrentTab) {
  const GURL url1 = StartAtURL("/title1.html");

  // Prepare and execute a CURRENT_TAB navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.source_contents = web_contents_;

  content::TestNavigationObserver navigation_observer(web_contents_);
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  EXPECT_TRUE(handle);
  EXPECT_EQ(url2, handle->GetURL());
  navigation_observer.Wait();

  // Verify the navigation happened in the same tab and window.
  EXPECT_EQ(url2, web_contents_->GetLastCommittedURL());
  EXPECT_EQ(1, tab_list_->GetTabCount());
  ASSERT_EQ(1u, GetAllBrowserWindowInterfaces().size());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Disposition_NewBackgroundTab) {
  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0 (active), Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Set the middle tab (Index 1) as active to test insertion logic.
  // Current state: [Tab0, Tab1 (Active), Tab2]
  tabs::TabInterface* source_tab = tab_list_->GetTab(1);
  tab_list_->ActivateTab(source_tab->GetHandle());
  ASSERT_EQ(1, tab_list_->GetActiveIndex());

  // 1. Prepare and execute first NEW_BACKGROUND_TAB navigation.
  // With PAGE_TRANSITION_LINK, the new tab should be inserted next to the
  // opener.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params1(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params1.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params1.source_contents = source_tab->GetContents();

  base::WeakPtr<content::NavigationHandle> handle1 = Navigate(&params1);
  ASSERT_TRUE(handle1);
  content::TestNavigationObserver observer1(handle1->GetWebContents());
  observer1.Wait();

  // Verify state: [Tab0, Tab1 (Active), NewTab1, Tab2]
  EXPECT_EQ(4, tab_list_->GetTabCount());
  tabs::TabInterface* new_tab1 = tab_list_->GetTab(2);
  EXPECT_EQ(url2, new_tab1->GetContents()->GetLastCommittedURL());
  EXPECT_EQ(source_tab, tab_list_->GetActiveTab());

  // 2. Prepare and execute second NEW_BACKGROUND_TAB navigation from SAME
  // opener. It should be inserted AFTER NewTab1 but BEFORE Tab2.
  const GURL url3 = embedded_test_server()->GetURL("/title3.html");
  NavigateParams params2(browser_window_, url3, ui::PAGE_TRANSITION_LINK);
  params2.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params2.source_contents = source_tab->GetContents();

  base::WeakPtr<content::NavigationHandle> handle2 = Navigate(&params2);
  ASSERT_TRUE(handle2);
  content::TestNavigationObserver observer2(handle2->GetWebContents());
  observer2.Wait();

  // Verify State: [Tab0, Tab1 (Active), NewTab1, NewTab2, Tab2]
  EXPECT_EQ(5, tab_list_->GetTabCount());

  // Verify NewTab1 is still at Index 2
  EXPECT_EQ(new_tab1, tab_list_->GetTab(2));

  // Verify NewTab2 is at Index 3 (The key check)
  tabs::TabInterface* new_tab2 = tab_list_->GetTab(3);
  ASSERT_TRUE(new_tab2);
  EXPECT_EQ(url3, new_tab2->GetContents()->GetLastCommittedURL());

  // Verify original Tab2 pushed to Index 4
  EXPECT_EQ(GURL("about:blank"),
            tab_list_->GetTab(4)->GetContents()->GetVisibleURL());

  // Verify Active Tab is still Source Tab
  EXPECT_EQ(1, tab_list_->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Navigate_NewBackgroundTab_NotLink_AppendsToEnd) {
  // Test ensuring that background navigations that are NOT links (e.g. Typed,
  // Bookmarks) are appended to the end of the tabstrip, rather than grouped
  // next to the opener.
  // This verifies usage of TabLaunchType::FROM_BROWSER_ACTIONS (or similar)
  // instead of FROM_LONGPRESS_BACKGROUND.

  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0 (active), Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Set the middle tab (Index 1) as active.
  tabs::TabInterface* source_tab = tab_list_->GetTab(1);
  tab_list_->ActivateTab(source_tab->GetHandle());
  ASSERT_EQ(1, tab_list_->GetActiveIndex());

  // Execute NEW_BACKGROUND_TAB navigation with PAGE_TRANSITION_TYPED.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params.source_contents = source_tab->GetContents();

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  content::TestNavigationObserver observer(handle->GetWebContents());
  observer.Wait();

  // Expected State: [Tab0, Tab1 (Active), Tab2, NewTab]
  // The new tab should be at Index 3 (End), NOT Index 2 (Next to Opener).
  EXPECT_EQ(4, tab_list_->GetTabCount());

  tabs::TabInterface* new_tab = tab_list_->GetTab(3);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify active tab didn't change
  EXPECT_EQ(1, tab_list_->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Disposition_NewForegroundTab) {
  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0 (active), Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Set the middle tab (Index 1) as active to test insertion logic.
  // Current state: [Tab0, Tab1 (Active), Tab2]
  tabs::TabInterface* source_tab = tab_list_->GetTab(1);
  tab_list_->ActivateTab(source_tab->GetHandle());
  ASSERT_EQ(1, tab_list_->GetActiveIndex());

  // Prepare and execute a NEW_FOREGROUND_TAB navigation from the active tab.
  // With PAGE_TRANSITION_LINK, the new tab should be inserted next to the
  // opener.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.source_contents = source_tab->GetContents();

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new tab was created.
  EXPECT_EQ(4, tab_list_->GetTabCount());

  // Verify insertion position: Should be at Index 2 (right after active tab 1).
  // Expected State: [Tab0, Tab1, NewTab (Active), Tab2]
  tabs::TabInterface* new_tab = tab_list_->GetTab(2);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the new tab is now the active one.
  EXPECT_EQ(2, tab_list_->GetActiveIndex());
  EXPECT_EQ(new_tab, tab_list_->GetActiveTab());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Navigate_NewForegroundTab_Omnibox_AppendsToEnd) {
  // Test ensuring that foreground navigations from the Omnibox (TYPED)
  // are appended to the end of the tabstrip, rather than adjacent to the
  // opener.
  // This verifies usage of TabLaunchType::FROM_OMNIBOX.

  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0 (active), Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Set the middle tab (Index 1) as active.
  tabs::TabInterface* source_tab = tab_list_->GetTab(1);
  tab_list_->ActivateTab(source_tab->GetHandle());
  ASSERT_EQ(1, tab_list_->GetActiveIndex());

  // Execute NEW_FOREGROUND_TAB navigation with PAGE_TRANSITION_TYPED.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.source_contents = source_tab->GetContents();

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  content::TestNavigationObserver observer(handle->GetWebContents());
  observer.Wait();

  // Expected State: [Tab0, Tab1, Tab2, NewTab (Active)]
  // The new tab should be at Index 3 (End), NOT Index 2 (Next to Opener).
  EXPECT_EQ(4, tab_list_->GetTabCount());

  tabs::TabInterface* new_tab = tab_list_->GetTab(3);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify new tab is active
  EXPECT_EQ(3, tab_list_->GetActiveIndex());
  EXPECT_EQ(new_tab, tab_list_->GetActiveTab());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest, Navigate_ProfileShutdown) {
  const GURL url1 = StartAtURL("/title1.html");

  // Start shutdown on the profile.
  Profile* profile = browser_window_->GetProfile();
  profile->NotifyWillBeDestroyed();
  ASSERT_TRUE(profile->ShutdownStarted());

  // Prepare and execute a navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.source_contents = web_contents_;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);

  // Verify the navigation was blocked.
  EXPECT_FALSE(handle);
  EXPECT_EQ(url1, web_contents_->GetLastCommittedURL());
  EXPECT_EQ(1, tab_list_->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Async_Disposition_CurrentTab) {
  const GURL url1 = StartAtURL("/title1.html");

  // Prepare and execute a CURRENT_TAB navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.source_contents = web_contents_;

  content::TestNavigationObserver navigation_observer(web_contents_);
  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();
  EXPECT_TRUE(handle);
  EXPECT_EQ(url2, handle->GetURL());
  navigation_observer.Wait();

  // Verify the navigation happened in the same tab and window.
  EXPECT_EQ(url2, web_contents_->GetLastCommittedURL());
  EXPECT_EQ(1, tab_list_->GetTabCount());
  ASSERT_EQ(1u, GetAllBrowserWindowInterfaces().size());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Async_Disposition_NewBackgroundTab) {
  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0 (active), Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Set the middle tab (Index 1) as active.
  tabs::TabInterface* source_tab = tab_list_->GetTab(1);
  tab_list_->ActivateTab(source_tab->GetHandle());
  ASSERT_EQ(1, tab_list_->GetActiveIndex());

  // 1. First Background Tab
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params1(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params1.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params1.source_contents = source_tab->GetContents();

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future1;
  Navigate(&params1, future1.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle1 = future1.Get();
  ASSERT_TRUE(handle1);
  content::TestNavigationObserver observer1(handle1->GetWebContents());
  observer1.Wait();

  EXPECT_EQ(4, tab_list_->GetTabCount());
  tabs::TabInterface* new_tab1 = tab_list_->GetTab(2);
  EXPECT_EQ(url2, new_tab1->GetContents()->GetLastCommittedURL());

  // 2. Second Background Tab (Same Source)
  const GURL url3 = embedded_test_server()->GetURL("/title3.html");
  NavigateParams params2(browser_window_, url3, ui::PAGE_TRANSITION_LINK);
  params2.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params2.source_contents = source_tab->GetContents();

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future2;
  Navigate(&params2, future2.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle2 = future2.Get();
  ASSERT_TRUE(handle2);
  content::TestNavigationObserver observer2(handle2->GetWebContents());
  observer2.Wait();

  // Verify Order: [Tab0, Tab1 (Active), NewTab1, NewTab2, Tab2]
  EXPECT_EQ(5, tab_list_->GetTabCount());

  // Verify NewTab1 still at Index 2
  EXPECT_EQ(new_tab1, tab_list_->GetTab(2));

  // Verify NewTab2 at Index 3
  EXPECT_EQ(url3, tab_list_->GetTab(3)->GetContents()->GetLastCommittedURL());

  // Verify Tab2 pushed to Index 4
  EXPECT_EQ(GURL("about:blank"),
            tab_list_->GetTab(4)->GetContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Async_Disposition_NewForegroundTab) {
  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0 (active), Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Set the middle tab (Index 1) as active to test insertion logic.
  // Current state: [Tab0, Tab1 (Active), Tab2]
  tabs::TabInterface* source_tab = tab_list_->GetTab(1);
  tab_list_->ActivateTab(source_tab->GetHandle());
  ASSERT_EQ(1, tab_list_->GetActiveIndex());

  // Prepare and execute a NEW_FOREGROUND_TAB navigation from the active tab.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.source_contents = source_tab->GetContents();

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new tab was created.
  EXPECT_EQ(4, tab_list_->GetTabCount());

  // Verify insertion position: Should be at Index 2 (right after active tab 1).
  // Expected State: [Tab0, Tab1, NewTab (Active), Tab2]
  tabs::TabInterface* new_tab = tab_list_->GetTab(2);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the new tab is now the active one.
  EXPECT_EQ(2, tab_list_->GetActiveIndex());
  EXPECT_EQ(new_tab, tab_list_->GetActiveTab());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Async_Navigate_ProfileShutdown) {
  const GURL url1 = StartAtURL("/title1.html");

  // Start shutdown on the profile.
  Profile* profile = browser_window_->GetProfile();
  profile->NotifyWillBeDestroyed();
  ASSERT_TRUE(profile->ShutdownStarted());

  // Prepare and execute a navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.source_contents = web_contents_;

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();

  // Verify the navigation was blocked.
  EXPECT_FALSE(handle);
  EXPECT_EQ(url1, web_contents_->GetLastCommittedURL());
  EXPECT_EQ(1, tab_list_->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Async_Disposition_NewWindow) {
  const GURL url1 = StartAtURL("/title1.html");
  ASSERT_EQ(1u, GetAllBrowserWindowInterfaces().size());

  // Prepare and execute a NEW_WINDOW navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_WINDOW;

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new window was created and the navigation occurred in it.
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  ASSERT_EQ(2u, windows.size());
  BrowserWindowInterface* new_window =
      windows[0] == browser_window_ ? windows[1] : windows[0];
  TabListInterface* new_tab_list = TabListInterface::From(new_window);
  EXPECT_EQ(1, new_tab_list->GetTabCount());
  tabs::TabInterface* new_tab = new_tab_list->GetTab(0);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the original window is unchanged.
  EXPECT_EQ(1, tab_list_->GetTabCount());
  EXPECT_EQ(url1, web_contents_->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest, Async_Disposition_NewPopup) {
  const GURL url1 = StartAtURL("/title1.html");
  ASSERT_EQ(1u, GetAllBrowserWindowInterfaces().size());

  // Prepare and execute a NEW_POPUP navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new window was created and the navigation occurred in it.
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  ASSERT_EQ(2u, windows.size());
  BrowserWindowInterface* new_window =
      windows[0] == browser_window_ ? windows[1] : windows[0];
  EXPECT_EQ(new_window->GetType(), BrowserWindowInterface::Type::TYPE_POPUP);
  TabListInterface* new_tab_list = TabListInterface::From(new_window);
  EXPECT_EQ(1, new_tab_list->GetTabCount());
  tabs::TabInterface* new_tab = new_tab_list->GetTab(0);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the original window is unchanged.
  EXPECT_EQ(1, tab_list_->GetTabCount());
  EXPECT_EQ(url1, web_contents_->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Disposition_NewPopup_ReturnsNull) {
  // Prepare and execute a NEW_POPUP navigation.
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  NavigateParams params(browser_window_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;

  // Synchronous Navigate() should return null for NEW_POPUP.
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  EXPECT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(
    NavigateAndroidBrowserTest,
    Disposition_OffTheRecord_FromRegularProfile_ReturnsNull) {
  ASSERT_FALSE(browser_window_->GetProfile()->IsOffTheRecord());

  // Prepare and execute an OFF_THE_RECORD navigation.
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  NavigateParams params(browser_window_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;

  // Synchronous Navigate() from a regular profile should return null.
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  EXPECT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Disposition_OffTheRecord_FromIncognitoProfile) {
  // Create a new incognito window.
  BrowserWindowInterface* incognito_window = CreateIncognitoBrowserWindow();
  TabListInterface* incognito_tab_list =
      TabListInterface::From(incognito_window);
  ASSERT_EQ(1, incognito_tab_list->GetTabCount());

  // Prepare and execute an OFF_THE_RECORD navigation from the incognito window.
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  NavigateParams params(incognito_window, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new tab was created in the incognito window and the navigation
  // occurred in it.
  EXPECT_EQ(2, incognito_tab_list->GetTabCount());
  tabs::TabInterface* new_tab = incognito_tab_list->GetTab(1);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the new tab is now the active one.
  EXPECT_EQ(1, incognito_tab_list->GetActiveIndex());
  EXPECT_EQ(new_tab, incognito_tab_list->GetActiveTab());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Async_Disposition_OffTheRecord_FromRegularProfile) {
  const GURL url1 = StartAtURL("/title1.html");
  ASSERT_EQ(1u, GetAllBrowserWindowInterfaces().size());
  ASSERT_FALSE(browser_window_->GetProfile()->IsOffTheRecord());

  // Prepare and execute an OFF_THE_RECORD navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new incognito window was created and the navigation occurred in
  // it.
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  ASSERT_EQ(2u, windows.size());
  BrowserWindowInterface* new_window =
      windows[0] == browser_window_ ? windows[1] : windows[0];
  EXPECT_TRUE(new_window->GetProfile()->IsOffTheRecord());
  TabListInterface* new_tab_list = TabListInterface::From(new_window);
  EXPECT_EQ(1, new_tab_list->GetTabCount());
  tabs::TabInterface* new_tab = new_tab_list->GetTab(0);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the original window is unchanged.
  EXPECT_EQ(1, tab_list_->GetTabCount());
  EXPECT_EQ(url1, web_contents_->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Async_Disposition_OffTheRecord_FromIncognitoProfile) {
  // Create a new incognito window.
  BrowserWindowInterface* incognito_window = CreateIncognitoBrowserWindow();
  TabListInterface* incognito_tab_list =
      TabListInterface::From(incognito_window);
  ASSERT_EQ(1, incognito_tab_list->GetTabCount());

  // Prepare and execute an OFF_THE_RECORD navigation from the incognito window.
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  NavigateParams params(incognito_window, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>>
      navigate_future;
  Navigate(&params, navigate_future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = navigate_future.Get();
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new tab was created in the incognito window and the navigation
  // occurred in it.
  EXPECT_EQ(2, incognito_tab_list->GetTabCount());
  tabs::TabInterface* new_tab = incognito_tab_list->GetTab(1);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the new tab is now the active one.
  EXPECT_EQ(1, incognito_tab_list->GetActiveIndex());
  EXPECT_EQ(new_tab, incognito_tab_list->GetActiveTab());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest, EnsureSingleNavigation) {
  const GURL url1 = StartAtURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  TabAdditionObserver tab_observer;
  tab_list_->AddTabListInterfaceObserver(&tab_observer);

  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);

  content::TestNavigationObserver observer(handle->GetWebContents());
  observer.Wait();

  ASSERT_TRUE(tab_observer.counter());
  // If Navigate() navigates twice, this will be 2.
  EXPECT_EQ(1, tab_observer.counter()->finish_count());

  tab_list_->RemoveTabListInterfaceObserver(&tab_observer);
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Navigate_WithExplicitTabstripIndex) {
  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0, Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Prepare a navigation with an explicit index of 1.
  // Current state: [Tab0, Tab1, Tab2]
  // Desired state: [Tab0, NewTab, Tab1, Tab2]
  const GURL url_new = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url_new, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = 1;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  content::TestNavigationObserver observer(handle->GetWebContents());
  observer.Wait();

  // Verify the tab count increased.
  EXPECT_EQ(4, tab_list_->GetTabCount());

  // Verify the new tab is exactly at index 1.
  tabs::TabInterface* new_tab = tab_list_->GetTab(1);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url_new, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the original Tab1 (which was at index 1) moved to index 2.
  EXPECT_EQ(GURL("about:blank"),
            tab_list_->GetTab(2)->GetContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Navigate_WithExplicitTabstripIndexInvalidIndex) {
  const GURL url1 = StartAtURL("/title1.html");
  // Create 2 extra tabs to establish a list: [Tab0, Tab1, Tab2].
  CreateTabs(2);
  ASSERT_EQ(3, tab_list_->GetTabCount());

  // Prepare a navigation with an invalid index of 10.
  // Current state: [Tab0, Tab1, Tab2]
  // Expected state: [Tab0, Tab1, Tab2, NewTab]
  const GURL url_new = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url_new, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = 10;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  content::TestNavigationObserver observer(handle->GetWebContents());
  observer.Wait();

  // Verify the tab count increased.
  EXPECT_EQ(4, tab_list_->GetTabCount());

  // Verify the new tab is at the final index.
  tabs::TabInterface* new_tab = tab_list_->GetTab(3);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url_new, new_tab->GetContents()->GetLastCommittedURL());
}
