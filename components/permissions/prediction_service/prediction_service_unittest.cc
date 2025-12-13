// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "google/protobuf/message_lite.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::EqualsProto;
using ::testing::ElementsAre;
using ::testing::Pointee;
using ::testing::ValuesIn;

constexpr auto kNoExperimentId =
    permissions::PredictionRequestFeatures::ExperimentId::kNoExperimentId;
constexpr auto kCpssV3ExperimentId =
    permissions::PredictionRequestFeatures::ExperimentId::kCpssV3ExperimentId;
constexpr auto kAiV3ExperimentId =
    permissions::PredictionRequestFeatures::ExperimentId::kAiV3ExperimentId;
constexpr auto kAiV4ExperimentId =
    permissions::PredictionRequestFeatures::ExperimentId::kAiV4ExperimentId;

// Helper common requests and responses. All of these are for the NOTIFICATION
// type.

// A request that has all counts 0. With user gesture.
permissions::PredictionRequestFeatures createFeaturesAllCountsZero(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  return {.gesture = permissions::PermissionRequestGestureType::GESTURE,
          .type = permissions::RequestType::kNotifications,
          .requested_permission_counts = {0, 0, 0, 0},
          .all_permission_counts = {0, 0, 0, 0},
          .url = GURL(),
          .experiment_id = experiment_id};
}

// A request that has all counts 5 expect for "grants" which are 6. Without user
// gesture.
permissions::PredictionRequestFeatures createFeaturesCountsNeedingRounding(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  return {.gesture = permissions::PermissionRequestGestureType::NO_GESTURE,
          .type = permissions::RequestType::kNotifications,
          .requested_permission_counts = {6, 5, 5, 5},
          .all_permission_counts = {6, 5, 5, 5},
          .url = GURL(),
          .experiment_id = experiment_id};
}

// A request that has all counts 50. With user gesture.
permissions::PredictionRequestFeatures createFeaturesEvenCountsOver100(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  return {.gesture = permissions::PermissionRequestGestureType::GESTURE,
          .type = permissions::RequestType::kNotifications,
          .requested_permission_counts = {50, 50, 50, 50},
          .all_permission_counts = {50, 50, 50, 50},
          .url = GURL(),
          .experiment_id = experiment_id};
}

// A request that has all counts 100. With user gesture.
permissions::PredictionRequestFeatures createFeaturesEvenCountsOver100Alt(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  return {.gesture = permissions::PermissionRequestGestureType::GESTURE,
          .type = permissions::RequestType::kNotifications,
          .requested_permission_counts = {100, 100, 100, 100},
          .all_permission_counts = {100, 100, 100, 100},
          .url = GURL(),
          .experiment_id = experiment_id};
}

// A request that has generic counts 50, and notification counts 0. Without user
// gesture.
permissions::PredictionRequestFeatures createFeaturesDifferentCounts(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  return {.gesture = permissions::PermissionRequestGestureType::NO_GESTURE,
          .type = permissions::RequestType::kNotifications,
          .requested_permission_counts = {0, 0, 0, 0},
          .all_permission_counts = {50, 50, 50, 50},
          .url = GURL(),
          .experiment_id = experiment_id};
}

permissions::GeneratePredictionsRequest createRequestAllCountsZero(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  permissions::GeneratePredictionsRequest request;
  auto* client_features = request.mutable_client_features();
  client_features->set_platform(permissions::GetCurrentPlatformProto());
  client_features->set_platform_enum(
      permissions::GetCurrentPlatformEnumProto());
  client_features->set_gesture(permissions::ClientFeatures_Gesture_GESTURE);
  client_features->set_gesture_enum(
      permissions::ClientFeatures_GestureEnum_GESTURE_V2);
  client_features->mutable_experiment_config()->set_experiment_id(
      static_cast<int>(experiment_id));

  auto* client_stats = client_features->mutable_client_stats();
  client_stats->set_avg_deny_rate(0);
  client_stats->set_avg_dismiss_rate(0);
  client_stats->set_avg_grant_rate(0);
  client_stats->set_avg_ignore_rate(0);
  client_stats->set_prompts_count(0);

  auto* permission_feature = request.mutable_permission_features()->Add();
  permission_feature->set_permission_relevance(
      permissions::PermissionFeatures_Relevance_RELEVANCE_UNSPECIFIED);
  permission_feature->mutable_notification_permission()->Clear();

  auto* permission_stats = permission_feature->mutable_permission_stats();
  permission_stats->set_avg_deny_rate(0);
  permission_stats->set_avg_dismiss_rate(0);
  permission_stats->set_avg_grant_rate(0);
  permission_stats->set_avg_ignore_rate(0);
  permission_stats->set_prompts_count(0);

  return request;
}

