// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_issuer_token_direct_fetcher.h"

#include <iostream>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
// The ASSIGN_OR_RETURN macro is defined in the both the base::expected code and
// the private-join-and-compute code. We need to undefine the macro here to
// avoid compiler errors.
#undef ASSIGN_OR_RETURN
#include "components/ip_protection/common/ip_protection_crypter.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/private-join-and-compute/src/crypto/big_num.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

using ::private_join_and_compute::BigNum;
using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::private_join_and_compute::elgamal::PublicKey;
using ::testing::StartsWith;

constexpr char kIssuerServerUrl[] =
    "https://prod.issuertoken.goog/v1/ipblinding/getIssuerToken";

// Helpers for creating an ElGamal public key.
absl::StatusOr<BigNum> CreateBigNum(const ECGroup& group, uint64_t n) {
  const BigNum order = group.GetOrder();
  const BigNum one = order >> (order.BitLength() - 1);
  if (!one.IsOne()) {
    return absl::InternalError("number is expected to be 1");
  }
  const BigNum zero = one >> 1;
  if (!zero.IsZero()) {
    return absl::InternalError("number is expected to be 0");
  }
  // Build big number from zero and one.
  BigNum bn = zero;
  for (size_t i = 0; i < 64; ++i) {
    if (n & (uint64_t(1) << i)) {
      bn += (one << i);
    }
  }
  ASSIGN_OR_RETURN(uint64_t iiv, bn.ToIntValue());
  if (iiv != n) {
    return absl::InternalError(
        "Number should fit uint64 and must be same as n.");
  }
  return bn;
}

absl::StatusOr<ECPoint> Exponent(const ECGroup& group,
                                 const ECPoint& point,
                                 uint64_t n) {
  ASSIGN_OR_RETURN(BigNum bn, CreateBigNum(group, n));
  return point.Mul(bn);
}

absl::StatusOr<std::string> GetTestPublicKeyBytes(uint64_t private_key) {
  auto context = std::make_unique<Context>();
  ASSIGN_OR_RETURN(ECGroup group,
                   ECGroup::Create(NID_secp224r1, context.get()));
  ASSIGN_OR_RETURN(ECPoint g, group.GetFixedGenerator());
  ASSIGN_OR_RETURN(ECPoint y, Exponent(group, g, private_key));
  PublicKey public_key = PublicKey{std::move(g), std::move(y)};
  ASSIGN_OR_RETURN(std::string public_key_bytes,
                   SerializePublicKey(public_key));
  return public_key_bytes;
}

class FetcherTestBase : public testing::Test {
 protected:
  // SetUp with default feature parameters, i.e., IPP enabled and experiment arm
  // is 42.
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters[net::features::kIpPrivacyDebugExperimentArm.name] = "42";
    SetFeatureParameters(std::move(parameters));
    token_server_get_issuer_token_url_ = GURL(kIssuerServerUrl);
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
  void SetResponse(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.url.is_valid());
          EXPECT_EQ(request.url, token_server_get_issuer_token_url_);
          EXPECT_EQ(request.method, net::HttpRequestHeaders::kPostMethod);
          EXPECT_EQ(request.credentials_mode,
                    network::mojom::CredentialsMode::kOmit);
          EXPECT_THAT(
              request.headers.GetHeader(net::HttpRequestHeaders::kAccept),
              testing::Optional(std::string("application/x-protobuf")));
          EXPECT_THAT(
              request.headers.GetHeader("Ip-Protection-Debug-Experiment-Arm"),
              expected_experiment_arm_);
          ASSERT_TRUE(request.request_body);
          GetIssuerTokenRequest request_proto;
          ASSERT_TRUE(request_proto.ParseFromString(GetUploadData(request)));
          ASSERT_TRUE(request_proto.has_service_type());
          EXPECT_EQ(request_proto.service_type(),
                    GetIssuerTokenRequest_ServiceType_CHROME);
          auto head = network::mojom::URLResponseHead::New();
          test_url_loader_factory_.AddResponse(
              token_server_get_issuer_token_url_, std::move(head), response,
              network::URLLoaderCompletionStatus(net::OK));
        }));
  }

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::optional<std::string> expected_experiment_arm_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  GURL token_server_get_issuer_token_url_;
};

