// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/session_restore_policy.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/sessions/tab_loader_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/tab_loader.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

namespace {

class ThumbnailWaiter {
 public:
  ThumbnailWaiter() = default;
  ~ThumbnailWaiter() = default;

  absl::optional<gfx::ImageSkia> WaitForThumbnail(ThumbnailImage* thumbnail) {
    std::unique_ptr<ThumbnailImage::Subscription> subscription =
        thumbnail->Subscribe();
    subscription->SetUncompressedImageCallback(base::BindRepeating(
        &ThumbnailWaiter::ThumbnailImageCallback, base::Unretained(this)));
    thumbnail->RequestThumbnailImage();
    run_loop_.Run();
    return image_;
  }

 protected:
  void ThumbnailImageCallback(gfx::ImageSkia thumbnail_image) {
    image_ = std::move(thumbnail_image);
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  absl::optional<gfx::ImageSkia> image_;
};

}  // anonymous namespace

// Test fixture for testing interaction of thumbnail tab helper and browser,
// specifically testing interaction of tab load and thumbnail capture.
class ThumbnailTabHelperInteractiveTest : public InProcessBrowserTest {
 public:
  ThumbnailTabHelperInteractiveTest() {
    url1_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot1.html"));
    url2_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot2.html"));
  }

  ThumbnailTabHelperInteractiveTest(const ThumbnailTabHelperInteractiveTest&) =
      delete;
  ThumbnailTabHelperInteractiveTest& operator=(
      const ThumbnailTabHelperInteractiveTest&) = delete;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  void ConfigureTabLoader(TabLoader* tab_loader) {
    TabLoaderTester tester(tab_loader);
    tester.SetMaxSimultaneousLoadsForTesting(1);
    tester.SetMaxLoadedTabCountForTesting(1);
  }
#endif

 protected:
  void SetUp() override {
    // This flag causes the thumbnail tab helper system to engage. Otherwise
    // there is no ThumbnailTabHelper created. Note that there *are* other flags
    // that also trigger the existence of the helper.
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCardImages);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    active_browser_list_ = BrowserList::GetInstance();
  }

  Browser* GetBrowser(int index) {
    CHECK(static_cast<int>(active_browser_list_->size()) > index);
    return active_browser_list_->get(index);
  }

  // Adds tabs to the given browser, all navigated to url1_. Returns
  // the final number of tabs.
  int AddSomeTabs(Browser* browser, int how_many) {
    int starting_tab_count = browser->tab_strip_model()->count();

    for (int i = 0; i < how_many; ++i) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url1_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    int tab_count = browser->tab_strip_model()->count();
    EXPECT_EQ(starting_tab_count + how_many, tab_count);
    return tab_count;
  }

  void EnsureTabLoaded(content::WebContents* tab) {
    content::NavigationController* controller = &tab->GetController();
    if (!controller->NeedsReload() && !controller->GetPendingEntry() &&
        !tab->IsLoading())
      return;

    content::LoadStopObserver(tab).Wait();
  }

  void WaitForAndVerifyThumbnail(Browser* browser, int tab_index) {
    auto* const web_contents =
        browser->tab_strip_model()->GetWebContentsAt(tab_index);
    auto* const thumbnail_tab_helper =
        ThumbnailTabHelper::FromWebContents(web_contents);
    auto thumbnail = thumbnail_tab_helper->thumbnail();
    EXPECT_FALSE(thumbnail->has_data())
        << " tab at index " << tab_index << " already has data.";

    ThumbnailWaiter waiter;
    const absl::optional<gfx::ImageSkia> data =
        waiter.WaitForThumbnail(thumbnail.get());
    EXPECT_TRUE(thumbnail->has_data())
        << " tab at index " << tab_index << " thumbnail has no data.";
    ASSERT_TRUE(data) << " observer for tab at index " << tab_index
                      << " received no thumbnail.";
    EXPECT_FALSE(data->isNull())
        << " tab at index " << tab_index << " generated empty thumbnail.";
  }

  GURL url1_;
  GURL url2_;

