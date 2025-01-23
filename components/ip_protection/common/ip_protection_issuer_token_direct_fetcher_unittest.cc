// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_issuer_token_direct_fetcher.h"

#include <iostream>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
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

namespace ip_protection {

constexpr char kIssuerServerUrl[] =
    "https://prod.issuertoken.goog/v1/ipblinding/getIssuerToken";

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
  base::test::TaskEnvironment task_environment_;
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
  }
  std::unique_ptr<IpProtectionIssuerTokenDirectFetcher> fetcher_;
};

TEST_F(IpProtectionIssuerTokenDirectFetcherTest, TryGetIssuerTokensSuccess) {
  GetIssuerTokenResponse response_proto;
  {
    GetIssuerTokenResponse_IssuerToken* issuerToken =
        response_proto.add_tokens();
    issuerToken->set_version(27);
    issuerToken->set_u("token0-u");
    issuerToken->set_e("token0-e");
    issuerToken = response_proto.add_tokens();
    issuerToken->set_version(42);
    issuerToken->set_u("token1-u");
    issuerToken->set_e("token1-e");
    response_proto.mutable_public_key()->set_y("pk-y");
    response_proto.set_expiration_time_seconds(123456);
    response_proto.set_next_epoch_start_time_seconds(333);
    response_proto.set_p_reveal(222);
  }
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
  EXPECT_EQ(tokens.size(), std::size_t(2));
  EXPECT_EQ(tokens[0].version, 27);
  EXPECT_EQ(tokens[0].u, "token0-u");
  EXPECT_EQ(tokens[0].e, "token0-e");
  EXPECT_EQ(tokens[1].version, 42);
  EXPECT_EQ(tokens[1].u, "token1-u");
  EXPECT_EQ(tokens[1].e, "token1-e");

  EXPECT_EQ(outcome.public_key, "pk-y");
  EXPECT_EQ(outcome.expiration_time_seconds, std::uint64_t(123456));
  EXPECT_EQ(outcome.next_epoch_start_time_seconds, std::uint64_t(333));
  EXPECT_EQ(outcome.p_reveal, 222);

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
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
  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(future.Get<0>());
  const auto& outcome = future.Get<0>().value();
  const auto& tokens = outcome.tokens;
  EXPECT_EQ(tokens.size(), std::size_t(0));
  EXPECT_EQ(outcome.public_key, "");
  EXPECT_EQ(outcome.expiration_time_seconds, std::uint64_t(0));
  EXPECT_EQ(outcome.next_epoch_start_time_seconds, std::uint64_t(0));
  EXPECT_EQ(outcome.p_reveal, 0);

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
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

  ASSERT_TRUE(future.Get<0>());
  const auto& outcome = future.Get<0>().value();
  const auto& tokens = outcome.tokens;
  EXPECT_EQ(tokens.size(), std::size_t(0));

  EXPECT_EQ(outcome.public_key, "");
  EXPECT_EQ(outcome.expiration_time_seconds, std::uint64_t(0));
  EXPECT_EQ(outcome.next_epoch_start_time_seconds, std::uint64_t(0));
  EXPECT_EQ(outcome.p_reveal, 0);

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensMultipleTokens) {
  GetIssuerTokenResponse response_proto;
  {
    // Add 400 tokens.
    for (int i = 0; i < 400; ++i) {
      GetIssuerTokenResponse_IssuerToken* issuerToken =
          response_proto.add_tokens();
      issuerToken->set_version(i);
      issuerToken->set_u(std::string(29, 'u'));
      issuerToken->set_e(std::string(29, 'e'));
    }
    response_proto.mutable_public_key()->set_y(std::string(29, 'y'));
    response_proto.set_expiration_time_seconds(112233);
    response_proto.set_next_epoch_start_time_seconds(445566);
    response_proto.set_p_reveal(1234);
  }

  const std::string response_str = response_proto.SerializeAsString();
  // When last checked (01/14/2025) response_str.size() is 26716.
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
  ASSERT_EQ(tokens.size(), std::size_t(400));
  EXPECT_EQ(tokens[42].version, 42);
  EXPECT_EQ(tokens[399].version, 399);
  EXPECT_EQ(tokens[19].u, std::string(29, 'u'));
  EXPECT_EQ(tokens[399].u, std::string(29, 'u'));
  EXPECT_EQ(tokens[23].e, std::string(29, 'e'));
  EXPECT_EQ(tokens[399].e, std::string(29, 'e'));

  EXPECT_EQ(outcome.public_key, std::string(29, 'y'));
  EXPECT_EQ(outcome.expiration_time_seconds, std::uint64_t(112233));
  EXPECT_EQ(outcome.next_epoch_start_time_seconds, std::uint64_t(445566));
  EXPECT_EQ(outcome.p_reveal, 1234);

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetIssuerTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest,
       TryGetIssuerTokensResponseOverLimit) {
  GetIssuerTokenResponse response_proto;
  {
    // Add 1000 tokens.
    for (int i = 0; i < 1000; ++i) {
      GetIssuerTokenResponse_IssuerToken* issuerToken =
          response_proto.add_tokens();
      issuerToken->set_version(27);
      issuerToken->set_u(std::string(29, 'u'));
      issuerToken->set_e(std::string(29, 'e'));
    }
    response_proto.mutable_public_key()->set_y(std::string(29, 'y'));
    response_proto.set_expiration_time_seconds(123456);
    response_proto.set_next_epoch_start_time_seconds(333);
    response_proto.set_p_reveal(222);
  }

  const std::string response_str = response_proto.SerializeAsString();
  // When last checked (01/14/2025) response_str.size() is 66043.
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
}

TEST_F(IpProtectionIssuerTokenDirectFetcherTest, NullExperimentArm) {
  SetFeatureParameters({});
  ASSERT_EQ(expected_experiment_arm_, std::nullopt);
  GetIssuerTokenResponse response_proto;
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
}

}  // namespace ip_protection
