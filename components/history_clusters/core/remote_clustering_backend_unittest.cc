// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/proto/clusters.pb.h"
#include "components/history_clusters/core/remote_clustering_backend.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

namespace {

static constexpr char kFakeExperimentName[] = "FakeExperimentName";
static constexpr char kFakeEndpoint[] = "https://endpoint.com/";

class RemoteClusteringBackendTest : public testing::Test {
 public:
  RemoteClusteringBackendTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        run_loop_quit_(run_loop_.QuitClosure()) {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeaturesAndParameters(
        {
            {
                kJourneys,
                {
                    {"JourneysExperimentName", kFakeExperimentName},
                },
            },
            {
                kRemoteModelForDebugging,
                {
                    {"JourneysRemoteModelEndpoint", kFakeEndpoint},
                },
            },
        },
        {});

    remote_clustering_backend_ = std::make_unique<RemoteClusteringBackend>(
        shared_url_loader_factory_, absl::nullopt);
  }

  RemoteClusteringBackendTest(const RemoteClusteringBackendTest&) = delete;
  RemoteClusteringBackendTest& operator=(const RemoteClusteringBackendTest&) =
      delete;

  // Helper to get the most recent remote request body.
  std::string GetPendingRequestBody() {
    const scoped_refptr<network::ResourceRequestBody>& request_body =
        test_url_loader_factory_.GetPendingRequest(0)->request.request_body;
    const network::DataElement& element = (*request_body->elements())[0];
    return std::string(element.As<network::DataElementBytes>().AsStringPiece());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<RemoteClusteringBackend> remote_clustering_backend_;

  // Used to verify the async callback is invoked.
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;
};

TEST_F(RemoteClusteringBackendTest, EndToEnd) {
  remote_clustering_backend_->GetClusters(
      base::BindLambdaForTesting(
          [&](const std::vector<history::Cluster>& clusters) {
            ASSERT_EQ(clusters.size(), 2u);

            ASSERT_EQ(clusters[0].visits.size(), 2u);
            EXPECT_EQ(clusters[0].visits[0].annotated_visit.url_row.url(),
                      "https://google.com/");
            EXPECT_FLOAT_EQ(clusters[0].visits[0].score, 0.66);
            EXPECT_EQ(clusters[0].visits[1].annotated_visit.url_row.url(),
                      "https://github.com/");
            EXPECT_FLOAT_EQ(clusters[0].visits[1].score, 0.66);

            ASSERT_EQ(clusters[1].visits.size(), 1u);
            EXPECT_EQ(clusters[1].visits[0].annotated_visit.url_row.url(),
                      "https://github.com/");
            EXPECT_FLOAT_EQ(clusters[1].visits[0].score, 0.66);

            run_loop_quit_.Run();
          }),
      GetHardcodedTestVisits());

  // This below block verifies the proto request sent to the remote endpoint.
  {
    EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));

    absl::optional<base::Value> value =
        base::JSONReader::Read(GetPendingRequestBody());
    ASSERT_TRUE(value);
    std::string* encoded = value->FindStringKey("data");
    ASSERT_TRUE(encoded);

    std::string decoded;
    ASSERT_TRUE(base::Base64Decode(*encoded, &decoded));

    proto::GetClustersRequest request;
    ASSERT_TRUE(request.ParseFromString(decoded));

    EXPECT_EQ(request.experiment_name(), kFakeExperimentName);
    ASSERT_EQ(request.visits_size(), 3);

    auto visit = request.visits().at(0);
    EXPECT_EQ(visit.visit_id(), 1);
    EXPECT_EQ(visit.navigation_time_ms(),
              GetHardcodedTestVisits()[0]
                  .visit_row.visit_time.ToDeltaSinceWindowsEpoch()
                  .InMilliseconds());
    EXPECT_EQ(visit.foreground_time_secs(), 5);
    EXPECT_EQ(visit.url(), "https://google.com/");
    EXPECT_EQ(visit.page_end_reason(), 3);

    visit = request.visits().at(1);
    EXPECT_EQ(visit.visit_id(), 2);
    EXPECT_EQ(visit.navigation_time_ms(),
              GetHardcodedTestVisits()[1]
                  .visit_row.visit_time.ToDeltaSinceWindowsEpoch()
                  .InMilliseconds());
    EXPECT_EQ(visit.foreground_time_secs(), 20);
    EXPECT_EQ(visit.url(), "https://github.com/");
    EXPECT_EQ(visit.page_end_reason(), 5);
    // TODO(tommycli): Add back visit.referring_visit_id() check after updating
    //  the HistoryService test methods to support that field.