class IpProtectionIssuerTokenDirectFetcherRetrieverTest
    : public FetcherTestBase {
 public:
  void SetFeatureParameters(
      std::map<std::string, std::string> parameters) override {
    FetcherTestBase::SetFeatureParameters(parameters);
    retriever_ =
        std::make_unique<IpProtectionIssuerTokenDirectFetcher::Retriever>(
            test_url_loader_factory_.GetSafeWeakWrapper()->Clone());
  }

  std::unique_ptr<IpProtectionIssuerTokenDirectFetcher::Retriever> retriever_;
};

TEST_F(IpProtectionIssuerTokenDirectFetcherRetrieverTest, NullExperimentArm) {
  SetFeatureParameters({});
  ASSERT_EQ(expected_experiment_arm_, std::nullopt);
  const std::string response_str = "";
  // Interceptor callback asserts the experiment arm value is nullopt.
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  // Call RetrieveIssuerToken() to trigger interceptor callback.
  retriever_->RetrieveIssuerToken(result_future.GetCallback());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherRetrieverTest,
       RetrieveIssuerTokenSuccess) {
  const std::string response_str = "some response";
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveIssuerToken(result_future.GetCallback());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherRetrieverTest, EmptyResponse) {
  const std::string response_str = "";
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveIssuerToken(result_future.GetCallback());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherRetrieverTest,
       LargeResponseWithinLimit) {
  const std::string response_str = std::string(32 * 1024, 'a');
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveIssuerToken(result_future.GetCallback());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  EXPECT_EQ(result.value().value(), response_str);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherRetrieverTest,
       LargeResponseOverLimit) {
  const std::string response_str = std::string(32 * 1024 + 1, 'a');
  SetResponse(response_str);
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveIssuerToken(result_future.GetCallback());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), net::ERR_INSUFFICIENT_RESOURCES);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherRetrieverTest, OutOfMemory) {
  const std::string response_str = "";
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            token_server_get_issuer_token_url_, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::ERR_OUT_OF_MEMORY));
      }));
  base::test::TestFuture<base::expected<std::optional<std::string>, int>>
      result_future;
  retriever_->RetrieveIssuerToken(result_future.GetCallback());
  base::expected<std::optional<std::string>, int> result = result_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), net::ERR_OUT_OF_MEMORY);
}

class IpProtectionIssuerTokenDirectFetcherTest : public FetcherTestBase {
 public:
  void SetFeatureParameters(
      std::map<std::string, std::string> parameters) override {
    FetcherTestBase::SetFeatureParameters(parameters);
    fetcher_ = std::make_unique<IpProtectionIssuerTokenDirectFetcher>(
        test_url_loader_factory_.GetSafeWeakWrapper()->Clone());

    absl::StatusOr<std::string> maybe_public_key_bytes =
        GetTestPublicKeyBytes(/*private_key=*/1);
    ASSERT_TRUE(maybe_public_key_bytes.ok());
    public_key_bytes_ = maybe_public_key_bytes.value();
  }

  GetIssuerTokenResponse BuildIssuerTokenResponse(size_t num_tokens) {
    GetIssuerTokenResponse response_proto;
    {
      for (size_t i = 0; i < num_tokens; ++i) {
        GetIssuerTokenResponse_IssuerToken* issuerToken =
            response_proto.add_tokens();
        issuerToken->set_version(1);
        std::string i_string = base::NumberToString(i);
        // Pad the token string to 29 characters (including "token", the index,
        // and "-u"). Example: "token0-u---------------------".
        int padding_length = 22 - i_string.size();
        std::string padding(padding_length, '-');
        issuerToken->set_u(base::StrCat({"token", i_string, "-u", padding}));
        issuerToken->set_e(base::StrCat({"token", i_string, "-e", padding}));
      }
      response_proto.mutable_public_key()->set_y(public_key_bytes_);
      response_proto.set_expiration_time_seconds(expiration_time_);
      response_proto.set_next_epoch_start_time_seconds(next_epoch_start_time_);
      response_proto.set_num_tokens_with_signal(num_tokens);
    }
    return response_proto;
  }

  std::unique_ptr<IpProtectionIssuerTokenDirectFetcher> fetcher_;
  std::string public_key_bytes_;
  const uint64_t expiration_time_ =
      (base::Time::Now() + base::Hours(10)).InSecondsFSinceUnixEpoch();
  const uint64_t next_epoch_start_time_ =
      (base::Time::Now() + base::Hours(12)).InSecondsFSinceUnixEpoch();
};

