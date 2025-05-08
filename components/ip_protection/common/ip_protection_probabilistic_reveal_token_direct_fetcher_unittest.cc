// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_direct_fetcher.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"

namespace ip_protection {

namespace {

using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::testing::StartsWith;

constexpr size_t kGetProbabilisticRevealTokenResponseMaxBodySize = 40 * 1024;
constexpr char kIssuerServerUrl[] =
    "https://aaftokenissuer.pa.googleapis.com/v1/issueprts";

}  // namespace

class FetcherTestBase : public testing::Test {
 protected:
  // SetUp with default feature parameters, i.e., IPP enabled and experiment arm
  // is 42.
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters[net::features::kIpPrivacyDebugExperimentArm.name] = "42";
    SetFeatureParameters(std::move(parameters));
    token_server_get_prt_url_ = GURL(kIssuerServerUrl);
  }

  void TearDown() override {
    scoped_feature_list_ = nullptr;
    expected_experiment_arm_ = std::nullopt;
  }

 public:
  // Set IP Protection feature parameters. Sets `expected_experiment_arm_` from
  // `parameters`. Sets `retriever_` to a new
  // one. Feature settings are picked up when retriever is constructed.
  virtual void SetFeatureParameters(
      std::map<std::string, std::string> parameters) {
    expected_experiment_arm_ = std::nullopt;
    if (parameters.find(net::features::kIpPrivacyDebugExperimentArm.name) !=
        parameters.end()) {
      expected_experiment_arm_ =
          parameters[net::features::kIpPrivacyDebugExperimentArm.name];
    }
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy, std::move(parameters));
  }

  // Response should be in scope until the interceptor is finished.
  void SetResponse(const std::string& response,
                   base::TimeDelta response_delay = base::Seconds(0)) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&, response_delay](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.url.is_valid());
          EXPECT_EQ(request.url, token_server_get_prt_url_);
          EXPECT_EQ(request.method, net::HttpRequestHeaders::kPostMethod);
          EXPECT_EQ(request.credentials_mode,
                    network::mojom::CredentialsMode::kOmit);
          EXPECT_THAT(
              request.headers.GetHeader(net::HttpRequestHeaders::kAccept),
              testing::Optional(std::string("application/x-protobuf")));
          EXPECT_THAT(
              request.headers.GetHeader("Ip-Protection-Debug-Experiment-Arm"),
              expected_experiment_arm_);
          EXPECT_TRUE(request.headers.HasHeader("X-Goog-Api-Key"));
          ASSERT_TRUE(request.request_body);
          GetProbabilisticRevealTokenRequest request_proto;
          ASSERT_TRUE(request_proto.ParseFromString(GetUploadData(request)));
          ASSERT_TRUE(request_proto.has_service_type());
          EXPECT_EQ(request_proto.service_type(),
                    GetProbabilisticRevealTokenRequest_ServiceType_CHROME);
          task_environment_.FastForwardBy(response_delay);
          auto head = network::mojom::URLResponseHead::New();
          test_url_loader_factory_.AddResponse(
              token_server_get_prt_url_, std::move(head), response,
              network::URLLoaderCompletionStatus(net::OK));
        }));
  }

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::optional<std::string> expected_experiment_arm_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  GURL token_server_get_prt_url_;
};

class IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest
    : public FetcherTestBase {
 public:
  void SetFeatureParameters(
      std::map<std::string, std::string> parameters) override {
    FetcherTestBase::SetFeatureParameters(parameters);
    retriever_ = std::make_unique<
        IpProtectionProbabilisticRevealTokenDirectFetcher::Retriever>(
        test_url_loader_factory_.GetSafeWeakWrapper()->Clone(),
        version_info::Channel::DEFAULT);
  }

  std::unique_ptr<IpProtectionProbabilisticRevealTokenDirectFetcher::Retriever>
      retriever_;
};

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest,
       NullExperimentArm) {
  SetFeatureParameters({});
  ASSERT_EQ(expected_experiment_arm_, std::nullopt);
  const std::string response_str = "";
  // Interceptor callback asserts the experiment arm value is nullopt.
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  // Call RetrieveProbabilisticRevealTokens() to trigger interceptor callback.
  retriever_->RetrieveProbabilisticRevealTokens(result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest,
       RetrieveProbabilisticRevealTokenSuccess) {
  const std::string response_str = "some response";
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveProbabilisticRevealTokens(result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest,
       EmptyResponse) {
  const std::string response_str = "";
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveProbabilisticRevealTokens(result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest,
       LargeResponseWithinLimit) {
  const std::string response_str =
      std::string(kGetProbabilisticRevealTokenResponseMaxBodySize, 'a');
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveProbabilisticRevealTokens(result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest,
       LargeResponseOverLimit) {
  const std::string response_str =
      std::string(kGetProbabilisticRevealTokenResponseMaxBodySize + 1, 'a');
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveProbabilisticRevealTokens(result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), net::ERR_INSUFFICIENT_RESOURCES);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest,
       OutOfMemory) {
  const std::string response_str = "";
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            token_server_get_prt_url_, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::ERR_OUT_OF_MEMORY));
      }));
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveProbabilisticRevealTokens(result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), net::ERR_OUT_OF_MEMORY);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherRetrieverTest,
       RequestTimeout) {
  const std::string response_str = "some response";
  SetResponse(response_str, /*response_delay=*/base::Seconds(61));
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveProbabilisticRevealTokens(result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), net::ERR_TIMED_OUT);
}

