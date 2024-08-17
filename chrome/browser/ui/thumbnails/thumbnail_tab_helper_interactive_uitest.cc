// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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

class BrowserRemovedObserver : public ui::test::StateObserver<bool>,
                               public BrowserListObserver {
 public:
  explicit BrowserRemovedObserver(Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
  }
  ~BrowserRemovedObserver() override = default;

 protected:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser_ == browser) {
      OnStateObserverStateChanged(true);
      browser_ = nullptr;
      BrowserList::RemoveObserver(this);
    }
  }

 private:
  raw_ptr<Browser> browser_;
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ThumbnailObserver, kThumbnailCreatedState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserRemovedObserver,
                                    kBrowserRemovedState);

}  // anonymous namespace

class ThumbnailTabHelperUpdatedInteractiveTest
    : public MemorySaverInteractiveTestMixin<InteractiveBrowserTest> {
 public:
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
    InteractiveBrowserTest::SetUp();
  }

  int GetTabCount() { return browser()->tab_strip_model()->count(); }

  Browser* GetBrowser(int browser_index) {
    return BrowserList::GetInstance()->get(browser_index);
  }

  auto CheckTabHasThumbnailData(int tab_index, bool has_data) {
    return CheckResult(
        [=, this]() {
          return ThumbnailTabHelper::FromWebContents(
                     browser()->tab_strip_model()->GetWebContentsAt(tab_index))
              ->thumbnail()
              ->has_data();
        },
        has_data,
        base::StrCat({"Checking that tab ", base::NumberToString(tab_index),
                      (has_data ? " has" : " doesn't have"), " data"}));
  }

  auto WaitForAndVerifyThumbnail(int tab_index, int browser_index = 0) {
    return Steps(
        ObserveState(
            kThumbnailCreatedState,
            [tab_index, browser_index]() {
              auto* browser = BrowserList::GetInstance()->get(browser_index);
              return browser->tab_strip_model()->GetWebContentsAt(tab_index);
            }),
        WaitForState(kThumbnailCreatedState, true));
  }

  auto VerifyTabIsNotLoadedAndNeedsReloading(int tab_index, int browser_index) {
    return Check(
        [tab_index, browser_index]() {
          auto* contents = BrowserList::GetInstance()
                               ->get(browser_index)
                               ->tab_strip_model()
                               ->GetWebContentsAt(tab_index);
          return !contents->IsLoading() &&
                 contents->GetController().NeedsReload();
        },
        base::StrCat({"Checking that tab ", base::NumberToString(tab_index),
                      " is not loaded and needs reloading"}));
  }

  auto CheckTabCountInBrowserIndex(int browser_index, int count) {
    return CheckResult(
        [this, browser_index]() {
          return GetBrowser(browser_index)->tab_strip_model()->count();
        },
        count,
        base::StrCat({"Checking that there are ", base::NumberToString(count),
                      " tabs"}));
  }

  auto CheckActiveTabInBrowserIndex(int browser_index, int active_index) {
    return CheckResult(
        [this, browser_index]() {
          return GetBrowser(browser_index)->tab_strip_model()->active_index();
        },
        active_index,
        base::StrCat({"Checking that the active tab is in index ",
                      base::NumberToString(active_index)}));
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
IN_PROC_BROWSER_TEST_F(ThumbnailTabHelperUpdatedInteractiveTest,
                       CapturesRestoredTabWhenRequested) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  RunTestSequence(
      InContext(GetBrowser(1)->window()->GetElementContext(),
                Steps(AddInstrumentedTab(kFirstTab,
                                         GURL(chrome::kChromeUINewTabURL), 1),
                      WaitForWebContentsReady(kFirstTab),
                      AddInstrumentedTab(kSecondTab,
                                         GURL(chrome::kChromeUINewTabURL), 2),
                      WaitForWebContentsReady(kSecondTab),
                      AddInstrumentedTab(kThirdTab,
                                         GURL(chrome::kChromeUINewTabURL), 3),
                      WaitForWebContentsReady(kThirdTab))),
      CheckTabCountInBrowserIndex(1, 4), CheckActiveTabInBrowserIndex(1, 3),
      ObserveState(kBrowserRemovedState, [this]() { return GetBrowser(1); }),
      // Can't close browser when WebContents is notifying observers.
      Do([this]() {
        // Override manual value set in MemorySaverInteractiveTestMixin to
        // prepare for tab strip being destroyed along with the browser.
        resource_coordinator::GetTabLifecycleUnitSource()
            ->SetFocusedTabStripModelForTesting(nullptr);
        GetBrowser(1)->window()->Close();
      }),
      WaitForState(kBrowserRemovedState, true), Do([this]() {
        // Set up the tab loader to ensure tabs are left unloaded.
        base::RepeatingCallback<void(TabLoader*)> callback =
            base::BindRepeating(
                &ThumbnailTabHelperUpdatedInteractiveTest::ConfigureTabLoader,
                base::Unretained(this));
        TabLoaderTester::SetConstructionCallbackForTesting(&callback);

        // Restore recently closed window.
        chrome::OpenWindowWithRestoredTabs(browser()->profile());
      }),
      CheckTabCountInBrowserIndex(1, 4), CheckActiveTabInBrowserIndex(1, 3),
      VerifyTabIsNotLoadedAndNeedsReloading(1, 1),
      VerifyTabIsNotLoadedAndNeedsReloading(2, 1),
      WaitForAndVerifyThumbnail(1, 1));

  // Clean up the callback.
  TabLoaderTester::SetConstructionCallbackForTesting(nullptr);
}

#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
