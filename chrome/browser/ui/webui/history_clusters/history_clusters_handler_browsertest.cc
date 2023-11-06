// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"

namespace history_clusters {

class HistoryClustersHandlerBrowserTest : public InProcessBrowserTest {
 public:
  HistoryClustersHandlerBrowserTest() {
    feature_list_.InitWithFeatures({history_clusters::internal::kJourneys}, {});
  }
  ~HistoryClustersHandlerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetInteger(
        history_clusters::prefs::kLastSelectedTab,
        history_clusters::prefs::TabbedPage::GROUP);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(GetChromeUIHistoryClustersURL())));
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
  raw_ptr<HistoryClustersHandler, AcrossTasksDanglingUntriaged> handler_;

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

// TODO(https://crbug.com/1335515): Flaky.
IN_PROC_BROWSER_TEST_F(HistoryClustersHandlerBrowserTest,
                       DISABLED_OpenVisitUrlsInTabGroupHardCap) {
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

IN_PROC_BROWSER_TEST_F(HistoryClustersHandlerBrowserTest,
                       RecordUIVisitActions) {
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->GetTabCount());

  base::HistogramTester histogram_tester;

  handler_->RecordVisitAction(mojom::VisitAction::kClicked, 0,
                              mojom::VisitType::kNonSRP);
  histogram_tester.ExpectBucketCount("History.Clusters.UIActions.Visit.Clicked",
                                     0, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.UIActions.nonSRPVisit.Clicked", 0, 1);

  handler_->RecordVisitAction(mojom::VisitAction::kDeleted, 0,
                              mojom::VisitType::kSRP);
  histogram_tester.ExpectBucketCount("History.Clusters.UIActions.Visit.Deleted",
                                     0, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.UIActions.SRPVisit.Deleted", 0, 1);

  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip_model->GetWebContentsAt(0));
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.WasSuccessful", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.NumberLinksOpened", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.NumberIndividualVisitsDeleted", 1,
      1);
}

IN_PROC_BROWSER_TEST_F(HistoryClustersHandlerBrowserTest,
                       RecordUIClusterActions) {
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->GetTabCount());

  base::HistogramTester histogram_tester;

  handler_->RecordClusterAction(mojom::ClusterAction::kVisitClicked, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.UIActions.Cluster.VisitClicked", 0, 1);
  handler_->RecordClusterAction(mojom::ClusterAction::kDeleted, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.UIActions.Cluster.Deleted", 0, 1);

  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip_model->GetWebContentsAt(0));
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.WasSuccessful", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.NumberClustersDeleted", 1, 1);
}

IN_PROC_BROWSER_TEST_F(HistoryClustersHandlerBrowserTest,
                       RecordUIRelatedSearchActions) {
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->GetTabCount());

  base::HistogramTester histogram_tester;

  handler_->RecordRelatedSearchAction(mojom::RelatedSearchAction::kClicked, 0);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.UIActions.RelatedSearch.Clicked", 0, 1);

  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip_model->GetWebContentsAt(0));
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.WasSuccessful", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.NumberRelatedSearchesClicked", 1, 1);
}

IN_PROC_BROWSER_TEST_F(HistoryClustersHandlerBrowserTest,
                       RecordUnsuccessfulOutcome) {
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->GetTabCount());

  base::HistogramTester histogram_tester;

  handler_->RecordToggledVisibility(false);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.UIActions.ToggledVisiblity", false, 1);

  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip_model->GetWebContentsAt(0));
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.WasSuccessful", false, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Actions.FinalState.NumberVisibilityToggles", 1, 1);
}

}  // namespace history_clusters
