// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/memories_remote_model_helper.h"
#include "components/history_clusters/core/proto/clusters.pb.h"
#include "components/history_clusters/core/visit_data.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

// Returns a Time that's `milliseconds` milliseconds after Windows epoch.
base::Time IntToTime(int milliseconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(milliseconds));
}

class HistoryClustersServiceTest : public testing::Test {
 public:
  HistoryClustersServiceTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        run_loop_quit_(run_loop_.QuitClosure()) {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    history_clusters_service_ = std::make_unique<HistoryClustersService>(
        history_service_.get(), shared_url_loader_factory_);
    history_clusters_service_test_api_ =
        std::make_unique<HistoryClustersServiceTestApi>(
            history_clusters_service_.get());
  }

  HistoryClustersServiceTest(const HistoryClustersServiceTest&) = delete;
  HistoryClustersServiceTest& operator=(const HistoryClustersServiceTest&) =
      delete;

  void EnableMemoriesWithEndpoint(
      const std::string& endpoint_url = kFakeEndpoint,
      const std::string& endpoint_experiment = "") {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeaturesAndParameters(
        {
            {
                kMemories,
                {},
            },
            {
                kRemoteModelForDebugging,
                {
                    {"MemoriesRemoteModelEndpoint", endpoint_url},
                    {"MemoriesRemoteModelEndpointExperimentName",
                     endpoint_experiment},
                },
            },
        },
        {});
  }

  void AddVisit(int time, const GURL& url) {
    history::AnnotatedVisit visit;
    visit.url_row.set_url(url);
    visit.visit_row.visit_time = IntToTime(time);
    AddVisit(visit);
  }

  void AddVisit(history::URLID url_id,
                const GURL& url,
                const std::u16string title,
                history::VisitID visit_id,
                base::Time visit_time,
                int page_end_reason) {
    history::AnnotatedVisit visit;
    visit.url_row.set_id(url_id);
    visit.url_row.set_url(url);
    visit.url_row.set_title(title);
    visit.visit_row.visit_id = visit_id;
    visit.visit_row.visit_time = visit_time;
    visit.context_annotations.page_end_reason = page_end_reason;
    AddVisit(visit);
  }