// A proto request that has all ratios 0.24 (~5/21) except for "grants" which
// are 0.29 (~6/21). Without user gesture.
permissions::GeneratePredictionsRequest createRequestRoundedCounts(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  permissions::GeneratePredictionsRequest request;

  auto* client_features = request.mutable_client_features();
  client_features->set_platform(permissions::GetCurrentPlatformProto());
  client_features->set_platform_enum(
      permissions::GetCurrentPlatformEnumProto());
  client_features->set_gesture(permissions::ClientFeatures_Gesture_NO_GESTURE);
  client_features->set_gesture_enum(
      permissions::ClientFeatures_GestureEnum_GESTURE_UNSPECIFIED_V2);
  client_features->mutable_experiment_config()->set_experiment_id(
      static_cast<int>(experiment_id));

  auto* client_stats = client_features->mutable_client_stats();
  client_stats->set_avg_deny_rate(0.2);
  client_stats->set_avg_dismiss_rate(0.2);
  client_stats->set_avg_grant_rate(0.3);
  client_stats->set_avg_ignore_rate(0.2);
  client_stats->set_prompts_count(20);

  auto* permission_feature = request.mutable_permission_features()->Add();
  permission_feature->mutable_notification_permission()->Clear();
  permission_feature->set_permission_relevance(
      permissions::PermissionFeatures_Relevance_RELEVANCE_UNSPECIFIED);

  auto* permission_stats = permission_feature->mutable_permission_stats();
  permission_stats->set_avg_deny_rate(0.2);
  permission_stats->set_avg_dismiss_rate(0.2);
  permission_stats->set_avg_grant_rate(0.3);
  permission_stats->set_avg_ignore_rate(0.2);
  permission_stats->set_prompts_count(20);

  return request;
}

// A proto request that has all ratios .25 and total count 100. With user
// gesture.
permissions::GeneratePredictionsRequest createRequestEqualCountsTotal20(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  permissions::GeneratePredictionsRequest request;

  auto* client_features = request.mutable_client_features();
  client_features->mutable_experiment_config()->set_experiment_id(
      static_cast<int>(experiment_id));
  client_features->set_platform(permissions::GetCurrentPlatformProto());
  client_features->set_platform_enum(
      permissions::GetCurrentPlatformEnumProto());
  client_features->set_gesture(permissions::ClientFeatures_Gesture_GESTURE);
  client_features->set_gesture_enum(
      permissions::ClientFeatures_GestureEnum_GESTURE_V2);

  auto* client_stats = client_features->mutable_client_stats();
  client_stats->set_avg_deny_rate(.3);
  client_stats->set_avg_dismiss_rate(.3);
  client_stats->set_avg_grant_rate(.3);
  client_stats->set_avg_ignore_rate(.3);
  client_stats->set_prompts_count(20);

  auto* permission_feature = request.mutable_permission_features()->Add();
  permission_feature->mutable_notification_permission()->Clear();
  permission_feature->set_permission_relevance(
      permissions::PermissionFeatures_Relevance_RELEVANCE_UNSPECIFIED);

  auto* permission_stats = permission_feature->mutable_permission_stats();
  permission_stats->set_avg_deny_rate(.3);
  permission_stats->set_avg_dismiss_rate(.3);
  permission_stats->set_avg_grant_rate(.3);
  permission_stats->set_avg_ignore_rate(.3);
  permission_stats->set_prompts_count(20);

  return request;
}

