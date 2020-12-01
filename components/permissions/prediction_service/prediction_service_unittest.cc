// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_service.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_common.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "google/protobuf/message_lite.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {
// Helper common requests and responses. All of these are for the NOTIFICATION
// type.

// A request that has all counts 0. With user gesture.
const permissions::PredictionRequestFeatures kFeaturesAllCountsZero = {
    permissions::PermissionRequestGestureType::GESTURE,
    permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
    {0, 0, 0, 0},
    {0, 0, 0, 0}};
// A request that has all counts 5 expect for "grants" which are 6. Without user
// gesture.
const permissions::PredictionRequestFeatures kFeaturesCountsNeedingRounding = {
    permissions::PermissionRequestGestureType::NO_GESTURE,
    permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
    {6, 5, 5, 5},
    {6, 5, 5, 5}};
// A request that has all counts 50. With user gesture.
const permissions::PredictionRequestFeatures kFeaturesEvenCountsOver100 = {
    permissions::PermissionRequestGestureType::GESTURE,
    permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
    {50, 50, 50, 50},
    {50, 50, 50, 50}};
// A request that has all counts 100. With user gesture.
const permissions::PredictionRequestFeatures kFeaturesEvenCountsOver100Alt = {
    permissions::PermissionRequestGestureType::GESTURE,
    permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
    {100, 100, 100, 100},
    {100, 100, 100, 100}};
// A request that has generic counts 50, and notification counts 0. Without user
// gesture.
const permissions::PredictionRequestFeatures kFeaturesDifferentCounts = {
    permissions::PermissionRequestGestureType::NO_GESTURE,
    permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
    {0, 0, 0, 0},
    {50, 50, 50, 50}};

// A proto request that has all ratios and total counts 0. With user gesture.
permissions::GetSuggestionsRequest kRequestAllCountsZero;
// A proto request that has all ratios 0.24 (~5/21) except for "grants" which
// are 0.29 (~6/21). Without user gesture.
permissions::GetSuggestionsRequest kRequestRoundedCounts;
// A proto request that has all ratios .25 and total count 100. With user
// gesture.
permissions::GetSuggestionsRequest kRequestEqualCountsTotal100;
// A proot request that has generic ratios .25 and total count 100 and
// notifications ratios and counts 0. Without user gesture.
permissions::GetSuggestionsRequest kRequestDifferentCounts;

// A response that has a likelihood of DiscretizedLikelihood::LIKELY.
permissions::GetSuggestionsResponse kResponseLikely;

// A response that has a likelihood of DiscretizedLikelihood::UNLIKELY.
permissions::GetSuggestionsResponse kResponseUnlikely;

