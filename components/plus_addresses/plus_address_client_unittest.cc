// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace plus_addresses {

// PlusAddressParsing tests validate the ParsePlusAddressFrom* methods
// Returns empty when the DataDecoder fails to parse the JSON.
TEST(PlusAddressParsing, NotValidJson) {
  data_decoder::DataDecoder::ValueOrError error = base::unexpected("error!");
  EXPECT_EQ(PlusAddressClient::ParsePlusAddressFromV1CreateForTesting(
                std::move(error)),
            absl::nullopt);
}

// Success case - Returns the plus address.
TEST(PlusAddressParsing, FromV1Create_ParsesSuccessfully) {
  absl::optional<base::Value> perfect = base::JSONReader::Read(R"(
    {
      "plusProfile": [
        {
          "unwanted": 123,
          "plusEmail" : {
            "plusAddress": "foobar"
          }
        }
      ],
      "unwanted": "abc"
    }
    )");
  ASSERT_TRUE(perfect.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(perfect.value());
  EXPECT_EQ(PlusAddressClient::ParsePlusAddressFromV1CreateForTesting(
                std::move(value)),
            absl::make_optional("foobar"));
}

// Returns empty if there are no Profile objects.
TEST(PlusAddressParsing, FromV1Create_FailsWithoutAddress) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile": [
        {
          "plusEmail" : {
          }
        }
      ]
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(PlusAddressClient::ParsePlusAddressFromV1CreateForTesting(
                std::move(value)),
            absl::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsWithoutEmailObject) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile": [
        {
          "address": "foobar"
        }
      ]
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(PlusAddressClient::ParsePlusAddressFromV1CreateForTesting(
                std::move(value)),
            absl::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsWithEmptyProfileList) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile": []
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(PlusAddressClient::ParsePlusAddressFromV1CreateForTesting(
                std::move(value)),
            absl::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsWithoutProfile) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"(
      {
        "address": "wouldnt this be nice?"
      }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(PlusAddressClient::ParsePlusAddressFromV1CreateForTesting(
                std::move(value)),
            absl::nullopt);
}

// Tests that use fake out the URL loading and issues requests to the enterprise
// provided server.
class PlusAddressClientRequests : public ::testing::Test {
 public:
  PlusAddressClientRequests()
      : scoped_shared_url_loader_factory(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory)),
        identity_manager(identity_test_env.identity_manager()) {
    test_url_loader_factory.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request = request;
        }));

    features_.InitAndEnableFeatureWithParameters(
        kFeature, {{kEnterprisePlusAddressServerUrl.name, server_base_url}});
  }

 protected:
  // Not used directly, but required for `IdentityTestEnvironment` to work.
  base::test::TaskEnvironment task_environment;
  std::string server_base_url = "https://enterprise.foo/";
  std::string fullProfileEndpoint =
      base::StrCat({server_base_url, kServerPlusProfileEndpoint});
  std::string token = "myToken";
  signin::AccessTokenInfo eternal_token_info =
      signin::AccessTokenInfo(token, base::Time::Max(), "");

  network::TestURLLoaderFactory test_url_loader_factory;
  network::ResourceRequest last_request;

  signin::IdentityTestEnvironment identity_test_env;
  scoped_refptr<network::SharedURLLoaderFactory>
      scoped_shared_url_loader_factory;
  raw_ptr<signin::IdentityManager> identity_manager;

 private:
  base::test::ScopedFeatureList features_;
  data_decoder::test::InProcessDataDecoder decoder_;
};

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, CreatePlusProfileV1_IssuesCorrectRequest) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  std::string site = "https://foobar.com";
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.CreatePlusAddress(site, base::DoNothing());

  // Validate that the V1 Create request uses the right url and requests method.
  EXPECT_EQ(last_request.url, fullProfileEndpoint);
  EXPECT_EQ(last_request.method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes "myToken".
  std::string authorization_value;
  last_request.headers.GetHeader("Authorization", &authorization_value);
  EXPECT_EQ(authorization_value, "Bearer " + token);

  // Validate the request payload.
  ASSERT_NE(last_request.request_body, nullptr);
  ASSERT_EQ(last_request.request_body->elements()->size(), 1u);
  absl::optional<base::Value> body =
      base::JSONReader::Read(last_request.request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  ASSERT_TRUE(body.has_value() && body->is_dict());
  std::string* facet_entry = body->GetDict().FindString("facet");
  ASSERT_NE(facet_entry, nullptr);
  EXPECT_EQ(*facet_entry, site);
}

// For tests that cover successful but unexpected server responses, see the
// PlusAddressParsing.FromV1Create tests.
TEST_F(PlusAddressClientRequests, CreatePlusProfileV1_RunsCallbackOnSuccess) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  std::string site = "https://foobar.com";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.CreatePlusAddress(site, on_response_parsed.Get());
  // Fulfill the request and the callback should be run
  EXPECT_CALL(on_response_parsed, Run("plusone@plus.plus")).Times(1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"(
    {
      "plusProfile": [
        {
          "unwanted": 123,
          "plusEmail" : {
            "plusAddress": "plusone@plus.plus"
          }
        }
      ],
      "unwanted": "abc"
    }
    )");
}