  void AddVisit(const history::AnnotatedVisit& visit) {
    history_service_->AddPageWithDetails(
        visit.url_row.url(), visit.url_row.title(), visit.url_row.visit_count(),
        visit.url_row.typed_count(), visit.visit_row.visit_time,
        visit.url_row.hidden(), history::VisitSource::SOURCE_BROWSED);

    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            next_navigation_id_);
    incomplete_visit_context_annotations.visit_row = visit.visit_row;
    incomplete_visit_context_annotations.url_row = visit.url_row;
    incomplete_visit_context_annotations.context_annotations =
        visit.context_annotations;
    incomplete_visit_context_annotations.status.history_rows = true;
    incomplete_visit_context_annotations.status.navigation_ended = true;
    incomplete_visit_context_annotations.status.navigation_end_signals = true;
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(
        next_navigation_id_);
    next_navigation_id_++;
  }

  // Helper to get the most recent remote request body.
  std::string GetPendingRequestBody() {
    const scoped_refptr<network::ResourceRequestBody>& request_body =
        test_url_loader_factory_.GetPendingRequest(0)->request.request_body;
    const network::DataElement& element = (*request_body->elements())[0];
    return std::string(element.As<network::DataElementBytes>().AsStringPiece());
  }

  // Verifies that that a particular hardcoded request is in a pending request
  // within the URL loader.
  void VerifyHardcodedTestDataInUrlLoaderRequest(
      const std::string& expected_experiment_name = "") {
    EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));
    proto::GetClustersRequest request;
    request.set_experiment_name(expected_experiment_name);
    auto* visit = request.add_visits();
    visit->set_visit_id(2);
    visit->set_navigation_time_ms(2);
    visit->set_origin("https://google.com/");
    visit->set_page_end_reason(3);
    visit->set_url("https://google.com/");
    visit = request.add_visits();
    visit->set_visit_id(4);
    visit->set_navigation_time_ms(4);
    visit->set_origin("https://github.com/");
    visit->set_page_end_reason(5);
    visit->set_url("https://github.com/");

    std::string encoded;
    base::Base64Encode(request.SerializeAsString(), &encoded);
    std::string expected_request_body =
        base::StringPrintf("{\"data\":\"%s\"}", encoded.c_str());

    EXPECT_EQ(GetPendingRequestBody(), expected_request_body);
  }

  // Fakes a particular partly hardcoded response from the URL loader.
  void InjectHardcodedTestDataToUrlLoaderResponse(
      std::vector<std::vector<int>> clustered_visit_ids) {
    proto::GetClustersResponse response;
    for (auto visit_ids : clustered_visit_ids) {
      auto* cluster = response.add_clusters();
      for (auto visit_id : visit_ids)
        cluster->add_visit_ids(visit_id);
    }
    if (!clustered_visit_ids.empty()) {
      response.mutable_clusters(0)->mutable_keywords()->Add("apples");
      // We had a bug where we couldn't match against uppercase keywords,
      // so we therefore want to test against an uppercase keyword.
      response.mutable_clusters(0)->mutable_keywords()->Add("Red Oranges");
    }
    test_url_loader_factory_.AddResponse(kFakeEndpoint,
                                         response.SerializeAsString());
    EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  }

  base::test::TaskEnvironment task_environment_;

  // Used to construct a `HistoryClustersService`.
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;

  static constexpr char kFakeEndpoint[] = "https://endpoint.com/";
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<HistoryClustersService> history_clusters_service_;
  std::unique_ptr<HistoryClustersServiceTestApi>
      history_clusters_service_test_api_;

  base::CancelableTaskTracker task_tracker_;

  // Used to verify the async callback is invoked.
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;

  // Tracks the next available navigation ID to be associated with visits.
  int64_t next_navigation_id_ = 0;
};

// Useless, but required by the C++14 standard. Please deliver us, C++17.
constexpr char HistoryClustersServiceTest::kFakeEndpoint[];

