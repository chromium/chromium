// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"

#include "base/test/scoped_feature_list.h"
#include "components/plus_addresses/features.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/plus_addresses/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
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
  std::string token = "myToken";
  client.CreatePlusAddressWithToken(site, base::DoNothing(), token);

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
  std::string site = "https://foobar.com";
  std::string token = "myToken";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.CreatePlusAddressWithToken(site, on_response_parsed.Get(), token);
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
  std::string site = "https://foobar.com";
  std::string token = "myToken";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.CreatePlusAddressWithToken(site, on_response_parsed.Get(), token);

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

}  // namespace plus_addresses