TEST_F(IpProtectionIssuerTokenDirectFetcherTest, TryGetIssuerTokensSuccess) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
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

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensTooFewTokens) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/9);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kTooFewTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensTooManyTokens) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/401);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kTooManyTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensExpirationTooSoon) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  uint64_t expiration_time =
      (base::Time::Now() + base::Hours(1)).InSecondsFSinceUnixEpoch();
  response_proto.set_expiration_time_seconds(expiration_time);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kExpirationTooSoon);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensExpirationTooLate) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  uint64_t expiration_time =
      (base::Time::Now() + base::Days(10)).InSecondsFSinceUnixEpoch();
  response_proto.set_expiration_time_seconds(expiration_time);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kExpirationTooLate);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensNumTokensWithSignalTooSmall) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  response_proto.set_num_tokens_with_signal(-1);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetIssuerTokensStatus::kInvalidNumTokensWithSignal);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensNumTokensWithSignalTooLarge) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  response_proto.set_num_tokens_with_signal(11);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetIssuerTokensStatus::kInvalidNumTokensWithSignal);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensInvalidPublicKey) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  response_proto.mutable_public_key()->set_y("invalid_public_key");
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kInvalidPublicKey);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensInvalidTokenVersion) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  response_proto.mutable_tokens()->at(0).set_version(2);
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kInvalidTokenVersion);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensInvalidTokenUSize) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  response_proto.mutable_tokens()->at(0).set_u("invalid_u");
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kInvalidTokenSize);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensInvalidTokenESize) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  response_proto.mutable_tokens()->at(0).set_e("invalid_e");
  const std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get<0>());
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kInvalidTokenSize);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensDefaultResponseValues) {
  GetIssuerTokenResponse response_proto;
  std::string response_str = response_proto.SerializeAsString();
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  EXPECT_FALSE(future.Get<0>());

  // The default response will parse successfully, but will fail the first
  // validation check (which checks the number of tokens).
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kTooFewTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensEmptyResponse) {
  std::string response_str = "";
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());

  // An empty response will parse successfully, but will fail the first
  // validation check (which checks the number of tokens).
  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kTooFewTokens);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensMultipleTokens) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/400);
  response_proto.set_num_tokens_with_signal(123);
  const std::string response_str = response_proto.SerializeAsString();
  // When last checked (01/24/2025) response_str.size() is 26447.
  ASSERT_LT(response_str.size(), std::size_t(32 * 1024));

  SetResponse(response_str);
  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;
  fetcher_->TryGetIssuerTokens(future.GetCallback());
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
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensResponseOverLimit) {
  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/1000);
  const std::string response_str = response_proto.SerializeAsString();
  // When last checked (01/24/2025) response_str.size() is 66047.
  ASSERT_GT(response_str.size(), std::size_t(32 * 1024));

  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  // url_loader->DownloadToString() will fail
  fetcher_->TryGetIssuerTokens(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kNetNotOk);
  EXPECT_EQ(result.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);
  EXPECT_EQ(result.try_again_after, base::Time::Now() + base::Minutes(1));
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensInvalidResponse) {
  const std::string response_str = "invalid-response";
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;
  fetcher_->TryGetIssuerTokens(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kResponseParsingFailed);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest, NullExperimentArm) {
  SetFeatureParameters({});
  ASSERT_EQ(expected_experiment_arm_, std::nullopt);

  GetIssuerTokenResponse response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/10);
  const std::string response_str = response_proto.SerializeAsString();
  // Interceptor callback verifies experiment arm is nullopt.
  SetResponse(response_str);
  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest, TryGetIssuerTokensBackoff) {
  // Set a response that is too large to be downloaded.
  GetIssuerTokenResponse large_response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/1000);
  std::string response_str = large_response_proto.SerializeAsString();
  ASSERT_GT(response_str.size(), std::size_t(32 * 1024));
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  // url_loader->DownloadToString() will fail.
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // First call - network error, backoff 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result1 = future.Get<1>();
  EXPECT_EQ(result1.status, TryGetIssuerTokensStatus::kNetNotOk);
  EXPECT_EQ(result1.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  base::Time backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result1.try_again_after, backoff_time);

  future.Clear();
  task_environment_.FastForwardBy(base::Seconds(59));
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Second call - too soon, no request made, backoff same as before.
  EXPECT_FALSE(future.Get<0>());
  const auto& result2 = future.Get<1>();
  EXPECT_EQ(result2.status, TryGetIssuerTokensStatus::kRequestBackedOff);
  EXPECT_EQ(result2.network_error_code, net::OK);
  EXPECT_EQ(result2.try_again_after, backoff_time);

  future.Clear();
  task_environment_.FastForwardBy(base::Seconds(1));
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Third call - request made, another network error, backoff 2 minutes.
  EXPECT_FALSE(future.Get<0>());
  const auto& result3 = future.Get<1>();
  EXPECT_EQ(result3.status, TryGetIssuerTokensStatus::kNetNotOk);
  EXPECT_EQ(result3.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  backoff_time = base::Time::Now() + base::Minutes(2);
  EXPECT_EQ(result3.try_again_after, backoff_time);

  future.Clear();
  task_environment_.FastForwardBy(base::Minutes(2));

  // Set a valid response.
  GetIssuerTokenResponse valid_response_proto =
      BuildIssuerTokenResponse(/*num_tokens=*/10);
  response_str = valid_response_proto.SerializeAsString();
  SetResponse(response_str);

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Fourth call - request successful, backoff reset.
  EXPECT_TRUE(future.Get<0>());
  const auto& result4 = future.Get<1>();
  EXPECT_EQ(result4.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result4.network_error_code, net::OK);
  EXPECT_EQ(result4.try_again_after, std::nullopt);

  future.Clear();

  // Set a large response again.
  response_str = large_response_proto.SerializeAsString();
  SetResponse(response_str);

  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Fifth call - network error, backoff back to 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result5 = future.Get<1>();
  EXPECT_EQ(result5.status, TryGetIssuerTokensStatus::kNetNotOk);
  EXPECT_EQ(result5.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result5.try_again_after, backoff_time);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensAccountStatusChangedWithAccountAvailable) {
  // Set a response that is too large to be downloaded.
  GetIssuerTokenResponse large_response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/1000);
  std::string response_str = large_response_proto.SerializeAsString();
  ASSERT_GT(response_str.size(), std::size_t(32 * 1024));
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  // url_loader->DownloadToString() will fail.
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // First call - network error, backoff 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result1 = future.Get<1>();
  EXPECT_EQ(result1.status, TryGetIssuerTokensStatus::kNetNotOk);
  EXPECT_EQ(result1.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  base::Time backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result1.try_again_after, backoff_time);

  future.Clear();

  // Set a valid response.
  GetIssuerTokenResponse valid_response_proto =
      BuildIssuerTokenResponse(/*num_tokens=*/10);
  response_str = valid_response_proto.SerializeAsString();
  SetResponse(response_str);

  // Retry request immediately after account status change.
  fetcher_->AccountStatusChanged(true);
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Second call - request successful without being backed off.
  EXPECT_TRUE(future.Get<0>());
  const auto& result2 = future.Get<1>();
  EXPECT_EQ(result2.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result2.network_error_code, net::OK);
  EXPECT_EQ(result2.try_again_after, std::nullopt);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensAccountStatusChangedWithAccountUnavailable) {
  // Set a response that is too large to be downloaded.
  GetIssuerTokenResponse large_response_proto = BuildIssuerTokenResponse(
      /*num_tokens=*/1000);
  std::string response_str = large_response_proto.SerializeAsString();
  ASSERT_GT(response_str.size(), std::size_t(32 * 1024));
  SetResponse(response_str);

  base::test::TestFuture<std::optional<TryGetIssuerTokensOutcome>,
                         TryGetIssuerTokensResult>
      future;

  // url_loader->DownloadToString() will fail.
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // First call - network error, backoff 1 minute.
  EXPECT_FALSE(future.Get<0>());
  const auto& result1 = future.Get<1>();
  EXPECT_EQ(result1.status, TryGetIssuerTokensStatus::kNetNotOk);
  EXPECT_EQ(result1.network_error_code, net::ERR_INSUFFICIENT_RESOURCES);

  base::Time backoff_time = base::Time::Now() + base::Minutes(1);
  EXPECT_EQ(result1.try_again_after, backoff_time);

  future.Clear();

  // Set a valid response.
  GetIssuerTokenResponse valid_response_proto =
      BuildIssuerTokenResponse(/*num_tokens=*/10);
  response_str = valid_response_proto.SerializeAsString();
  SetResponse(response_str);

  // Retry request immediately after account status change.
  fetcher_->AccountStatusChanged(false);
  fetcher_->TryGetIssuerTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Second call - request backed off (backoff was not reset).
  EXPECT_FALSE(future.Get<0>());
  const auto& result2 = future.Get<1>();
  EXPECT_EQ(result2.status, TryGetIssuerTokensStatus::kRequestBackedOff);
  EXPECT_EQ(result2.network_error_code, net::OK);
  EXPECT_EQ(result2.try_again_after, backoff_time);
}

}  // namespace ip_protection