TEST_F(HistoryClustersServiceTest, QueryMemoriesVariousQueries) {
  std::string experiment_name = "someExperiment";
  EnableMemoriesWithEndpoint(kFakeEndpoint, experiment_name);

  AddVisit(0, GURL{"https://google.com"}, u"Google title", 2, IntToTime(2), 3);
  AddVisit(0, GURL{"https://github.com"}, u"Github title", 4, IntToTime(4), 5);

  struct TestData {
    std::string query;
    const bool expect_first_cluster;
    const bool expect_second_cluster;
  } test_data[] = {
      // Empty query should get both.
      {"", true, true},
      // Non matching query should get none.
      {"non_matching_query", false, false},
      // Query matching one cluster.
      {"oran", true, false},
      // This verifies the memory doesn't flicker away as the user is typing
      // out: "red oran" one key at a time. Also tests out multi-term queries.
      {"red", true, false},
      {"red ", true, false},
      {"red o", true, false},
      {"red or", true, false},
      {"red ora", true, false},
      {"red oran", true, false},
  };

  for (size_t i = 0; i < base::size(test_data); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case i=%d, query=%s", int(i),
                                    test_data[i].query.c_str()));

    base::RunLoop run_loop;
    auto run_loop_quit = run_loop.QuitClosure();

    test_url_loader_factory_.ClearResponses();
    ASSERT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

    auto query_params = mojom::QueryParams::New();
    query_params->query = test_data[i].query;
    history_clusters_service_->QueryMemories(
        std::move(query_params),
        // This "expect" block is not run until after the fake response is sent
        // further down in this method.
        base::BindLambdaForTesting(
            [&](HistoryClustersService::QueryMemoriesResponse response) {
              // Verify that the continuation query params is nullptr.
              ASSERT_FALSE(!!response.query_params);

              size_t expected_size = int(test_data[i].expect_first_cluster) +
                                     int(test_data[i].expect_second_cluster);
              ASSERT_EQ(response.clusters.size(), expected_size);

              if (test_data[i].expect_first_cluster) {
                const auto& cluster = response.clusters[0];
                EXPECT_FALSE(cluster->id.is_empty());
                ASSERT_EQ(cluster->top_visits.size(), 2u);
                EXPECT_EQ(cluster->top_visits[0]->id, 2);
                EXPECT_EQ(cluster->top_visits[0]->url, "https://google.com/");
                EXPECT_EQ(cluster->top_visits[0]->time, IntToTime(2));
                EXPECT_EQ(cluster->top_visits[0]->page_title, "Google title");
                EXPECT_EQ(cluster->top_visits[1]->id, 4);
                EXPECT_EQ(cluster->top_visits[1]->url, "https://github.com/");
                EXPECT_EQ(cluster->top_visits[1]->time, IntToTime(4));
                EXPECT_EQ(cluster->top_visits[1]->page_title, "Github title");
                ASSERT_EQ(cluster->keywords.size(), 2u);
                EXPECT_EQ(cluster->keywords[0], u"apples");
                EXPECT_EQ(cluster->keywords[1], u"Red Oranges");
              }

              if (test_data[i].expect_second_cluster) {
                const auto& cluster = test_data[i].expect_first_cluster
                                          ? response.clusters[1]
                                          : response.clusters[0];
                EXPECT_FALSE(cluster->id.is_empty());
                ASSERT_EQ(cluster->top_visits.size(), 1u);
                EXPECT_EQ(cluster->top_visits[0]->id, 4);
                EXPECT_EQ(cluster->top_visits[0]->url, "https://github.com/");
                EXPECT_EQ(cluster->top_visits[0]->time, IntToTime(4));
                EXPECT_EQ(cluster->top_visits[0]->page_title, "Github title");
                EXPECT_TRUE(cluster->keywords.empty());
              }

              run_loop_quit.Run();
            }),
        &task_tracker_);

    VerifyHardcodedTestDataInUrlLoaderRequest(experiment_name);
    InjectHardcodedTestDataToUrlLoaderResponse({{2, 4}, {4}});

    // Verify the callback is invoked.
    run_loop.Run();
  }
}

