// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_clusters/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace history_clusters {

class HistoryClustersHandlerBrowserTest : public InProcessBrowserTest {
 public:
  HistoryClustersHandlerBrowserTest() {
    feature_list_.InitWithFeatures({history_clusters::internal::kJourneys}, {});
  }
  ~HistoryClustersHandlerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIHistoryClustersURL)));
    EXPECT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents()));
    handler_ = browser()
                   ->tab_strip_model()
                   ->GetActiveWebContents()
                   ->GetWebUI()
                   ->GetController()
                   ->template GetAs<HistoryUI>()
                   ->GetHistoryClustersHandlerForTesting();
  }

 protected:
  raw_ptr<HistoryClustersHandler> handler_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests whether the handler opens all the given URLs in the same tab group and
// and in the expected order.
IN_PROC_BROWSER_TEST_F(HistoryClustersHandlerBrowserTest,
                       OpenVisitUrlsInTabGroup) {
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->GetTabCount());

  std::vector<mojom::URLVisitPtr> visits;
  auto visit1 = mojom::URLVisit::New();
  visit1->normalized_url = GURL("https://foo");
  visits.push_back(std::move(visit1));
  auto visit2 = mojom::URLVisit::New();
  visit2->normalized_url = GURL("https://bar");
  visits.push_back(std::move(visit2));

  handler_->OpenVisitUrlsInTabGroup(std::move(visits));
  ASSERT_EQ(3, tab_strip_model->GetTabCount());

  ASSERT_EQ(tab_strip_model->GetTabGroupForTab(1).value(),
            tab_strip_model->GetTabGroupForTab(2).value());

  ASSERT_EQ(GURL("https://foo"),
            tab_strip_model->GetWebContentsAt(1)->GetVisibleURL());
  ASSERT_EQ(GURL("https://bar"),
            tab_strip_model->GetWebContentsAt(2)->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(HistoryClustersHandlerBrowserTest,
                       OpenVisitUrlsInTabGroupHardCap) {
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->GetTabCount());

  std::vector<mojom::URLVisitPtr> visits;
  for (size_t i = 0; i < 50; ++i) {
    auto visit = mojom::URLVisit::New();
    visit->normalized_url = GURL("https://foo");
    visits.push_back(std::move(visit));
  }

  // Verify that we open 32 at maximum. Including the NTP, that's 33 total.
  handler_->OpenVisitUrlsInTabGroup(std::move(visits));
  ASSERT_EQ(33, tab_strip_model->GetTabCount());
}

}  // namespace history_clusters
