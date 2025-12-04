// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"

#include "base/base_switches.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class AndroidBrowserWindowBrowserTest : public AndroidBrowserTest {
 public:
  AndroidBrowserWindowBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {// Disable ChromeTabbedActivity instance limit so that the total number
         // of
         // windows created by the entire test suite won't be limited.
         //
         // See MultiWindowUtils#getMaxInstances() for the reason:
         // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java;l=209;drc=0bcba72c5246a910240b311def40233f7d3f15af
         chrome::android::kDisableInstanceLimit,

         // Enable incognito windows on Android.
         feed::kAndroidOpenIncognitoAsWindow},
        /*disabled_features=*/
        {// Disable prewarm to avoid crash when profile is shutting down.
         features::kPrewarm});
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
  }

  AndroidBrowserWindow* GetBrowserWindow() {
    std::vector<BrowserWindowInterface*> windows =
        GetAllBrowserWindowInterfaces();
    EXPECT_EQ(1u, windows.size());
    return static_cast<AndroidBrowserWindow*>(windows[0]);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Case 1: Synchronous return, callback and return receive nonnull value.
IN_PROC_BROWSER_TEST_F(AndroidBrowserWindowBrowserTest,
                       OpenURL_URLCanBeOpenedImmediately) {
  AndroidBrowserWindow* window = GetBrowserWindow();
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);

  // We store GURL instead of NavigationHandle& to avoid storing an abstract
  // class.
  base::test::TestFuture<GURL> future;
  content::WebContents* result = window->OpenURL(
      params,
      base::BindLambdaForTesting([&](content::NavigationHandle& handle) {
        future.SetValue(handle.GetURL());
      }));

  // Result should be immediately available.
  EXPECT_NE(nullptr, result);
  EXPECT_EQ(url, result->GetVisibleURL());

  // Callback should have been called.
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(url, future.Get());
}

// Case 2: Asynchronous return, null return, callback received a value.
IN_PROC_BROWSER_TEST_F(AndroidBrowserWindowBrowserTest,
                       OpenURL_URLCannotBeOpenedImmediately_Success) {
  AndroidBrowserWindow* window = GetBrowserWindow();
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // NEW_WINDOW forces the asynchronous path in Android's Navigate().
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_WINDOW,
                                ui::PAGE_TRANSITION_LINK, false);

  base::test::TestFuture<GURL> future;
  content::WebContents* result = window->OpenURL(
      params,
      base::BindLambdaForTesting([&](content::NavigationHandle& handle) {
        future.SetValue(handle.GetURL());
      }));

  // Should return nullptr for async operation.
  EXPECT_EQ(nullptr, result);

  // Callback should eventually be called with a valid handle.
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(url, future.Get());
}

// Case 3: Asynchronous return, null return, callback not called.
IN_PROC_BROWSER_TEST_F(AndroidBrowserWindowBrowserTest,
                       OpenURL_URLCannotBeOpenedImmediately_Failure) {
  AndroidBrowserWindow* window = GetBrowserWindow();
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Initiate profile shutdown to force ValidNavigateParams to return false.
  Profile* profile = window->GetProfile();
  profile->NotifyWillBeDestroyed();
  ASSERT_TRUE(profile->ShutdownStarted());

  // Use NEW_WINDOW to force the code path that normally would go async.
  // Navigate() will fail internal validation and post a null result.
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_WINDOW,
                                ui::PAGE_TRANSITION_LINK, false);

  base::test::TestFuture<GURL> future;
  content::WebContents* result = window->OpenURL(
      params,
      base::BindLambdaForTesting([&](content::NavigationHandle& handle) {
        future.SetValue(handle.GetURL());
      }));

  // Should return nullptr.
  EXPECT_EQ(nullptr, result);

  // To verify the callback is NOT called, we need to wait until the
  // internal task posted by Navigate() has executed.
  // Since Navigate() posts its failure callback to the current thread,
  // we can post a task to quit the run loop. This ensures the Navigate()
  // task runs before our quit closure.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // The wrapper callback in OpenURL checks for a null handle and suppresses
  // the user callback if the handle is null.
  EXPECT_FALSE(future.IsReady());
}