// A proot request that has generic ratios .25 and total count 100 and
// notifications ratios and counts 0. Without user gesture.
permissions::GeneratePredictionsRequest createRequestDifferentCounts(
    permissions::PredictionRequestFeatures::ExperimentId experiment_id =
        kNoExperimentId) {
  permissions::GeneratePredictionsRequest request;

  auto* client_features = request.mutable_client_features();
  client_features->set_platform(permissions::GetCurrentPlatformProto());
  client_features->set_platform_enum(
      permissions::GetCurrentPlatformEnumProto());
  client_features->set_gesture(permissions::ClientFeatures_Gesture_NO_GESTURE);
  client_features->set_gesture_enum(
      permissions::ClientFeatures_GestureEnum_GESTURE_UNSPECIFIED_V2);
  client_features->mutable_experiment_config()->set_experiment_id(
      static_cast<int>(experiment_id));

  auto* client_stats = client_features->mutable_client_stats();
  client_stats->set_avg_deny_rate(.3);
  client_stats->set_avg_dismiss_rate(.3);
  client_stats->set_avg_grant_rate(.3);
  client_stats->set_avg_ignore_rate(.3);
  client_stats->set_prompts_count(20);

  auto* permission_feature = request.mutable_permission_features()->Add();
  permission_feature->mutable_notification_permission()->Clear();
  permission_feature->set_permission_relevance(
      permissions::PermissionFeatures_Relevance_RELEVANCE_UNSPECIFIED);

  auto* permission_stats = permission_feature->mutable_permission_stats();
  permission_stats->set_avg_deny_rate(0);
  permission_stats->set_avg_dismiss_rate(0);
  permission_stats->set_avg_grant_rate(0);
  permission_stats->set_avg_ignore_rate(0);
  permission_stats->set_prompts_count(0);

  return request;
}

// A response that has a likelihood of DiscretizedLikelihood::LIKELY.
permissions::GeneratePredictionsResponse createResponseLikely() {
  permissions::GeneratePredictionsResponse response;
  response.mutable_prediction()->Clear();
  auto* prediction = response.mutable_prediction()->Add();
  prediction->mutable_grant_likelihood()->set_discretized_likelihood(
      permissions::
          PermissionPrediction_Likelihood_DiscretizedLikelihood_LIKELY);
  return response;
}

// A response that has a likelihood of DiscretizedLikelihood::UNLIKELY.
permissions::GeneratePredictionsResponse createResponseUnlikely() {
  permissions::GeneratePredictionsResponse response;
  response.mutable_prediction()->Clear();
  auto* prediction = response.mutable_prediction()->Add();
  prediction->mutable_grant_likelihood()->set_discretized_likelihood(
      permissions::
          PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY);
  return response;
}

}  // namespace