  raw_ptr<const BrowserList> active_browser_list_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if defined(THREAD_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
// TODO(crbug.com/1288117, crbug.com/1336124): Flakes on macOS and various
// MSAN/TSAN/ASAN builders.
#define MAYBE_TabLoadTriggersScreenshot DISABLED_TabLoadTriggersScreenshot
#else
#define MAYBE_TabLoadTriggersScreenshot TabLoadTriggersScreenshot
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ThumbnailTabHelperInteractiveTest,
                       MAYBE_TabLoadTriggersScreenshot) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  DCHECK_EQ(2, browser()->tab_strip_model()->count());
  WaitForAndVerifyThumbnail(browser(), 1);
}

// TODO(crbug.com/1399402): Times out on MSan bot.
#if defined(MEMORY_SANITIZER)
#define MAYBE_TabDiscardPreservesScreenshot \
  DISABLED_TabDiscardPreservesScreenshot
#else
#define MAYBE_TabDiscardPreservesScreenshot TabDiscardPreservesScreenshot
#endif
IN_PROC_BROWSER_TEST_F(ThumbnailTabHelperInteractiveTest,
                       MAYBE_TabDiscardPreservesScreenshot) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  DCHECK_EQ(2, browser()->tab_strip_model()->count());
  WaitForAndVerifyThumbnail(browser(), 1);

  content::WebContents* web_contents_to_discard =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
      web_contents_to_discard)
      ->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT);

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_NE(web_contents_to_discard, new_web_contents);
  EXPECT_TRUE(new_web_contents->WasDiscarded());

  auto* const thumbnail_tab_helper =
      ThumbnailTabHelper::FromWebContents(new_web_contents);
  EXPECT_TRUE(thumbnail_tab_helper);
  auto thumbnail = thumbnail_tab_helper->thumbnail();
  EXPECT_TRUE(thumbnail->has_data());
}

// TabLoader (used here) is available only when browser is built
// with ENABLE_SESSION_SERVICE.
#if BUILDFLAG(ENABLE_SESSION_SERVICE)

// On browser restore, some tabs may not be loaded. Requesting a
// thumbnail for one of these tabs should trigger load and capture.
// TODO(crbug.com/1294473, crbug.com/1294473): Flaky on Mac and various
// sanitizer builds.
#if BUILDFLAG(IS_MAC) || defined(THREAD_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_CapturesRestoredTabWhenRequested \
  DISABLED_CapturesRestoredTabWhenRequested
#else
#define MAYBE_CapturesRestoredTabWhenRequested CapturesRestoredTabWhenRequested
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ThumbnailTabHelperInteractiveTest,
                       MAYBE_CapturesRestoredTabWhenRequested) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  Browser* browser2 = GetBrowser(1);

  // Add tabs and close browser.
  constexpr int kTabCount = 4;
  AddSomeTabs(browser2, kTabCount - browser2->tab_strip_model()->count());
  EXPECT_EQ(kTabCount, browser2->tab_strip_model()->count());
  CloseBrowserSynchronously(browser2);

  // Set up the tab loader to ensure tabs are left unloaded.
  base::RepeatingCallback<void(TabLoader*)> callback = base::BindRepeating(
      &ThumbnailTabHelperInteractiveTest::ConfigureTabLoader,
      base::Unretained(this));
  TabLoaderTester::SetConstructionCallbackForTesting(&callback);

  // Restore recently closed window.
  chrome::OpenWindowWithRestoredTabs(browser()->profile());
  ASSERT_EQ(2U, active_browser_list_->size());
  browser2 = GetBrowser(1);

  EXPECT_EQ(kTabCount, browser2->tab_strip_model()->count());
  EXPECT_EQ(kTabCount - 1, browser2->tab_strip_model()->active_index());

  // These tabs shouldn't want to be loaded.
  for (int tab_idx = 1; tab_idx < kTabCount - 1; ++tab_idx) {
    auto* contents = browser2->tab_strip_model()->GetWebContentsAt(tab_idx);
    EXPECT_FALSE(contents->IsLoading());
    EXPECT_TRUE(contents->GetController().NeedsReload());
  }

  // So we now know that tabs 1 and 2 are not [yet] loading.
  // See if the act of observing one causes the thumbnail to be generated.
  WaitForAndVerifyThumbnail(browser2, 1);

  // Clean up the callback.
  TabLoaderTester::SetConstructionCallbackForTesting(nullptr);
}

#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