void InitializeProtoHelperObjects() {
  kRequestAllCountsZero.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_deny_rate(0);
  kRequestAllCountsZero.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_dismiss_rate(0);
  kRequestAllCountsZero.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_grant_rate(0);
  kRequestAllCountsZero.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_ignore_rate(0);
  kRequestAllCountsZero.mutable_client_features()
      ->mutable_client_stats()
      ->set_prompts_count(0);
  kRequestAllCountsZero.mutable_client_features()->set_platform(
      permissions::GetCurrentPlatformProto());
  kRequestAllCountsZero.mutable_client_features()->set_gesture(
      permissions::ClientFeatures_Gesture_GESTURE);
  auto* permission_feature =
      kRequestAllCountsZero.mutable_permission_features()->Add();
  permission_feature->mutable_permission_stats()->set_avg_deny_rate(0);
  permission_feature->mutable_permission_stats()->set_avg_dismiss_rate(0);
  permission_feature->mutable_permission_stats()->set_avg_grant_rate(0);
  permission_feature->mutable_permission_stats()->set_avg_ignore_rate(0);
  permission_feature->mutable_permission_stats()->set_prompts_count(0);
  permission_feature->mutable_notification_permission()
      ->InitAsDefaultInstance();

  kRequestRoundedCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_deny_rate(0.24);
  kRequestRoundedCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_dismiss_rate(0.24);
  kRequestRoundedCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_grant_rate(0.29);
  kRequestRoundedCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_ignore_rate(0.24);
  kRequestRoundedCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_prompts_count(21);
  kRequestRoundedCounts.mutable_client_features()->set_platform(
      permissions::GetCurrentPlatformProto());
  kRequestRoundedCounts.mutable_client_features()->set_gesture(
      permissions::ClientFeatures_Gesture_NO_GESTURE);
  permission_feature =
      kRequestRoundedCounts.mutable_permission_features()->Add();
  permission_feature->mutable_permission_stats()->set_avg_deny_rate(0.24);
  permission_feature->mutable_permission_stats()->set_avg_dismiss_rate(0.24);
  permission_feature->mutable_permission_stats()->set_avg_grant_rate(0.29);
  permission_feature->mutable_permission_stats()->set_avg_ignore_rate(0.24);
  permission_feature->mutable_permission_stats()->set_prompts_count(21);
  permission_feature->mutable_notification_permission()
      ->InitAsDefaultInstance();

  kRequestEqualCountsTotal100.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_deny_rate(.25);
  kRequestEqualCountsTotal100.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_dismiss_rate(.25);
  kRequestEqualCountsTotal100.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_grant_rate(.25);
  kRequestEqualCountsTotal100.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_ignore_rate(.25);
  kRequestEqualCountsTotal100.mutable_client_features()
      ->mutable_client_stats()
      ->set_prompts_count(100);
  kRequestEqualCountsTotal100.mutable_client_features()->set_platform(
      permissions::GetCurrentPlatformProto());
  kRequestEqualCountsTotal100.mutable_client_features()->set_gesture(
      permissions::ClientFeatures_Gesture_GESTURE);
  permission_feature =
      kRequestEqualCountsTotal100.mutable_permission_features()->Add();
  permission_feature->mutable_permission_stats()->set_avg_deny_rate(.25);
  permission_feature->mutable_permission_stats()->set_avg_dismiss_rate(.25);
  permission_feature->mutable_permission_stats()->set_avg_grant_rate(.25);
  permission_feature->mutable_permission_stats()->set_avg_ignore_rate(.25);
  permission_feature->mutable_permission_stats()->set_prompts_count(100);
  permission_feature->mutable_notification_permission()
      ->InitAsDefaultInstance();

  kRequestDifferentCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_deny_rate(.25);
  kRequestDifferentCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_dismiss_rate(.25);
  kRequestDifferentCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_grant_rate(.25);
  kRequestDifferentCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_avg_ignore_rate(.25);
  kRequestDifferentCounts.mutable_client_features()
      ->mutable_client_stats()
      ->set_prompts_count(100);
  kRequestDifferentCounts.mutable_client_features()->set_platform(
      permissions::GetCurrentPlatformProto());
  kRequestDifferentCounts.mutable_client_features()->set_gesture(
      permissions::ClientFeatures_Gesture_NO_GESTURE);
  permission_feature =
      kRequestDifferentCounts.mutable_permission_features()->Add();
  permission_feature->mutable_permission_stats()->set_avg_deny_rate(0);
  permission_feature->mutable_permission_stats()->set_avg_dismiss_rate(0);
  permission_feature->mutable_permission_stats()->set_avg_grant_rate(0);
  permission_feature->mutable_permission_stats()->set_avg_ignore_rate(0);
  permission_feature->mutable_permission_stats()->set_prompts_count(0);
  permission_feature->mutable_notification_permission()
      ->InitAsDefaultInstance();

  auto* prediction = kResponseLikely.mutable_suggestion()->Add();
  prediction->mutable_grant_likelihood()->set_discretized_likelihood(
      permissions::
          PermissionSuggestion_Likelihood_DiscretizedLikelihood_LIKELY);

  prediction = kResponseUnlikely.mutable_suggestion()->Add();
  prediction->mutable_grant_likelihood()->set_discretized_likelihood(
      permissions::
          PermissionSuggestion_Likelihood_DiscretizedLikelihood_UNLIKELY);
}

}  // namespace

namespace permissions {
class PredictionServiceTest : public testing::Test {
 public:
  PredictionServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ~PredictionServiceTest() override = default;

  void SetUp() override {
    prediction_service_ =
        std::make_unique<PredictionService>(test_shared_loader_factory_);

    InitializeProtoHelperObjects();
  }

