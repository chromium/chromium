// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class NavigateAndroidBrowserTest : public AndroidBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
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
  raw_ptr<BrowserWindowInterface> browser_window_;
  raw_ptr<TabListInterface> tab_list_;
  raw_ptr<content::WebContents> web_contents_;
};

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest, Disposition_CurrentTab) {
  // Start at a known URL.
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents_, url1));
  ASSERT_EQ(url1, web_contents_->GetLastCommittedURL());

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
  // Start at a known URL.
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents_, url1));
  ASSERT_EQ(0, tab_list_->GetActiveIndex());
  ASSERT_EQ(1, tab_list_->GetTabCount());

  // Prepare and execute a NEW_BACKGROUND_TAB navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new tab was created and the navigation occurred in it.
  EXPECT_EQ(2, tab_list_->GetTabCount());
  tabs::TabInterface* new_tab = tab_list_->GetTab(1);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the original tab is still the active one.
  EXPECT_EQ(0, tab_list_->GetActiveIndex());
  EXPECT_EQ(url1, web_contents_->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest,
                       Disposition_NewForegroundTab) {
  // Start at a known URL.
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents_, url1));
  ASSERT_EQ(0, tab_list_->GetActiveIndex());
  ASSERT_EQ(1, tab_list_->GetTabCount());

  // Prepare and execute a NEW_FOREGROUND_TAB navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetWebContents());

  // Observe the navigation in the new tab's WebContents.
  content::TestNavigationObserver navigation_observer(handle->GetWebContents());
  navigation_observer.Wait();

  // Verify a new tab was created and the navigation occurred in it.
  EXPECT_EQ(2, tab_list_->GetTabCount());
  tabs::TabInterface* new_tab = tab_list_->GetTab(1);
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(url2, new_tab->GetContents()->GetLastCommittedURL());

  // Verify the new tab is now the active one.
  EXPECT_EQ(1, tab_list_->GetActiveIndex());
  EXPECT_EQ(new_tab, tab_list_->GetActiveTab());
  EXPECT_EQ(url1, web_contents_->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(NavigateAndroidBrowserTest, Navigate_ProfileShutdown) {
  // Start at a known URL.
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents_, url1));
  ASSERT_EQ(url1, web_contents_->GetLastCommittedURL());

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
