// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "base/base_switches.h"
#include "base/test/test_future.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class NavigateAndroidBrowserTest : public AndroidBrowserTest {
 public:
  NavigateAndroidBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {// Disable ChromeTabbedActivity instance limit so that the total number
         // of windows created by the entire test suite won't be limited.
         //
         // See MultiWindowUtils#getMaxInstances() for the reason:
         // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java;l=209;drc=0bcba72c5246a910240b311def40233f7d3f15af

         // Enable incognito windows on Android.
         feed::kAndroidOpenIncognitoAsWindow},
        /*disabled_features=*/{});
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    AndroidBrowserTest::SetUpDefaultCommandLine(command_line);

    // Disable the first-run experience (FRE) so that when a function under
    // test launches an Intent for ChromeTabbedActivity, ChromeTabbedActivity
    // will be shown instead of FirstRunActivity.
    command_line->AppendSwitch("disable-fre");

    // Force DeviceInfo#isDesktop() to be true so that the kDisableInstanceLimit
    // flag in the constructor can be effective when running tests on an
    // emulator without "--force-desktop-android".
    //
    // See MultiWindowUtils#getMaxInstances() for the reason:
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java;l=213;drc=0bcba72c5246a910240b311def40233f7d3f15af
    command_line->AppendSwitch(switches::kForceDesktopAndroid);
  }

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
  GURL StartAtURL(const std::string& url_path) {
    const GURL url = embedded_test_server()->GetURL(url_path);
    CHECK(content::NavigateToURL(web_contents_, url));
    CHECK_EQ(url, web_contents_->GetLastCommittedURL());
    return url;
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

 private:
  base::test::ScopedFeatureList feature_list_;
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
  const GURL url1 = StartAtURL("/title1.html");
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
  ASSERT_EQ(0, tab_list_->GetActiveIndex());
  ASSERT_EQ(1, tab_list_->GetTabCount());

  // Prepare and execute a NEW_BACKGROUND_TAB navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();
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
                       Async_Disposition_NewForegroundTab) {
  const GURL url1 = StartAtURL("/title1.html");
  ASSERT_EQ(0, tab_list_->GetActiveIndex());
  ASSERT_EQ(1, tab_list_->GetTabCount());

  // Prepare and execute a NEW_FOREGROUND_TAB navigation.
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser_window_, url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  base::test::TestFuture<base::WeakPtr<content::NavigationHandle>> future;
  Navigate(&params, future.GetCallback());
  base::WeakPtr<content::NavigationHandle> handle = future.Get();
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
