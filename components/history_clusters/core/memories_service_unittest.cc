// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_service.h"

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/memories_service_test_api.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Returns a Time that's |milliseconds| milliseconds after Windows epoch.
base::Time IntToTime(int milliseconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(milliseconds));
}

class MemoriesServiceTest : public testing::Test {
 public:
  MemoriesServiceTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        memories_service_(std::make_unique<memories::MemoriesService>(
            nullptr,
            shared_url_loader_factory_)),
        memories_service_test_api_(
            std::make_unique<memories::MemoriesServiceTestApi>(
                memories_service_.get())),
        task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        run_loop_quit_(run_loop_.QuitClosure()) {}

  MemoriesServiceTest(const MemoriesServiceTest&) = delete;
  MemoriesServiceTest& operator=(const MemoriesServiceTest&) = delete;

  void AddVisit(int time, const GURL& url) {
    auto& visit =
        memories_service_->GetOrCreateIncompleteVisit(next_navigation_id_);
    visit.visit_row.visit_time = IntToTime(time);
    visit.url_row.set_url(url);
    visit.status.history_rows = true;
    visit.status.navigation_ended = true;
    visit.status.navigation_end_signals = true;
    memories_service_->CompleteVisitIfReady(next_navigation_id_);
    next_navigation_id_++;
  }

  // Helper to get the most recent remote request body.
  std::string GetPendingRequestBody() {
    const scoped_refptr<network::ResourceRequestBody>& request_body =
        test_url_loader_factory_.GetPendingRequest(0)->request.request_body;
    const network::DataElement& element = (*request_body->elements())[0];
    return std::string(element.As<network::DataElementBytes>().AsStringPiece());
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<memories::MemoriesService> memories_service_;
  std::unique_ptr<memories::MemoriesServiceTestApi> memories_service_test_api_;

  // Used to allow decoding in tests without spinning up an isolated process.
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  // Used to verify the async callback is invoked.
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;

  // Tracks the next available navigation ID to be associated with visits.
  int64_t next_navigation_id_ = 0;
};

TEST_F(MemoriesServiceTest, GetMemories) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories,
      {{memories::kMemoriesRemoteModelEndpointParam, endpoint}});

  AddVisit(0, GURL{"google.com"});
  AddVisit(1, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));
  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        EXPECT_EQ(memories.size(), 2u);
        run_loop_quit_.Run();
      }));

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(endpoint));
  EXPECT_EQ(
      GetPendingRequestBody(),
      R"({"visits":[{"foreground_time_secs":0,"is_from_google_search":false,)"
      R"("navigation_time_ms":0,"origin":"","page_end_reason":0,)"
      R"("page_transition":0,"site_engagement_score":0,"url":"","visitId":0},)"
      R"({"foreground_time_secs":0,"is_from_google_search":false,)"
      R"("navigation_time_ms":0,"origin":"","page_end_reason":0,)"
      R"("page_transition":0,"site_engagement_score":0,"url":"",)"
      R"("visitId":0}]})");

  // Fake a response from the endpoint.
  test_url_loader_factory_.AddResponse(endpoint, R"({"memories": [{}, {}]})");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, GetMemoriesWithEmptyEndpoint) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories, {{memories::kMemoriesRemoteModelEndpointParam, ""}});

  AddVisit(0, GURL{"google.com"});
  AddVisit(1, GURL{"github.com"});

  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        EXPECT_TRUE(memories.empty());
        run_loop_quit_.Run();
      }));

  // Verify no request is made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, GetMemoriesWithEmptyResponse) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories,
      {{memories::kMemoriesRemoteModelEndpointParam, endpoint}});

  AddVisit(0, GURL{"google.com"});
  AddVisit(1, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));
  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        EXPECT_TRUE(memories.empty());
        run_loop_quit_.Run();
      }));

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(endpoint));

  // Fake a response from the endpoint.
  test_url_loader_factory_.AddResponse(endpoint, "");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, GetMemoriesWithInvalidJsonResponse) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories,
      {{memories::kMemoriesRemoteModelEndpointParam, endpoint}});

  AddVisit(0, GURL{"google.com"});
  AddVisit(1, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));
  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        EXPECT_TRUE(memories.empty());
        run_loop_quit_.Run();
      }));

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(endpoint));

  // Fake a response from the endpoint.
  test_url_loader_factory_.AddResponse(endpoint, "{waka404woko.weke) !*(&,");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, GetMemoriesWithBadResponse) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories,
      {{memories::kMemoriesRemoteModelEndpointParam, endpoint}});

  AddVisit(0, GURL{"google.com"});
  AddVisit(1, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));
  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        EXPECT_TRUE(memories.empty());
        run_loop_quit_.Run();
      }));

  // Verify a request is made.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(endpoint));

  // Fake a response from the endpoint.
  test_url_loader_factory_.AddResponse(endpoint, "{}");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, GetMemoriesWithPendingRequest) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories,
      {{memories::kMemoriesRemoteModelEndpointParam, endpoint}});

  AddVisit(0, GURL{"google.com"});
  AddVisit(1, GURL{"github.com"});

  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));
  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify not reached.
        EXPECT_TRUE(false);
      }));

  EXPECT_TRUE(test_url_loader_factory_.IsPending(endpoint));
  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        EXPECT_EQ(memories.size(), 2u);
        run_loop_quit_.Run();
      }));

  // Verify there's a single request to the endpoint.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(endpoint));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Fake a response from the endpoint.
  test_url_loader_factory_.AddResponse(endpoint, R"({"memories": [{}, {}]})");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, CompleteVisitIfReady) {
  auto test = [&](memories::RecordingStatus status, bool expected_complete) {
    auto& visit = memories_service_->GetOrCreateIncompleteVisit(0);
    visit.status = status;
    memories_service_->CompleteVisitIfReady(0);
    EXPECT_NE(memories_service_->HasIncompleteVisit(0), expected_complete);
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

  auto test_dcheck = [&](memories::RecordingStatus status) {
    auto& visit = memories_service_->GetOrCreateIncompleteVisit(0);
    visit.status = status;
    EXPECT_DCHECK_DEATH(memories_service_->CompleteVisitIfReady(0));
    EXPECT_TRUE(memories_service_->HasIncompleteVisit(0));
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

TEST_F(MemoriesServiceTest, CompleteVisitIfReadyWhenFeatureDisabled) {
  {
    // When the feature is disabled, the incomplete visit should be removed but
    // not added to visits.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(memories::kMemories);
    auto& visit = memories_service_->GetOrCreateIncompleteVisit(0);
    visit.status = {true, true, true};
    memories_service_->CompleteVisitIfReady(0);
    EXPECT_FALSE(memories_service_->HasIncompleteVisit(0));
    EXPECT_TRUE(memories_service_test_api_->GetVisits().empty());
  }

  {
    // When the feature is enabled, the incomplete visit should be removed and
    // added to visits.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(memories::kMemories);
    auto& visit = memories_service_->GetOrCreateIncompleteVisit(0);
    visit.status = {true, true, true};
    memories_service_->CompleteVisitIfReady(0);
    EXPECT_FALSE(memories_service_->HasIncompleteVisit(0));
    EXPECT_EQ(memories_service_test_api_->GetVisits().size(), 1u);
  }
}

}  // namespace