// TODO (kaklilu): Add tests verifying behavior when request times out or the
// response is too large to be downloaded.
TEST_F(PlusAddressClientRequests,
       CreatePlusProfileV1_FailedRequestDoesntRunCallback) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  std::string site = "https://foobar.com";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.CreatePlusAddress(site, on_response_parsed.Get());

  // The request fails and the callback is never run
  EXPECT_CALL(on_response_parsed, Run(testing::_)).Times(0);
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      GURL(fullProfileEndpoint),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND),
      network::CreateURLResponseHead(net::HTTP_NOT_FOUND), ""));
}

TEST(PlusAddressClient, ChecksUrlParamIsValidGurl) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  std::string server_url = "https://foo.com/";
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      kFeature, {{kEnterprisePlusAddressServerUrl.name, server_url}});
  PlusAddressClient client(
      identity_test_env.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  ASSERT_TRUE(client.GetServerUrlForTesting().has_value());
  EXPECT_EQ(client.GetServerUrlForTesting().value(), server_url);
}

TEST(PlusAddressClient, RejectsNonUrlStrings) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      kFeature, {{kEnterprisePlusAddressServerUrl.name, "kirubeldotcom"}});
  PlusAddressClient client(
      identity_test_env.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_FALSE(client.GetServerUrlForTesting().has_value());
}

class PlusAddressAuthToken : public ::testing::Test {
 public:
  PlusAddressAuthToken() {
    // Init the feature param to add `test_scope_` to GetUnconsentedOAuth2Scopes
    features_.InitAndEnableFeatureWithParameters(
        kFeature, {{kEnterprisePlusAddressOAuthScope.name, test_scope_}});

    // Time-travel back to 1970 so that we can test with base::Time::FromDoubleT
    clock_.SetNow(base::Time::FromDoubleT(1));
  }

 protected:
  // A blocking helper that signs the user in and gets an OAuth token with our
  // test scope.
  // Note: this blocks indefinitely if there are no listeners for token
  // creation. This means it must be called after GetAuthToken.
  void WaitForSignInAndToken() {
    identity_test_env_.MakePrimaryAccountAvailable(
        test_email_address_, signin::ConsentLevel::kSignin);
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            test_token_, test_token_expiration_time_, "id", test_scopes_);
  }

  // A blocking helper that gets an OAuth token for our test scope that expires
  // at `expiration_time`.
  void WaitForToken(base::Time expiration_time) {
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            test_token_, expiration_time, "id", test_scopes_);
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  base::Clock* test_clock() { return &clock_; }

  std::string test_token_ = "access_token";
  std::string test_scope_ = "https://googleapis.com/test.scope";
  signin::ScopeSet test_scopes_ = {test_scope_};
  base::Time test_token_expiration_time_ = base::Time::FromDoubleT(1000);

 private:
  // Required by `signin::IdentityTestEnvironment`.
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  base::test::ScopedFeatureList features_;
  base::SimpleTestClock clock_;
  std::string test_email_address_ = "foo@gmail.com";
};

TEST_F(PlusAddressAuthToken, RequestedBeforeSignin) {
  PlusAddressClient client(
      identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());

  bool ran_callback = false;
  client.GetAuthToken(
      base::BindLambdaForTesting([&]() { ran_callback = true; }));

  // The callback is run only after signin.
  EXPECT_FALSE(ran_callback);
  WaitForSignInAndToken();
  EXPECT_TRUE(ran_callback);
}

TEST_F(PlusAddressAuthToken, RequestedUserNeverSignsIn) {
  PlusAddressClient client(
      identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);
  client.GetAuthToken(callback.Get());
}

TEST_F(PlusAddressAuthToken, RequestedAfterExpiration) {
  PlusAddressClient client(
      identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  // Make an initial OAuth token request.
  base::MockOnceClosure first_callback;
  client.GetAuthToken(first_callback.Get());
  EXPECT_CALL(first_callback, Run).Times(1);

  // Sign in, get a token, and fast-forward to after it is expired.
  WaitForSignInAndToken();
  base::Time now = test_token_expiration_time_ + base::Seconds(1);
  AdvanceTimeTo(now);

  // Issue another request for an OAuth token.
  base::MockOnceClosure second_callback;
  client.GetAuthToken(second_callback.Get());

  // Callback is only run once the new OAuth token request has completed.
  EXPECT_CALL(second_callback, Run).Times(1);
  WaitForToken(/*expiration_time=*/now + base::Hours(1));
}

}  // namespace plus_addresses
