// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/session_restore_policy.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/sessions/tab_loader_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_interactive_test_mixin.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/state_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/tab_loader.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

namespace {

class ThumbnailWaiter {
 public:
  ThumbnailWaiter() = default;
  ~ThumbnailWaiter() = default;

  std::optional<gfx::ImageSkia> WaitForThumbnail(ThumbnailImage* thumbnail) {
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
  std::optional<gfx::ImageSkia> image_;
};

// Replacement for `ThumbnailWaiter` which will be used in tests that are
// migrated to Kombucha. This uses the StateObserver pattern to detect
// that the thumbnail has been updated.
class ThumbnailObserver : public ui::test::StateObserver<bool> {
 public:
  explicit ThumbnailObserver(content::WebContents* web_contents) {
    auto* const thumbnail_tab_helper =
        ThumbnailTabHelper::FromWebContents(web_contents);
    auto* thumbnail = thumbnail_tab_helper->thumbnail().get();

    subscription_ = thumbnail->Subscribe();
    subscription_->SetUncompressedImageCallback(
        base::BindRepeating(&ThumbnailObserver::ThumbnailImageCallback,
                            weak_ptr_factory_.GetWeakPtr()));
    thumbnail->RequestThumbnailImage();
  }
  ~ThumbnailObserver() override = default;

 protected:
  void ThumbnailImageCallback(gfx::ImageSkia thumbnail_image) {
    OnStateObserverStateChanged(!thumbnail_image.isNull());
    subscription_ = nullptr;
  }

 private:
  std::unique_ptr<ThumbnailImage::Subscription> subscription_;
  base::WeakPtrFactory<ThumbnailObserver> weak_ptr_factory_{this};
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ThumbnailObserver, kThumbnailCreatedState);

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
        !tab->IsLoading()) {
      return;
    }

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
    const std::optional<gfx::ImageSkia> data =
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

// Updated test fixture for testing interaction of thumbnail tab helper and
// browser, specifically testing interaction of tab load and thumbnail capture.
class ThumbnailTabHelperUpdatedInteractiveTest
    : public MemorySaverInteractiveTestMixin<InteractiveBrowserTest> {
 protected:
  void SetUp() override {
    // This flag causes the thumbnail tab helper system to engage. Otherwise
    // there is no ThumbnailTabHelper created. Note that there *are* other flags
    // that also trigger the existence of the helper.
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCardImages);
    InteractiveBrowserTest::SetUp();
  }

  int GetTabCount() { return browser()->tab_strip_model()->count(); }

  auto CheckTabHasThumbnailData(int tab_index, bool has_data) {
    return CheckResult(
        [=]() {
          return ThumbnailTabHelper::FromWebContents(
                     browser()->tab_strip_model()->GetWebContentsAt(tab_index))
              ->thumbnail()
              ->has_data();
        },
        has_data,
        base::StrCat({"Checking that tab ", base::NumberToString(tab_index),
                      (has_data ? " has" : " doesn't have"), " data"}));
  }

  auto WaitForAndVerifyThumbnail(int tab_index) {
    return Steps(
        ObserveState(
            kThumbnailCreatedState,
            [this, tab_index]() {
              return this->browser()->tab_strip_model()->GetWebContentsAt(
                  tab_index);
            }),
        WaitForState(kThumbnailCreatedState, true));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ThumbnailTabHelperUpdatedInteractiveTest,
                       TabLoadTriggersScreenshot) {
  RunTestSequence(
      AddInstrumentedTab(kFirstTab, GURL(chrome::kChromeUINewTabURL), 0),
      WaitForWebContentsReady(kFirstTab), CheckTabHasThumbnailData(0, false),
      SelectTab(kTabStripElementId, 1),
      CheckResult([this]() { return GetTabCount(); }, 2,
                  "Checking that there are two tabs"),
      WaitForAndVerifyThumbnail(0), CheckTabHasThumbnailData(0, true));
}

IN_PROC_BROWSER_TEST_F(ThumbnailTabHelperUpdatedInteractiveTest,
                       TabDiscardPreservesScreenshot) {
  RunTestSequence(
      AddInstrumentedTab(kFirstTab, GURL(chrome::kChromeUINewTabURL), 0),
      WaitForWebContentsReady(kFirstTab), CheckTabHasThumbnailData(0, false),
      SelectTab(kTabStripElementId, 1), WaitForAndVerifyThumbnail(0),
      CheckTabHasThumbnailData(0, true), TryDiscardTab(0),
      CheckTabIsDiscarded(0, true), CheckTabHasThumbnailData(0, true));
}

// TabLoader (used here) is available only when browser is built
// with ENABLE_SESSION_SERVICE.
#if BUILDFLAG(ENABLE_SESSION_SERVICE)

// On browser restore, some tabs may not be loaded. Requesting a
// thumbnail for one of these tabs should trigger load and capture.
// TODO(crbug.com/40883117): Flaky on Mac, ChromeOS,
// and various sanitizer builds.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) ||             \
    defined(THREAD_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
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
  const int active_tab_index = browser2->tab_strip_model()->active_index();
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
  EXPECT_EQ(active_tab_index, browser2->tab_strip_model()->active_index());

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