namespace permissions {

struct PredictionServiceTestParam {
  PredictionRequestFeatures features;
  GeneratePredictionsRequest expected_request;
  std::optional<std::string> url_string_for_features;
};

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
  }

  void Respond(const GURL& url,
               double delay_in_seconds = 0,
               int err_code = net::OK) {
    if (delay_in_seconds > 0) {
      // Post a task to rerun this after |delay_in_seconds| seconds
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PredictionServiceTest::Respond,
                         base::Unretained(this), url, 0, err_code),
          base::Seconds(delay_in_seconds));
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
                       std::unique_ptr<GeneratePredictionsRequest> request,
                       std::string access_token) {
    received_requests_.emplace_back(std::move(request));
    if (request_loop) {
      request_loop->Quit();
    }

    // Access token should always be the empty string.
    EXPECT_EQ(std::string(), access_token);
  }

  void ResponseCallback(
      base::RunLoop* response_loop,
      bool lookup_successful,
      bool response_from_cache,
      const std::optional<GeneratePredictionsResponse>& response) {
    received_responses_.emplace_back(response);
    if (response_loop) {
      response_loop->Quit();
    }

    // The response is never from the cache.
    EXPECT_FALSE(response_from_cache);
  }

 protected:
  std::vector<std::unique_ptr<GeneratePredictionsRequest>> received_requests_;
  std::vector<std::optional<GeneratePredictionsResponse>> received_responses_;
  std::unique_ptr<PredictionService> prediction_service_;

  // Different paths to simulate different server behaviours.
  const GURL kUrl_Unlikely{"http://predictionsevice.com/unlikely"};
  const GURL kUrl_Likely{"http://predictionsevice.com/likely"};
  const GURL kUrl_Invalid{"http://predictionsevice.com/invalid"};
  const GURL test_requesting_url{
      "https://www.test.example/path/to/page.html:8080"};

 private:
  std::string GetResponseForUrl(const GURL& url) {
    if (url == kUrl_Unlikely) {
      return createResponseUnlikely().SerializeAsString();
    } else if (url == kUrl_Likely) {
      return createResponseLikely().SerializeAsString();
    } else if (url == GURL(permissions::kDefaultPredictionServiceUrl)) {
      return createResponseLikely().SerializeAsString();
    } else if (url == kUrl_Invalid) {
      return "This is not a valid response";
    }

    return "";
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(PredictionServiceTest, PromptCountsAreBucketed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {permissions::features::kPermissionPredictionsV2,
       permissions::features::kPermissionsAIv4},
      {});

  struct {
    size_t prompt_count;
    int expected_bucket;
  } kTests[] = {{4, 4},   {5, 5},   {6, 6},   {7, 7},    {8, 8},
                {9, 9},   {10, 10}, {11, 10}, {12, 12},  {14, 12},
                {15, 15}, {19, 15}, {20, 20}, {100, 20}, {1000, 20}};

  prediction_service_->set_prediction_service_url_for_testing(
      GURL(kUrl_Likely));

  for (const auto& kTest : kTests) {
    permissions::PredictionRequestFeatures features =
        createFeaturesAllCountsZero();
    features.requested_permission_counts.denies = kTest.prompt_count;

    permissions::GeneratePredictionsRequest expected_request =
        createRequestAllCountsZero();
    expected_request.mutable_permission_features()
        ->at(0)
        .mutable_permission_stats()
        ->set_avg_deny_rate(1);
    expected_request.mutable_permission_features()
        ->at(0)
        .mutable_permission_stats()
        ->set_prompts_count(kTest.expected_bucket);
    expected_request.mutable_permission_features()
        ->at(0)
        .set_permission_relevance(
            permissions::PermissionFeatures_Relevance_RELEVANCE_UNSPECIFIED);

    base::RunLoop run_loop;
    StartLookup(features, &run_loop, nullptr /* response_loop */);
    run_loop.Run();

    EXPECT_THAT(received_requests_,
                ElementsAre(Pointee(EqualsProto(expected_request))));

    received_requests_.clear();
  }
}

class PredictionServiceProtoRequestTest
    : public PredictionServiceTest,
      public testing::WithParamInterface<PredictionServiceTestParam> {};