    // Don't verify the visit 3 because it's a synched visit and would be
    // filtered by `GetAnnotatedVisitsToCluster` in history clusters service in
    // the real world; it's bypassed by these tests so it's we assert all 3
    // visits were sent.
    // TODO(manukh): Once we move the filtering from
    //  `GetAnnotatedVisitsToCluster` to the backend model, we won't need to do
    //  this 'trick' and this test will match the real world; i.e. both cases
    //  will send all 3 visits.
  }

  // This block sends a fake proto response back via the URL loader.
  {
    proto::GetClustersResponse response;

    auto* cluster = response.add_clusters();
    auto* visit = cluster->add_cluster_visits();
    visit->set_visit_id(1);
    visit->set_score(0.66);
    visit = cluster->add_cluster_visits();
    visit->set_visit_id(2);
    visit->set_score(0.66);

    cluster = response.add_clusters();
    visit = cluster->add_cluster_visits();
    visit->set_visit_id(2);
    visit->set_score(0.66);

    test_url_loader_factory_.AddResponse(kFakeEndpoint,
                                         response.SerializeAsString());
    EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  }

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(RemoteClusteringBackendTest, EmptyVisitsRequest) {
  remote_clustering_backend_->GetClusters(
      base::BindLambdaForTesting(
          [&](const std::vector<history::Cluster>& clusters) {
            EXPECT_TRUE(clusters.empty());
            run_loop_quit_.Run();
          }),
      std::vector<history::AnnotatedVisit>());

  // Verify no request is made.
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(RemoteClusteringBackendTest, EmptyResponse) {
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  remote_clustering_backend_->GetClusters(
      base::BindLambdaForTesting(
          [&](const std::vector<history::Cluster>& clusters) {
            EXPECT_TRUE(clusters.empty());
            run_loop_quit_.Run();
          }),
      GetHardcodedTestVisits());

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Fake an empty but valid response from the endpoint.
  test_url_loader_factory_.AddResponse(
      kFakeEndpoint, proto::GetClustersResponse().SerializeAsString());
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(RemoteClusteringBackendTest, InvalidJsonResponse) {
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  remote_clustering_backend_->GetClusters(
      base::BindLambdaForTesting(
          [&](const std::vector<history::Cluster>& clusters) {
            EXPECT_TRUE(clusters.empty());
            run_loop_quit_.Run();
          }),
      GetHardcodedTestVisits());

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Fake a junk response from the endpoint.
  test_url_loader_factory_.AddResponse(kFakeEndpoint,
                                       "{waka404woko.weke) !*(&,");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(RemoteClusteringBackendTest, EmptyJsonResponse) {
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  remote_clustering_backend_->GetClusters(
      base::BindLambdaForTesting(
          [&](const std::vector<history::Cluster>& clusters) {
            EXPECT_TRUE(clusters.empty());
            run_loop_quit_.Run();
          }),
      GetHardcodedTestVisits());

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Fake an empty but valid response from the endpoint.
  test_url_loader_factory_.AddResponse(
      kFakeEndpoint, proto::GetClustersResponse().SerializeAsString());
  EXPECT_FALSE(test_url_loader_factory_.IsPending(kFakeEndpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(RemoteClusteringBackendTest, TwoSimultaneousRequests) {
  remote_clustering_backend_->GetClusters(
      base::BindLambdaForTesting(
          [&](const std::vector<history::Cluster>& clusters) {
            EXPECT_EQ(clusters.size(), 2u);
            // Don't quit the run loop. We want to use the second request's
            // response as the complete-condition for this test.
          }),
      GetHardcodedTestVisits());

  // Verify there's a single request to the endpoint.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kFakeEndpoint));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  remote_clustering_backend_->GetClusters(
      base::BindLambdaForTesting(
          [&](const std::vector<history::Cluster>& clusters) {
            EXPECT_EQ(clusters.size(), 2u);
            run_loop_quit_.Run();
          }),
      GetHardcodedTestVisits());

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

}  // namespace

}  // namespace history_clusters