class IpProtectionProbabilisticRevealTokenDirectFetcherTest
    : public FetcherTestBase {
 public:
  void SetFeatureParameters(
      std::map<std::string, std::string> parameters) override {
    FetcherTestBase::SetFeatureParameters(parameters);
    fetcher_ =
        std::make_unique<IpProtectionProbabilisticRevealTokenDirectFetcher>(
            test_url_loader_factory_.GetSafeWeakWrapper()->Clone(),
            version_info::Channel::DEFAULT);

    base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                   absl::Status>
        maybe_issuer =
            ProbabilisticRevealTokenTestIssuer::Create(/*private_key=*/1);
    ASSERT_TRUE(maybe_issuer.has_value())
        << "creating test issuer failed with error: " << maybe_issuer.error();
    auto issuer = std::move(maybe_issuer).value();
    public_key_bytes_ = issuer->GetSerializedPublicKey();
  }

  GetProbabilisticRevealTokenResponse BuildProbabilisticRevealTokenResponse(
      size_t num_tokens) {
    GetProbabilisticRevealTokenResponse response_proto;
    {
      for (size_t i = 0; i < num_tokens; ++i) {
        GetProbabilisticRevealTokenResponse_ProbabilisticRevealToken* token =
            response_proto.add_tokens();
        token->set_version(1);
        std::string i_string = base::NumberToString(i);
        // Pad the token string to 33 characters (including "token", the index,
        // and "-u"). Example: "token0-u---------------------".
        int padding_length = 26 - i_string.size();
        std::string padding(padding_length, '-');
        token->set_u(base::StrCat({"token", i_string, "-u", padding}));
        token->set_e(base::StrCat({"token", i_string, "-e", padding}));
      }
      response_proto.mutable_public_key()->set_y(public_key_bytes_);
      response_proto.mutable_expiration_time()->set_seconds(expiration_time_);
      response_proto.mutable_next_epoch_start_time()->set_seconds(
          next_epoch_start_time_);
      response_proto.set_num_tokens_with_signal(num_tokens);
      response_proto.set_epoch_id(std::string(8, '0'));
    }
    return response_proto;
  }

  std::unique_ptr<IpProtectionProbabilisticRevealTokenDirectFetcher> fetcher_;
  std::string public_key_bytes_;
  const uint64_t expiration_time_ =
      (base::Time::Now() + base::Hours(10)).InSecondsFSinceUnixEpoch();
  const uint64_t next_epoch_start_time_ =
      (base::Time::Now() + base::Hours(12)).InSecondsFSinceUnixEpoch();
};

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensSuccess) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(future.Get<0>());
  const auto& outcome = future.Get<0>().value();
  const auto& tokens = outcome.tokens;
  EXPECT_EQ(tokens.size(), 10UL);
  EXPECT_EQ(tokens[0].version, 1);
  EXPECT_THAT(tokens[0].u, StartsWith("token0-u"));
  EXPECT_THAT(tokens[0].e, StartsWith("token0-e"));
  EXPECT_EQ(tokens[1].version, 1);
  EXPECT_THAT(tokens[1].u, StartsWith("token1-u"));
  EXPECT_THAT(tokens[1].e, StartsWith("token1-e"));

  EXPECT_EQ(outcome.public_key, public_key_bytes_);
  EXPECT_EQ(outcome.expiration_time_seconds, expiration_time_);
  EXPECT_EQ(outcome.next_epoch_start_time_seconds, next_epoch_start_time_);
  EXPECT_EQ(outcome.num_tokens_with_signal, 10);
  EXPECT_EQ(outcome.epoch_id, std::string(8, '0'));

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetProbabilisticRevealTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensTooFewTokens) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/9);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kTooFewTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensTooManyTokens) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/401);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kTooManyTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensExpirationTooSoon) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  uint64_t expiration_time =
      (base::Time::Now() + base::Hours(1)).InSecondsFSinceUnixEpoch();
  response_proto.mutable_expiration_time()->set_seconds(expiration_time);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kExpirationTooSoon);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensExpirationTooLate) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  uint64_t expiration_time =
      (base::Time::Now() + base::Days(10)).InSecondsFSinceUnixEpoch();
  response_proto.mutable_expiration_time()->set_seconds(expiration_time);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kExpirationTooLate);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensNumTokensWithSignalTooSmall) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  response_proto.set_num_tokens_with_signal(-1);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kInvalidNumTokensWithSignal);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensNumTokensWithSignalTooLarge) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  response_proto.set_num_tokens_with_signal(11);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kInvalidNumTokensWithSignal);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensInvalidPublicKey) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  response_proto.mutable_public_key()->set_y("invalid_public_key");
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kInvalidPublicKey);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensInvalidTokenVersion) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  response_proto.mutable_tokens()->at(0).set_version(2);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kInvalidTokenVersion);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensInvalidTokenUSize) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  response_proto.mutable_tokens()->at(0).set_u("invalid_u");
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kInvalidTokenSize);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensInvalidTokenESize) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  response_proto.mutable_tokens()->at(0).set_e("invalid_e");
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kInvalidTokenSize);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensDefaultResponseValues) {
  GetProbabilisticRevealTokenResponse response_proto;
  std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());

  // The default response will parse successfully, but will fail the first
  // validation check (which checks the number of tokens).
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kTooFewTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensEmptyResponse) {
  std::string response_str = "";
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());

  // An empty response will parse successfully, but will fail the first
  // validation check (which checks the number of tokens).
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kTooFewTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensMultipleTokens) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/400);
  response_proto.set_num_tokens_with_signal(123);
  const std::string response_str = response_proto.SerializeAsString();
  // When last checked (03/25/2025) response_str.size() is 29659.
  ASSERT_LT(response_str.size(),
            std::size_t(kGetProbabilisticRevealTokenResponseMaxBodySize));

  SetResponse(response_str);
  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(future.Get<0>());
  const auto& outcome = future.Get<0>().value();
  const auto& tokens = outcome.tokens;
  ASSERT_EQ(tokens.size(), 400UL);
  EXPECT_EQ(tokens[42].version, 1);
  EXPECT_EQ(tokens[399].version, 1);
  EXPECT_THAT(tokens[19].u, StartsWith("token19-u"));
  EXPECT_THAT(tokens[399].u, StartsWith("token399-u"));
  EXPECT_THAT(tokens[23].e, StartsWith("token23-e"));
  EXPECT_THAT(tokens[399].e, StartsWith("token399-e"));

  EXPECT_EQ(outcome.public_key, public_key_bytes_);
  EXPECT_EQ(outcome.expiration_time_seconds, expiration_time_);
  EXPECT_EQ(outcome.next_epoch_start_time_seconds, next_epoch_start_time_);
  EXPECT_EQ(outcome.num_tokens_with_signal, 123);

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetProbabilisticRevealTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensResponseOverLimit) {
  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/1000);
  const std::string response_str = response_proto.SerializeAsString();
  // When last checked (01/24/2025) response_str.size() is 66047.
  ASSERT_GT(response_str.size(),
            std::size_t(kGetProbabilisticRevealTokenResponseMaxBodySize));

  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  // url_loader->DownloadToString() will fail
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetProbabilisticRevealTokensStatus::kNetNotOk);
  EXPECT_EQ(result.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);
  EXPECT_EQ(result.try_again_after, base::Time::Now() + base::Minutes(1));
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensInvalidResponse) {
  const std::string response_str = "invalid-response";
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kResponseParsingFailed);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       NullExperimentArm) {
  SetFeatureParameters({});
  ASSERT_EQ(expected_experiment_arm_, std::nullopt);

  GetProbabilisticRevealTokenResponse response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/10);
  const std::string response_str = response_proto.SerializeAsString();
  // Interceptor callback verifies experiment arm is nullopt.
  SetResponse(response_str);
  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetProbabilisticRevealTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionProbabilisticRevealTokenDirectFetcherTest,
       TryGetProbabilisticRevealTokensBackoff) {
  // Set a response that is too large to be downloaded.
  GetProbabilisticRevealTokenResponse large_response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/1000);
  std::string response_str = large_response_proto.SerializeAsString();
  ASSERT_GT(response_str.size(),
            std::size_t(kGetProbabilisticRevealTokenResponseMaxBodySize));
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  // url_loader->DownloadToString() will fail.
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // First call - network error, backoff 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result1 = future.Get<1>();
  EXPECT_EQ(result1.status, TryGetProbabilisticRevealTokensStatus::kNetNotOk);
  EXPECT_EQ(result1.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  base::Time backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result1.try_again_after, backoff_time);

  future.Clear();
  task_environment_.FastForwardBy(base::Seconds(59));
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Second call - too soon, no request made, backoff same as before.
  EXPECT_FALSE(future.Get<0>());
  const auto& result2 = future.Get<1>();
  EXPECT_EQ(result2.status,
            TryGetProbabilisticRevealTokensStatus::kRequestBackedOff);
  EXPECT_EQ(result2.network_error_code, net::OK);
  EXPECT_EQ(result2.try_again_after, backoff_time);

  future.Clear();
  task_environment_.FastForwardBy(base::Seconds(1));
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Third call - request made, another network error, backoff 2 minutes.
  EXPECT_FALSE(future.Get<0>());
  const auto& result3 = future.Get<1>();
  EXPECT_EQ(result3.status, TryGetProbabilisticRevealTokensStatus::kNetNotOk);
  EXPECT_EQ(result3.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  backoff_time = base::Time::Now() + base::Minutes(2);
  EXPECT_EQ(result3.try_again_after, backoff_time);

  future.Clear();
  task_environment_.FastForwardBy(base::Minutes(2));

  // Set a valid response.
  GetProbabilisticRevealTokenResponse valid_response_proto =
      BuildProbabilisticRevealTokenResponse(/*num_tokens=*/10);
  response_str = valid_response_proto.SerializeAsString();
  SetResponse(response_str);

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Fourth call - request successful, backoff reset.
  EXPECT_TRUE(future.Get<0>());
  const auto& result4 = future.Get<1>();
  EXPECT_EQ(result4.status, TryGetProbabilisticRevealTokensStatus::kSuccess);
  EXPECT_EQ(result4.network_error_code, net::OK);
  EXPECT_EQ(result4.try_again_after, std::nullopt);

  future.Clear();

  // Set a large response again.
  response_str = large_response_proto.SerializeAsString();
  SetResponse(response_str);

  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Fifth call - network error, backoff back to 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result5 = future.Get<1>();
  EXPECT_EQ(result5.status, TryGetProbabilisticRevealTokensStatus::kNetNotOk);
  EXPECT_EQ(result5.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result5.try_again_after, backoff_time);
}

