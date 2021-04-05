// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_service.h"

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/memories_service_test_api.h"
#include "components/history_clusters/core/visit_data.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

std::string StripWhitespace(std::string str) {
  base::EraseIf(str, base::IsAsciiWhitespace<char>);
  return str;
}

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
    AddVisit(visit);
  }

  void AddVisit(const memories::MemoriesVisit& visit) {
    auto& visit_copy =
        memories_service_->GetOrCreateIncompleteVisit(next_navigation_id_);
    visit_copy = visit;
    visit_copy.status.history_rows = true;
    visit_copy.status.navigation_ended = true;
    visit_copy.status.navigation_end_signals = true;
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
      memories::kMemories, {{memories::kRemoteModelEndpointParam, endpoint}});

  auto AddVisitWithDetails = [&](int time, const GURL& url,
                                 const std::u16string title, int visit_id,
                                 int page_end_reason) {
    memories::MemoriesVisit visit;
    visit.visit_row.visit_time = IntToTime(time);
    visit.url_row.set_url(url);
    visit.url_row.set_title(title);
    visit.visit_row.visit_id = visit_id;
    visit.context_signals.page_end_reason = page_end_reason;
    AddVisit(visit);
  };

  AddVisitWithDetails(2, GURL{"https://google.com"}, u"Google title", 2, 3);
  AddVisitWithDetails(4, GURL{"https://github.com"}, u"Github title", 4, 5);

  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));
  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        ASSERT_EQ(memories.size(), 2u);
        EXPECT_FALSE(memories[0]->id.is_empty());
        ASSERT_EQ(memories[0]->top_visits.size(), 2u);
        EXPECT_EQ(memories[0]->top_visits[0]->id, 2);
        EXPECT_EQ(memories[0]->top_visits[0]->url, "https://google.com/");
        EXPECT_EQ(memories[0]->top_visits[0]->time, IntToTime(2));
        EXPECT_EQ(base::UTF16ToUTF8(memories[0]->top_visits[0]->page_title),
                  "Google title");
        EXPECT_EQ(memories[0]->top_visits[1]->id, 4);
        EXPECT_EQ(memories[0]->top_visits[1]->url, "https://github.com/");
        EXPECT_EQ(memories[0]->top_visits[1]->time, IntToTime(4));
        EXPECT_EQ(base::UTF16ToUTF8(memories[0]->top_visits[1]->page_title),
                  "Github title");
        ASSERT_EQ(memories[1]->top_visits.size(), 1u);
        EXPECT_FALSE(memories[1]->id.is_empty());
        EXPECT_EQ(memories[1]->top_visits[0]->id, 4);
        EXPECT_EQ(memories[1]->top_visits[0]->url, "https://github.com/");
        EXPECT_EQ(memories[1]->top_visits[0]->time, IntToTime(4));
        EXPECT_EQ(base::UTF16ToUTF8(memories[1]->top_visits[0]->page_title),
                  "Github title");
        run_loop_quit_.Run();
      }));

  // Verify the serialized request.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(endpoint));
  EXPECT_EQ(GetPendingRequestBody(), StripWhitespace(R"(
      {
        "visits": [
          {
            "foregroundTimeSecs": 0,
            "isFromGoogleSearch": false,
            "navigationTimeMs": 2.0,
            "origin": "https://google.com/",
            "pageEndReason": 3,
            "pageTransition": 0,
            "siteEngagementScore": 0,
            "url": "https://google.com/",
            "visitId": 2.0
          },
          {
            "foregroundTimeSecs": 0,
            "isFromGoogleSearch": false,
            "navigationTimeMs": 4.0,
            "origin": "https://github.com/",
            "pageEndReason": 5,
            "pageTransition": 0,
            "siteEngagementScore": 0,
            "url": "https://github.com/",
            "visitId": 4.0
          }
        ]
      })"));

  // Fake a response from the endpoint.
  test_url_loader_factory_.AddResponse(endpoint, R"(
      {
        "memories": [
          {
            "description": "description",
            "topics": [
              "topic 1",
              "topic 2"
            ],
            "visitIds": [
              2,
              4
            ]
          },
          {
            "description": "description 2",
            "topics": [
              "topic 3",
              "topic 4"
            ],
            "visitIds": [
              4
            ]
          }
        ]
      })");
  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, GetMemoriesWithEmptyVisits) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories, {{memories::kRemoteModelEndpointParam, endpoint}});

  memories_service_->GetMemories(
      base::BindLambdaForTesting([&](memories::Memories memories) {
        // Verify the parsed response.
        EXPECT_TRUE(memories.empty());
        run_loop_quit_.Run();
      }));

  // Verify no request is made.
  EXPECT_FALSE(test_url_loader_factory_.IsPending(endpoint));

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(MemoriesServiceTest, GetMemoriesWithEmptyEndpoint) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories, {{memories::kRemoteModelEndpointParam, ""}});

  AddVisit(0, GURL{"google.com"});
  AddVisit(1, GURL{"github.com"});

  EXPECT_DCHECK_DEATH(memories_service_->GetMemories(base::BindLambdaForTesting(
      [&](memories::Memories memories) { NOTREACHED(); })));

  // Verify no request is made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(MemoriesServiceTest, GetMemoriesWithEmptyResponse) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories, {{memories::kRemoteModelEndpointParam, endpoint}});

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
      memories::kMemories, {{memories::kRemoteModelEndpointParam, endpoint}});

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

TEST_F(MemoriesServiceTest, GetMemoriesWithEmptyJsonResponse) {
  const char endpoint[] = "https://endpoint.com/";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      memories::kMemories, {{memories::kRemoteModelEndpointParam, endpoint}});

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
      memories::kMemories, {{memories::kRemoteModelEndpointParam, endpoint}});

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