TEST_P(PredictionServiceProtoRequestTest, BuiltProtoRequestIsCorrect) {
  // Test origin being added correctly in the request.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {permissions::features::kPermissionPredictionsV2,
       permissions::features::kPermissionsAIv4},
      {});

  PredictionServiceTestParam param = GetParam();
  if (param.url_string_for_features.has_value()) {
    param.features.url = GURL(param.url_string_for_features.value());
    param.expected_request.mutable_site_features()->set_origin(
        param.url_string_for_features.value());
  }

  prediction_service_->set_prediction_service_url_for_testing(
      GURL(kUrl_Likely));
  base::RunLoop run_loop;
  StartLookup(param.features, &run_loop, nullptr /* response_loop */);

  run_loop.Run();
  EXPECT_THAT(received_requests_,
              ElementsAre(Pointee(EqualsProto(param.expected_request))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PredictionServiceProtoRequestTest,
    ValuesIn(std::vector<PredictionServiceTestParam>{
        {createFeaturesAllCountsZero(kNoExperimentId),
         createRequestAllCountsZero(kNoExperimentId),
         "https://www.test.example/"},
        {createFeaturesCountsNeedingRounding(kAiV3ExperimentId),
         createRequestRoundedCounts(kAiV3ExperimentId), std::nullopt},
        {createFeaturesEvenCountsOver100(kAiV4ExperimentId),
         createRequestEqualCountsTotal20(kAiV4ExperimentId), std::nullopt},
        {createFeaturesEvenCountsOver100Alt(kCpssV3ExperimentId),
         createRequestEqualCountsTotal20(kCpssV3ExperimentId), std::nullopt},
        {createFeaturesDifferentCounts(kAiV4ExperimentId),
         createRequestDifferentCounts(kAiV4ExperimentId), std::nullopt},
    }));

TEST_F(PredictionServiceTest, ResponsesAreCorrect) {
  struct {
    GURL url;
    std::optional<GeneratePredictionsResponse> expected_response;
    double delay_in_seconds;
    int err_code;
  } kTests[] = {
      // Test different responses.
      {kUrl_Likely,
       std::optional<GeneratePredictionsResponse>(createResponseLikely())},
      {kUrl_Unlikely,
       std::optional<GeneratePredictionsResponse>(createResponseUnlikely())},

      // Test the response's timeout.
      {kUrl_Likely,
       std::optional<GeneratePredictionsResponse>(createResponseLikely()), 0.5},
      {kUrl_Likely, std::nullopt, 2},

      // Test error code responses.
      {kUrl_Likely, std::nullopt, 0, net::ERR_SSL_PROTOCOL_ERROR},
      {kUrl_Likely, std::nullopt, 0, net::ERR_CONNECTION_FAILED},
  };

  for (const auto& kTest : kTests) {
    prediction_service_->set_prediction_service_url_for_testing(kTest.url);
    base::RunLoop response_loop;
    StartLookup(createFeaturesAllCountsZero(), nullptr, &response_loop);
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

// Test that the Web Prediction Service url can be overridden via command
// line, and the fallback logic in case the provided url is not valid.
TEST_F(PredictionServiceTest, FeatureParamAndCommandLineCanOverrideDefaultUrl) {
  struct {
    std::optional<std::string> command_line_switch_value;
    GURL expected_request_url;
    permissions::GeneratePredictionsResponse expected_response;
  } kTests[] = {
      // Test without any overrides.
      {std::nullopt, GURL(kDefaultPredictionServiceUrl),
       createResponseLikely()},

      // Test only the command line override.
      {kUrl_Unlikely.spec(), kUrl_Unlikely, createResponseUnlikely()},
      {"this is not a url", GURL(kDefaultPredictionServiceUrl),
       createResponseLikely()},
      {"", GURL(kDefaultPredictionServiceUrl), createResponseLikely()},
  };

  prediction_service_->recalculate_service_url_every_time_for_testing();

  for (const auto& kTest : kTests) {
    base::test::ScopedFeatureList scoped_feature_list;

    if (kTest.command_line_switch_value.has_value()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          kDefaultPredictionServiceUrlSwitchKey,
          kTest.command_line_switch_value.value());
    }

    base::RunLoop response_loop;
    StartLookup(createFeaturesAllCountsZero(), nullptr, &response_loop);
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
}  // namespace permissions

TEST_F(PredictionServiceTest, HandleSimultaneousRequests) {
  prediction_service_->set_prediction_service_url_for_testing(kUrl_Likely);
  base::RunLoop response_loop;
  StartLookup(createFeaturesAllCountsZero(), nullptr, &response_loop);

  prediction_service_->set_prediction_service_url_for_testing(kUrl_Unlikely);
  base::RunLoop response_loop2;
  StartLookup(createFeaturesAllCountsZero(), nullptr, &response_loop2);

  EXPECT_EQ(2u, prediction_service_->pending_requests_for_testing().size());

  Respond(kUrl_Unlikely);
  response_loop2.Run();

  EXPECT_EQ(1u, received_responses_.size());
  EXPECT_TRUE(received_responses_[0]);
  EXPECT_EQ(createResponseUnlikely().SerializeAsString(),
            received_responses_[0]->SerializeAsString());
  EXPECT_EQ(1u, prediction_service_->pending_requests_for_testing().size());

  Respond(kUrl_Likely);
  response_loop.Run();

  EXPECT_EQ(2u, received_responses_.size());
  EXPECT_TRUE(received_responses_[1]);
  EXPECT_EQ(createResponseLikely().SerializeAsString(),
            received_responses_[1]->SerializeAsString());
  EXPECT_EQ(0u, prediction_service_->pending_requests_for_testing().size());
}

TEST_F(PredictionServiceTest, InvalidResponse) {
  base::RunLoop response_loop;
  StartLookup(createFeaturesAllCountsZero(), nullptr, &response_loop);
  Respond(GURL(kUrl_Invalid));
  response_loop.Run();
  EXPECT_FALSE(received_responses_[0]);
}

}  // namespace permissions