TEST_F(
    IpProtectionProbabilisticRevealTokenDirectFetcherTest,
    TryGetProbabilisticRevealTokensAccountStatusChangedWithAccountAvailable) {
  // Set a response that is too large to be downloaded.
  GetProbabilisticRevealTokenResponse large_response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/1000);
  std::string response_str = large_response_proto.SerializeAsString();
  ASSERT_GT(response_str.size(),
            std::size_t(kGetProbabilisticRevealTokenResponseMaxBodySize));
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  // url_loader->DownloadToString() will fail.
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // First call - network error, backoff 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result1 = future.Get<1>();
  EXPECT_EQ(result1.status, TryGetProbabilisticRevealTokensStatus::kNetNotOk);
  EXPECT_EQ(result1.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  base::Time backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result1.try_again_after, backoff_time);

  future.Clear();

  // Set a valid response.
  GetProbabilisticRevealTokenResponse valid_response_proto =
      BuildProbabilisticRevealTokenResponse(/*num_tokens=*/10);
  response_str = valid_response_proto.SerializeAsString();
  SetResponse(response_str);

  // Retry request immediately after account status change.
  fetcher_->AccountStatusChanged(true);
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Second call - request successful without being backed off.
  EXPECT_TRUE(future.Get<0>());
  const auto& result2 = future.Get<1>();
  EXPECT_EQ(result2.status, TryGetProbabilisticRevealTokensStatus::kSuccess);
  EXPECT_EQ(result2.network_error_code, net::OK);
  EXPECT_EQ(result2.try_again_after, std::nullopt);
}

