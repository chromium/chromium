// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"

#include <algorithm>

#include "base/base_switches.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/browser_created_waiter.h"
#include "components/browser_apis/tab_strip/tab_strip_service_view.h"
#include "components/browser_apis/tab_strip/types/tab.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"

namespace {

class BrowserClosedVerifier : public BrowserCollectionObserver {
 public:
  explicit BrowserClosedVerifier(BrowserWindowInterface* target_browser)
      : target_browser_(target_browser) {
    observation_.Observe(GlobalBrowserCollection::GetInstance());
  }

  void Wait() { run_loop_.Run(); }

  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (browser != target_browser_) {
      return;
    }
    EXPECT_NE(nullptr, TabListInterface::From(browser));
    EXPECT_NE(nullptr, TabStripServiceFeature::From(browser));
    EXPECT_FALSE(
        std::ranges::contains(GetAllBrowserWindowInterfaces(), browser));
    target_browser_ = nullptr;
    run_loop_.Quit();
  }

 private:
  raw_ptr<BrowserWindowInterface> target_browser_;
  base::RunLoop run_loop_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};

}  // namespace

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

  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          url, ui::mojom::WindowOpenDisposition::kCurrentTab,
          ui::PAGE_TRANSITION_LINK);

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
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          url, ui::mojom::WindowOpenDisposition::kNewWindow,
          ui::PAGE_TRANSITION_LINK);

  base::test::TestFuture<GURL> future;
  BrowserCreatedWaiter window_waiter;
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
  ASSERT_TRUE(window_waiter.Wait());
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
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          url, ui::mojom::WindowOpenDisposition::kNewWindow,
          ui::PAGE_TRANSITION_LINK);

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
  content::OpenURLParams source_params =
      content::OpenURLParams::CreateBrowserInitiated(
          embedded_test_server()->GetURL("/simple.html"),
          ui::mojom::WindowOpenDisposition::kCurrentTab,
          ui::PAGE_TRANSITION_TYPED);

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
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          url, ui::mojom::WindowOpenDisposition::kNewPopup,
          ui::PAGE_TRANSITION_LINK);
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
  content::OpenURLParams source_params =
      content::OpenURLParams::CreateBrowserInitiated(
          embedded_test_server()->GetURL("/simple.html"),
          ui::mojom::WindowOpenDisposition::kCurrentTab,
          ui::PAGE_TRANSITION_TYPED);

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
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          url, ui::mojom::WindowOpenDisposition::kNewPopup,
          ui::PAGE_TRANSITION_LINK);
  params.user_gesture = true;
  params.source_render_process_id =
      source_rfh->GetProcess()->GetID().GetUnsafeValue();
  params.source_render_frame_id = source_rfh->GetRoutingID();

  base::test::TestFuture<GURL> future;
  BrowserCreatedWaiter window_waiter;
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
  ASSERT_TRUE(window_waiter.Wait());
}

IN_PROC_BROWSER_TEST_F(
    AndroidBrowserWindowBrowserTest,
    OnBrowserClosed_TabListInterfaceValidAndWindowExcludedFromIterator) {
  AndroidBrowserWindow* window = GetBrowserWindow();
  ASSERT_NE(nullptr, TabListInterface::From(window));
  ASSERT_NE(nullptr, TabStripServiceFeature::From(window));

  BrowserClosedVerifier verifier(window);
  window->GetWindow()->Close();
  verifier.Wait();
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserWindowBrowserTest,
                       TabStripServiceFeatureRegisteredAndReflectsTabs) {
  AndroidBrowserWindow* window = GetBrowserWindow();
  GURL initial_url = embedded_test_server()->GetURL("/simple.html");
  content::OpenURLParams initial_params =
      content::OpenURLParams::CreateBrowserInitiated(
          initial_url, ui::mojom::WindowOpenDisposition::kCurrentTab,
          ui::PAGE_TRANSITION_TYPED);
  content::WebContents* initial_contents =
      window->OpenURL(initial_params, base::DoNothing());
  ASSERT_NE(nullptr, initial_contents);
  ASSERT_TRUE(content::WaitForLoadStop(initial_contents));

  TabStripServiceFeature* feature = TabStripServiceFeature::From(window);
  ASSERT_NE(nullptr, feature);
  ASSERT_NE(nullptr, feature->GetTabStripService());

  tabs_api::TabStripServiceView* view = feature->GetTabStripServiceView();
  ASSERT_NE(nullptr, view);
  EXPECT_EQ(1u, view->GetTabs().size());
  ASSERT_NE(nullptr, view->GetActiveTab());
  EXPECT_EQ(initial_url, view->GetActiveTab()->url);
  const tabs::TabHandle initial_handle = view->GetActiveTab()->ToTabHandle();

  GURL new_tab_url = embedded_test_server()->GetURL("/title1.html");
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          new_tab_url, ui::mojom::WindowOpenDisposition::kNewForegroundTab,
          ui::PAGE_TRANSITION_LINK);
  base::test::TestFuture<GURL> future;
  window->OpenURL(params, base::BindLambdaForTesting(
                              [&](content::NavigationHandle& handle) {
                                future.SetValue(handle.GetURL());
                              }));
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(2u, view->GetTabs().size());
  ASSERT_NE(nullptr, view->GetActiveTab());
  EXPECT_NE(initial_handle, view->GetActiveTab()->ToTabHandle());
  EXPECT_EQ(TabListInterface::From(window)->GetActiveTab()->GetHandle(),
            view->GetActiveTab()->ToTabHandle());
}
