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
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class AndroidBrowserWindowBrowserTest
    : public BrowserWindowAndroidBrowserTestBase {
 public:
  AndroidBrowserWindowBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {},
        /*disabled_features=*/
        {// Disable prewarm to avoid crash when profile is shutting down.
         features::kPrewarm});
  }

  void SetUpOnMainThread() override {
    BrowserWindowAndroidBrowserTestBase::SetUpOnMainThread();
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

IN_PROC_BROWSER_TEST_F(AndroidBrowserWindowBrowserTest, OpenURL_PopupBlocked) {
  AndroidBrowserWindow* window = GetBrowserWindow();
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // 1. Navigate to a page first to have a valid source.
  content::OpenURLParams source_params(
      embedded_test_server()->GetURL("/simple.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);

  base::test::TestFuture<GURL> source_future;
  content::WebContents* source_contents = window->OpenURL(
      source_params,
      base::BindLambdaForTesting([&](content::NavigationHandle& handle) {
        source_future.SetValue(handle.GetURL());
      }));
  ASSERT_TRUE(source_contents);
  ASSERT_TRUE(source_future.Wait());

  content::RenderFrameHost* source_rfh = source_contents->GetPrimaryMainFrame();

  // 2. Create params for a popup without a user gesture.
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_POPUP,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  params.user_gesture = false;
  params.source_render_process_id =
      source_rfh->GetProcess()->GetID().GetUnsafeValue();
  params.source_render_frame_id = source_rfh->GetRoutingID();

  base::test::TestFuture<GURL> future;
  content::WebContents* result = window->OpenURL(
      params,
      base::BindLambdaForTesting([&](content::NavigationHandle& handle) {
        future.SetValue(handle.GetURL());
      }));

  // The popup should be blocked, so OpenURL should return nullptr.
  EXPECT_EQ(nullptr, result);

  // To verify the callback is NOT called, we need to wait to ensure no async
  // task was posted.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserWindowBrowserTest, OpenURL_PopupAllowed) {
  AndroidBrowserWindow* window = GetBrowserWindow();
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // 1. Navigate to a page first to have a valid source.
  content::OpenURLParams source_params(
      embedded_test_server()->GetURL("/simple.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);

  base::test::TestFuture<GURL> source_future;
  content::WebContents* source_contents = window->OpenURL(
      source_params,
      base::BindLambdaForTesting([&](content::NavigationHandle& handle) {
        source_future.SetValue(handle.GetURL());
      }));
  ASSERT_TRUE(source_contents);
  ASSERT_TRUE(source_future.Wait());

  content::RenderFrameHost* source_rfh = source_contents->GetPrimaryMainFrame();

  // 2. Create params for a popup WITH a user gesture.
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_POPUP,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  params.user_gesture = true;
  params.source_render_process_id =
      source_rfh->GetProcess()->GetID().GetUnsafeValue();
  params.source_render_frame_id = source_rfh->GetRoutingID();

  base::test::TestFuture<GURL> future;
  content::WebContents* result = window->OpenURL(
      params,
      base::BindLambdaForTesting([&](content::NavigationHandle& handle) {
        future.SetValue(handle.GetURL());
      }));

  // The popup should be allowed.
  if (result) {
    EXPECT_EQ(url, result->GetVisibleURL());
  }

  // Callback should eventually be called.
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(url, future.Get());
}