TEST_F(HistoryClustersServiceTest, QueryMemoriesWithEmptyVisits) {
  EnableMemoriesWithEndpoint();

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify that the continuation query params is nullptr.
            ASSERT_FALSE(!!response.query_params);
            // Verify the parsed response.
            EXPECT_TRUE(response.clusters.empty());
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  // Verify no request is made.
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, QueryMemoriesWithEmptyEndpoint) {
  EnableMemoriesWithEndpoint("");

  AddVisit(1, GURL{"google.com"});
  AddVisit(2, GURL{"github.com"});

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify that the continuation query params is nullptr.
            ASSERT_FALSE(!!response.query_params);
            // Verify the empty response.
            EXPECT_TRUE(response.clusters.empty());
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  // Verify no request is made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, QueryMemoriesWithEmptyResponse) {
  EnableMemoriesWithEndpoint();

  AddVisit(1, GURL{"google.com"});
  AddVisit(2, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify that the continuation query params is nullptr.
            ASSERT_FALSE(!!response.query_params);
            // Verify the parsed response.
            EXPECT_TRUE(response.clusters.empty());
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Fake an empty but valid response from the endpoint.
  test_url_loader_factory_.AddResponse(
      kFakeEndpoint, proto::GetClustersResponse().SerializeAsString());
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, QueryMemoriesWithInvalidJsonResponse) {
  EnableMemoriesWithEndpoint();

  AddVisit(1, GURL{"google.com"});
  AddVisit(2, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify that the continuation query params is nullptr.
            ASSERT_FALSE(!!response.query_params);
            // Verify the parsed response.
            EXPECT_TRUE(response.clusters.empty());
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Fake a junk response from the endpoint.
  test_url_loader_factory_.AddResponse(kFakeEndpoint,
                                       "{waka404woko.weke) !*(&,");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, QueryMemoriesWithEmptyJsonResponse) {
  EnableMemoriesWithEndpoint();

  AddVisit(1, GURL{"google.com"});
  AddVisit(2, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify that the continuation query params is nullptr.
            ASSERT_FALSE(!!response.query_params);
            // Verify the parsed response.
            EXPECT_TRUE(response.clusters.empty());
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Fake an empty but valid response from the endpoint.
  test_url_loader_factory_.AddResponse(
      kFakeEndpoint, proto::GetClustersResponse().SerializeAsString());
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, QueryMemoriesWithPendingRequest) {
  EnableMemoriesWithEndpoint();

  AddVisit(1, GURL{"google.com"});
  AddVisit(2, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify that the continuation query params is nullptr.
            ASSERT_FALSE(!!response.query_params);
            // Verify the parsed response.
            EXPECT_EQ(response.clusters.size(), 2u);
          }),
      &task_tracker_);

  // Verify there's a single request to the endpoint.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify that the continuation query params is nullptr.
            ASSERT_FALSE(!!response.query_params);
            // Verify the parsed response.
            EXPECT_EQ(response.clusters.size(), 2u);
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  // Verify there are two requests to the endpoint.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);

  // Fake a response from the endpoint with two clusters.
  proto::GetClustersResponse response;
  response.add_clusters();
  response.add_clusters();
  test_url_loader_factory_.AddResponse(kFakeEndpoint,
                                       response.SerializeAsString());
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify both callbacks are invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, QueryMemoriesWithHistoryDb) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              kMemories,
              {{"MemoriesPersistContextAnnotationsInHistoryDb", "true"}},
          },
          {
              kRemoteModelForDebugging,
              {{"MemoriesRemoteModelEndpoint", kFakeEndpoint}},
          },
      },
      {});

  // Must not be too old otherwise the history layer will ignore the visit.
  const auto visit_time = base::Time::Now() - base::TimeDelta::FromDays(1);
  AddVisit(1, GURL{"https://google.com"}, u"Google title", 1, visit_time, 3);
  AddVisit(2, GURL{"https://github.com"}, u"Github title", 2, visit_time, 5);

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      // This "expect" block is not run until after the fake response is sent
      // further down in this method.
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify the parsed response.
            ASSERT_EQ(response.clusters.size(), 2u);
            EXPECT_FALSE(response.clusters[0]->id.is_empty());
            ASSERT_EQ(response.clusters[0]->top_visits.size(), 2u);
            EXPECT_EQ(response.clusters[0]->top_visits[0]->id, 1);
            EXPECT_EQ(response.clusters[0]->top_visits[0]->url,
                      "https://google.com/");
            EXPECT_EQ(response.clusters[0]->top_visits[0]->time, visit_time);
            EXPECT_EQ(response.clusters[0]->top_visits[0]->page_title,
                      "Google title");
            EXPECT_EQ(response.clusters[0]->top_visits[1]->id, 2);
            EXPECT_EQ(response.clusters[0]->top_visits[1]->url,
                      "https://github.com/");
            EXPECT_EQ(response.clusters[0]->top_visits[1]->time, visit_time);
            EXPECT_EQ(response.clusters[0]->top_visits[1]->page_title,
                      "Github title");
            ASSERT_EQ(response.clusters[1]->top_visits.size(), 1u);
            EXPECT_FALSE(response.clusters[1]->id.is_empty());
            EXPECT_EQ(response.clusters[1]->top_visits[0]->id, 2);
            EXPECT_EQ(response.clusters[1]->top_visits[0]->url,
                      "https://github.com/");
            EXPECT_EQ(response.clusters[1]->top_visits[0]->time, visit_time);
            EXPECT_EQ(response.clusters[1]->top_visits[0]->page_title,
                      "Github title");
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  // Verify there's a single request to the endpoint.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Fake a response from the endpoint with two clusters.
  InjectHardcodedTestDataToUrlLoaderResponse({{1, 2}, {2}});

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest,
       QueryMemoriesWithHistoryDbWithPendingRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {
              kMemories,
              {{"MemoriesPersistContextAnnotationsInHistoryDb", "true"}},
          },
          {
              kRemoteModelForDebugging,
              {{"MemoriesRemoteModelEndpoint", kFakeEndpoint}},
          },
      },
      {});

  // Must not be too old otherwise the history layer will ignore the visit.
  const auto visit_time = base::Time::Now() - base::TimeDelta::FromDays(1);
  AddVisit(1, GURL{"https://google.com"}, u"Google title", 1, visit_time, 3);
  AddVisit(2, GURL{"https://github.com"}, u"Github title", 2, visit_time, 5);

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            ADD_FAILURE() << "This should not be reached.";
          }),
      &task_tracker_);

  // Verify there are no requests to the endpoint just yet.
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Cancel pending queries, if any.
  task_tracker_.TryCancelAll();

  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  history_clusters_service_->QueryMemories(
      mojom::QueryParams::New(),
      base::BindLambdaForTesting(
          [&](HistoryClustersService::QueryMemoriesResponse response) {
            // Verify the parsed response.
            EXPECT_EQ(response.clusters.size(), 2u);
            run_loop_quit_.Run();
          }),
      &task_tracker_);

  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  // Verify there's a single request to the endpoint.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Fake a response from the endpoint with two clusters.
  InjectHardcodedTestDataToUrlLoaderResponse({{1, 2}, {2}});

  // Verify the last callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, CompleteVisitContextAnnotationsIfReady) {
  auto test = [&](RecordingStatus status, bool expected_complete) {
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.status = status;
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
    EXPECT_NE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0),
        expected_complete);
  };

  // Complete cases:
  {
    SCOPED_TRACE("Complete without UKM");
    test({true, true, true}, true);
  }
  {
    SCOPED_TRACE("Complete with UKM");
    test({true, true, true, true, true}, true);
  }

  // Incomplete without UKM cases:
  {
    SCOPED_TRACE("Incomplete, missing history rows");
    test({false, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation hasn't ended");
    test({true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation end metrics haven't been recorded");
    test({true, true}, false);
  }

  // Incomplete with UKM cases:
  {
    SCOPED_TRACE("Incomplete, missing history rows");
    test({false, true, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation hasn't ended");
    test({true, false, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation end metrics haven't been recorded");
    test({true, true, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, UKM page end missing");
    test({true, true, true, true, false}, false);
  }

  auto test_dcheck = [&](RecordingStatus status) {
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.status = status;
    EXPECT_DCHECK_DEATH(
        history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0));
    EXPECT_TRUE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
  };

  // Impossible cases:
  {
    SCOPED_TRACE(
        "Impossible, navigation end signals recorded before navigation ended");
    test_dcheck({true, false, true});
  }
  {
    SCOPED_TRACE(
        "Impossible, navigation end signals recorded before history rows");
    test_dcheck({false, true, true});
  }
  {
    SCOPED_TRACE("Impossible, unexpected UKM page end recorded");
    test_dcheck({false, false, false, false, true});
  }
}

TEST_F(HistoryClustersServiceTest,
       CompleteVisitContextAnnotationsIfReadyWhenFeatureDisabled) {
  {
    // When the feature is disabled, the `IncompleteVisitContextAnnotations`
    // should be removed but not added to visits.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kMemories);
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.status = {true, true, true};
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
    EXPECT_FALSE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
    EXPECT_TRUE(history_clusters_service_test_api_->GetVisits().empty());
  }

  {
    // When the feature is enabled, the `IncompleteVisitContextAnnotations`
    // should be removed and added to visits.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kMemories);
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.status = {true, true, true};
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
    EXPECT_FALSE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
    EXPECT_EQ(history_clusters_service_test_api_->GetVisits().size(), 1u);
  }
}

}  // namespace

}  // namespace history_clusters
