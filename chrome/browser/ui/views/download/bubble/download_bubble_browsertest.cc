// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "url/gurl.h"

class DownloadBubbleTest : public InProcessBrowserTest {
 public:
  DownloadBubbleTest() {
    test_features_.InitAndEnableFeatures(
        {feature_engagement::kIPHDownloadToolbarButtonFeature,
         safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2},
        {});
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void DownloadAndWait(const GURL& url) {
    content::DownloadTestObserverTerminal observer(
        browser()->profile()->GetDownloadManager(), 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    observer.WaitForFinished();
  }

  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

// Download bubble does not exist in Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(DownloadBubbleTest, IPHWithNoInteraction) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
      browser_view->GetFeaturePromoController()));

  GURL url = embedded_test_server()->GetURL("/download-test1.lib");
  DownloadAndWait(url);

  browser_view->toolbar()->download_button()->CloseDialog(
      views::Widget::ClosedReason::kLostFocus);

  EXPECT_TRUE(browser_view->IsFeaturePromoActive(
      feature_engagement::kIPHDownloadToolbarButtonFeature));
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleTest, NoIPHWithInteraction) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
      browser_view->GetFeaturePromoController()));

  // Simulate an interaction with the download bubble
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser()->profile());
  tracker->NotifyEvent("download_bubble_interaction");

  GURL url = embedded_test_server()->GetURL("/download-test1.lib");
  DownloadAndWait(url);

  browser_view->toolbar()->download_button()->CloseDialog(
      views::Widget::ClosedReason::kLostFocus);

  EXPECT_FALSE(browser_view->IsFeaturePromoActive(
      feature_engagement::kIPHDownloadToolbarButtonFeature));
}
#endif
