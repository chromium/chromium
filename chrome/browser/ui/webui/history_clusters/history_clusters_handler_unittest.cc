// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

history::ClusterVisit CreateVisit(
    std::string url,
    float score,
    std::vector<std::string> related_searches = {}) {
  history::ClusterVisit visit;
  visit.annotated_visit = {
      {GURL{url}, 0}, {}, {}, {}, 0, 0, history::VisitSource::SOURCE_BROWSED};
  visit.annotated_visit.content_annotations.related_searches = related_searches;
  visit.score = score;
  visit.normalized_url = GURL{url};
  return visit;
}

class HistoryClustersHandlerTest : public BrowserWithTestWindowTest {
 public:
  HistoryClustersHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<HistoryClustersHandler>(
        mojo::PendingReceiver<history_clusters::mojom::PageHandler>(),
        profile(), web_contents_.get());
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  HistoryClustersHandler& handler() { return *handler_; }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        {HistoryClustersServiceFactory::GetInstance(),
         HistoryClustersServiceFactory::GetDefaultFactory()},
        {HistoryServiceFactory::GetInstance(),
         HistoryServiceFactory::GetDefaultFactory()},
        {TemplateURLServiceFactory::GetInstance(),
         base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}};
  }

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<HistoryClustersHandler> handler_;
};

// Just a basic test that we transform the data to mojom. A lot of the meat of
// the visit hiding logic is within QueryClustersState and HistoryClustersUtil.
TEST_F(HistoryClustersHandlerTest, QueryClustersResultToMojom_Integration) {
  std::vector<history::Cluster> clusters;

  history::Cluster cluster;
  cluster.cluster_id = 4;
  cluster.related_searches = {"one", "two", "three", "four", "five"};
  cluster.visits.push_back(CreateVisit("https://low-score-1", .4));
  cluster.visits.push_back(CreateVisit("https://low-score-1", .4));

  clusters.push_back(cluster);

  mojom::QueryResultPtr mojom_result =
      QueryClustersResultToMojom(profile(), "query", clusters, true, false);

  EXPECT_EQ(mojom_result->query, "query");
  EXPECT_EQ(mojom_result->can_load_more, true);
  EXPECT_EQ(mojom_result->is_continuation, false);

  ASSERT_EQ(mojom_result->clusters.size(), 1u);
  const auto& cluster_mojom = mojom_result->clusters[0];

  EXPECT_EQ(cluster_mojom->id, 4);
  const auto& visits = cluster_mojom->visits;
  ASSERT_EQ(visits.size(), 2u);
  // Test that the hidden attribute is passed through to mojom.
  ASSERT_EQ(cluster_mojom->related_searches.size(), 5u);
  EXPECT_EQ(cluster_mojom->related_searches[0]->query, "one");
  EXPECT_EQ(cluster_mojom->related_searches[1]->query, "two");
  EXPECT_EQ(cluster_mojom->related_searches[2]->query, "three");
  EXPECT_EQ(cluster_mojom->related_searches[3]->query, "four");
  EXPECT_EQ(cluster_mojom->related_searches[4]->query, "five");
}

TEST_F(HistoryClustersHandlerTest, OpenVisitUrlsInTabGroup) {
  std::vector<mojom::URLVisitPtr> visits;
  visits.push_back(mojom::URLVisit::New());
  visits.back()->normalized_url = GURL("http://www.google.com/search?q=foo");
  visits.push_back(mojom::URLVisit::New());
  visits.back()->normalized_url = GURL("http://foo/1");
  handler().OpenVisitUrlsInTabGroup(std::move(visits), "My Tab Group Name");

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(2u, static_cast<size_t>(tab_strip_model->GetTabCount()));
  ASSERT_EQ(GURL("http://www.google.com/search?q=foo"),
            tab_strip_model->GetWebContentsAt(0)->GetURL());
  ASSERT_EQ(GURL("http://foo/1"),
            tab_strip_model->GetWebContentsAt(1)->GetURL());

  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  ASSERT_EQ(1u, tab_group_model->ListTabGroups().size());
  ASSERT_EQ(0, tab_strip_model->GetIndexOfWebContents(
                   tab_strip_model->GetActiveWebContents()));
  auto* tab_group =
      tab_group_model->GetTabGroup(tab_group_model->ListTabGroups()[0]);
  ASSERT_TRUE(tab_group);
  EXPECT_EQ(tab_group->visual_data()->title(), u"My Tab Group Name");
}

}  // namespace

}  // namespace history_clusters