  void Respond(const GURL& url,
               double delay_in_seconds = 0,
               int err_code = net::OK) {
    if (delay_in_seconds > 0) {
      // Post a task to rerun this after |delay_in_seconds| seconds
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PredictionServiceTest::Respond,
                         base::Unretained(this), url, 0, err_code),
          base::TimeDelta::FromSecondsD(delay_in_seconds));
      return;
    }

    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    head->headers->AddHeader("Content-Type", "application/octet-stream");
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, network::URLLoaderCompletionStatus(err_code), std::move(head),
        GetResponseForUrl(url));
  }

  void StartLookup(const PredictionRequestFeatures& entity,
                   base::RunLoop* request_loop,
                   base::RunLoop* response_loop) {
    prediction_service_->StartLookup(
        entity,
        base::BindOnce(&PredictionServiceTest::RequestCallback,
                       base::Unretained(this), request_loop),
        base::BindOnce(&PredictionServiceTest::ResponseCallback,
                       base::Unretained(this), response_loop));
  }

  void RequestCallback(base::RunLoop* request_loop,
                       std::unique_ptr<GetSuggestionsRequest> request,
                       std::string access_token) {
    received_requests_.emplace_back(std::move(request));
    if (request_loop)
      request_loop->Quit();

    // Access token should always be the empty string.
    EXPECT_EQ(std::string(), access_token);
  }

  void ResponseCallback(base::RunLoop* response_loop,
                        bool lookup_successful,
                        bool response_from_cache,
                        std::unique_ptr<GetSuggestionsResponse> response) {
    received_responses_.emplace_back(std::move(response));
    if (response_loop)
      response_loop->Quit();

    // The response is never from the cache.
    EXPECT_FALSE(response_from_cache);
  }

 protected:
  std::vector<std::unique_ptr<GetSuggestionsRequest>> received_requests_;
  std::vector<std::unique_ptr<GetSuggestionsResponse>> received_responses_;
  std::unique_ptr<PredictionService> prediction_service_;

  // Different paths to simulate different server behaviours.
  const GURL kUrl_Unlikely{"http://predictionsevice.com/unlikely"};
  const GURL kUrl_Likely{"http://predictionsevice.com/likely"};
  const GURL kUrl_Invalid{"http://predictionsevice.com/invalid"};

 private:
  std::string GetResponseForUrl(const GURL& url) {
    if (url == kUrl_Unlikely) {
      return kResponseUnlikely.SerializeAsString();
    } else if (url == kUrl_Likely) {
      return kResponseLikely.SerializeAsString();
    } else if (url == GURL(permissions::kDefaultPredictionServiceUrl)) {
      return kResponseLikely.SerializeAsString();
    } else if (url == kUrl_Invalid) {
      return "This is not a valid response";
    }

    return "";
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(PredictionServiceTest, BuiltProtoRequestIsCorrect) {
  struct {
    PredictionRequestFeatures entity;
    GetSuggestionsRequest expected_request;
  } kTests[] = {
      {kFeaturesAllCountsZero, kRequestAllCountsZero},
      {kFeaturesCountsNeedingRounding, kRequestRoundedCounts},
      {kFeaturesEvenCountsOver100, kRequestEqualCountsTotal100},
      {kFeaturesEvenCountsOver100Alt, kRequestEqualCountsTotal100},
      {kFeaturesDifferentCounts, kRequestDifferentCounts},
  };

  prediction_service_->set_prediction_service_url_for_testing(
      GURL(kUrl_Likely));
  for (const auto& kTest : kTests) {
    base::RunLoop run_loop;
    StartLookup(kTest.entity, &run_loop, nullptr /* response_loop */);
    run_loop.Run();

    EXPECT_EQ(1u, received_requests_.size());
    EXPECT_EQ(kTest.expected_request.SerializeAsString(),
              received_requests_[0]->SerializeAsString());

    received_requests_.clear();
  }
}

TEST_F(PredictionServiceTest, ResponsesAreCorrect) {
  struct {
    GURL url;
    base::Optional<GetSuggestionsResponse> expected_response;
    double delay_in_seconds;
    int err_code;
  } kTests[] = {
      // Test different responses.
      {kUrl_Likely, base::Optional<GetSuggestionsResponse>(kResponseLikely)},
      {kUrl_Unlikely,
       base::Optional<GetSuggestionsResponse>(kResponseUnlikely)},

      // Test the response's timeout.
      {kUrl_Likely, base::Optional<GetSuggestionsResponse>(kResponseLikely),
       0.5},
      {kUrl_Likely, base::nullopt, 2},

      // Test error code responses.
      {kUrl_Likely, base::nullopt, 0, net::ERR_SSL_PROTOCOL_ERROR},
      {kUrl_Likely, base::nullopt, 0, net::ERR_CONNECTION_FAILED},
  };

  for (const auto& kTest : kTests) {
    prediction_service_->set_prediction_service_url_for_testing(kTest.url);
    base::RunLoop response_loop;
    StartLookup(kFeaturesAllCountsZero, nullptr, &response_loop);
    Respond(kTest.url, kTest.delay_in_seconds, kTest.err_code);
    response_loop.Run();

    EXPECT_EQ(1u, received_responses_.size());
    if (kTest.expected_response.has_value()) {
      EXPECT_TRUE(received_responses_[0]);
      EXPECT_EQ(kTest.expected_response->SerializeAsString(),
                received_responses_[0]->SerializeAsString());
    } else {
      EXPECT_FALSE(received_responses_[0]);
    }
    received_responses_.clear();
  }
}

// Test that the Web Prediction Service url can be overridden via feature params
// and command line, and the fallback logic in case the provided url is not
// valid.
TEST_F(PredictionServiceTest, FeatureParamAndCommandLineCanOverrideDefaultUrl) {
  struct {
    base::Optional<std::string> command_line_switch_value;
    base::Optional<std::string> url_override_param_value;
    GURL expected_request_url;
    permissions::GetSuggestionsResponse expected_response;
  } kTests[] = {
      // Test without any overrides.
      {base::nullopt, base::nullopt, GURL(kDefaultPredictionServiceUrl),
       kResponseLikely},

      // Test only the FeatureParam override.
      {base::nullopt, kUrl_Unlikely.spec(), kUrl_Unlikely, kResponseUnlikely},
      {base::nullopt, "this is not a url", GURL(kDefaultPredictionServiceUrl),
       kResponseLikely},
      {base::nullopt, "", GURL(kDefaultPredictionServiceUrl), kResponseLikely},

      // Test only the command line override.
      {kUrl_Unlikely.spec(), base::nullopt, kUrl_Unlikely, kResponseUnlikely},
      {"this is not a url", base::nullopt, GURL(kDefaultPredictionServiceUrl),
       kResponseLikely},
      {"", base::nullopt, GURL(kDefaultPredictionServiceUrl), kResponseLikely},

      // Command line takes precedence over FeatureParam, if valid.
      {kUrl_Likely.spec(), kUrl_Unlikely.spec(), kUrl_Likely, kResponseLikely},
      {"this is not a url", kUrl_Unlikely.spec(), kUrl_Unlikely,
       kResponseUnlikely},
      {"this is not a url", "this is not a url",
       GURL(kDefaultPredictionServiceUrl), kResponseLikely},
  };

  prediction_service_->recalculate_service_url_every_time_for_testing();

  for (const auto& kTest : kTests) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (kTest.url_override_param_value.has_value()) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kPermissionPredictionServiceUseUrlOverride,
          {{feature_params::kPermissionPredictionServiceUrlOverride.name,
            kTest.url_override_param_value.value()}});
    }

    if (kTest.command_line_switch_value.has_value()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          kDefaultPredictionServiceUrlSwitchKey,
          kTest.command_line_switch_value.value());
    }

    base::RunLoop response_loop;
    StartLookup(kFeaturesAllCountsZero, nullptr, &response_loop);
    Respond(kTest.expected_request_url);
    response_loop.Run();
    EXPECT_EQ(1u, received_responses_.size());
    EXPECT_TRUE(received_responses_[0]);
    EXPECT_EQ(kTest.expected_response.SerializeAsString(),
              received_responses_[0]->SerializeAsString());

    // Cleanup for next test.
    received_responses_.clear();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        kDefaultPredictionServiceUrlSwitchKey);
  }
}