TEST_F(
    IpProtectionProbabilisticRevealTokenDirectFetcherTest,
    TryGetProbabilisticRevealTokensAccountStatusChangedWithAccountUnavailable) {
  // Set a response that is too large to be downloaded.
  GetProbabilisticRevealTokenResponse large_response_proto =
      BuildProbabilisticRevealTokenResponse(
          /*num_tokens=*/1000);
  std::string response_str = large_response_proto.SerializeAsString();
  ASSERT_GT(response_str.size(),
            std::size_t(kGetProbabilisticRevealTokenResponseMaxBodySize));
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;

  // url_loader->DownloadToString() will fail.
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // First call - network error, backoff 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result1 = future.Get<1>();
  EXPECT_EQ(result1.status, TryGetProbabilisticRevealTokensStatus::kNetNotOk);
  EXPECT_EQ(result1.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  base::Time backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result1.try_again_after, backoff_time);

  future.Clear();

  // Set a valid response.
  GetProbabilisticRevealTokenResponse valid_response_proto =
      BuildProbabilisticRevealTokenResponse(/*num_tokens=*/10);
  response_str = valid_response_proto.SerializeAsString();
  SetResponse(response_str);

  // Retry request immediately after account status change.
  fetcher_->AccountStatusChanged(false);
  fetcher_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Second call - request backed off (backoff was not reset).
  EXPECT_FALSE(future.Get<0>());
  const auto& result2 = future.Get<1>();
  EXPECT_EQ(result2.status,
            TryGetProbabilisticRevealTokensStatus::kRequestBackedOff);
  EXPECT_EQ(result2.network_error_code, net::OK);
  EXPECT_EQ(result2.try_again_after, backoff_time);
}

}  // namespace ip_protection
