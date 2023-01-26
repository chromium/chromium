// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "content/public/test/browser_task_environment.h"
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

class HistoryClustersHandlerTest : public testing::Test {
 public:
  HistoryClustersHandlerTest() {
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_,
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  }

  // Everything must be called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
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
      QueryClustersResultToMojom(&profile_, "query", clusters, true, false);

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

// TODO(manukh) Add a test case for `VisitToMojom`.

}  // namespace

}  // namespace history_clusters