TEST_F(PredictionServiceTest,
       FeatureEnabledWithNoFeatureParamFallsBackOnDefault) {
  prediction_service_->recalculate_service_url_every_time_for_testing();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPermissionPredictionServiceUseUrlOverride);

  base::RunLoop response_loop;
  StartLookup(kFeaturesAllCountsZero, nullptr, &response_loop);
  Respond(GURL(kDefaultPredictionServiceUrl));
  response_loop.Run();
  EXPECT_EQ(1u, received_responses_.size());
  EXPECT_TRUE(received_responses_[0]);
  EXPECT_EQ(kResponseLikely.SerializeAsString(),
            received_responses_[0]->SerializeAsString());
}

TEST_F(PredictionServiceTest, HandleSimultaneousRequests) {
  prediction_service_->set_prediction_service_url_for_testing(kUrl_Likely);
  base::RunLoop response_loop;
  StartLookup(kFeaturesAllCountsZero, nullptr, &response_loop);

  prediction_service_->set_prediction_service_url_for_testing(kUrl_Unlikely);
  base::RunLoop response_loop2;
  StartLookup(kFeaturesAllCountsZero, nullptr, &response_loop2);

  EXPECT_EQ(2u, prediction_service_->pending_requests_for_testing().size());

  Respond(kUrl_Unlikely);
  response_loop2.Run();

  EXPECT_EQ(1u, received_responses_.size());
  EXPECT_TRUE(received_responses_[0]);
  EXPECT_EQ(kResponseUnlikely.SerializeAsString(),
            received_responses_[0]->SerializeAsString());
  EXPECT_EQ(1u, prediction_service_->pending_requests_for_testing().size());

  Respond(kUrl_Likely);
  response_loop.Run();

  EXPECT_EQ(2u, received_responses_.size());
  EXPECT_TRUE(received_responses_[1].get() != nullptr);
  EXPECT_EQ(kResponseLikely.SerializeAsString(),
            received_responses_[1]->SerializeAsString());
  EXPECT_EQ(0u, prediction_service_->pending_requests_for_testing().size());
}

TEST_F(PredictionServiceTest, InvalidResponse) {
  base::RunLoop response_loop;
  StartLookup(kFeaturesAllCountsZero, nullptr, &response_loop);
  Respond(GURL(kUrl_Invalid));
  response_loop.Run();
  EXPECT_FALSE(received_responses_[0]);
}

}  // namespace permissions
