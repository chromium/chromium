// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSerializedRequestProto[] = "serialized_request_proto";
const char kSerializedResponseProto[] = "result_proto";
const char kRequestUrl[] = "https://googleapis.com/nearbysharing/test";
const char kAccessToken[] = "access_token";
const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";
const char kGet[] = "GET";
const char kPost[] = "POST";
const char kPatch[] = "PATCH";

const ash::nearby::NearbyApiCallFlow::QueryParameters&
GetTestRequestProtoAsQueryParameters() {
  static const base::NoDestructor<
      ash::nearby::NearbyApiCallFlow::QueryParameters>
      request_as_query_parameters(
          {{"field1", "value1a"}, {"field1", "value1b"}, {"field2", "value2"}});
  return *request_as_query_parameters;
}

// Adds the "alt=proto" query parameters which specifies that the response
// should be formatted as a serialized proto. Adds the key-value pairs of
// |request_as_query_parameters| as query parameters.
// |request_as_query_parameters| is only non-null for GET requests.
GURL UrlWithQueryParameters(
    const std::string& url,
    const std::optional<ash::nearby::NearbyApiCallFlow::QueryParameters>&
        request_as_query_parameters) {
  GURL url_with_qp(url);

  url_with_qp =
      net::AppendQueryParameter(url_with_qp, kQueryParameterAlternateOutputKey,
                                kQueryParameterAlternateOutputProto);

  if (request_as_query_parameters) {
    for (const auto& key_value : *request_as_query_parameters) {
      url_with_qp = net::AppendQueryParameter(url_with_qp, key_value.first,
                                              key_value.second);
    }
  }

  return url_with_qp;
}

}  // namespace

namespace ash::nearby {

class NearbyApiCallFlowImplTest : public testing::Test {
 protected:
  NearbyApiCallFlowImplTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    flow_.SetPartialNetworkTrafficAnnotation(
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void StartPostRequestApiCallFlow() {
    StartPostRequestApiCallFlowWithSerializedRequest(kSerializedRequestProto);
  }

  void StartPostRequestApiCallFlowWithSerializedRequest(
      const std::string& serialized_request) {
    flow_.StartPostRequest(GURL(kRequestUrl), serialized_request,
                           shared_factory_, kAccessToken,
                           base::BindOnce(&NearbyApiCallFlowImplTest::OnResult,
                                          base::Unretained(this)),
                           base::BindOnce(&NearbyApiCallFlowImplTest::OnError,
                                          base::Unretained(this)));
    // A pending fetch for the API request should be created.
    CheckNearbySharingClientHttpPostRequest(serialized_request);
  }

  void StartPatchRequestApiCallFlow() {
    StartPatchRequestApiCallFlowWithSerializedRequest(kSerializedRequestProto);
  }

  void StartPatchRequestApiCallFlowWithSerializedRequest(
      const std::string& serialized_request) {
    flow_.StartPatchRequest(GURL(kRequestUrl), serialized_request,
                            shared_factory_, kAccessToken,
                            base::BindOnce(&NearbyApiCallFlowImplTest::OnResult,
                                           base::Unretained(this)),
                            base::BindOnce(&NearbyApiCallFlowImplTest::OnError,
                                           base::Unretained(this)));
    // A pending fetch for the API request should be created.
    CheckNearbySharingClientHttpPatchRequest(serialized_request);
  }

  void StartGetRequestApiCallFlow() {
    StartGetRequestApiCallFlowWithRequestAsQueryParameters(
        GetTestRequestProtoAsQueryParameters());
  }

  void StartGetRequestApiCallFlowWithRequestAsQueryParameters(
      const NearbyApiCallFlow::QueryParameters& request_as_query_parameters) {
    flow_.StartGetRequest(GURL(kRequestUrl), request_as_query_parameters,
                          shared_factory_, kAccessToken,
                          base::BindOnce(&NearbyApiCallFlowImplTest::OnResult,
                                         base::Unretained(this)),
                          base::BindOnce(&NearbyApiCallFlowImplTest::OnError,
                                         base::Unretained(this)));
    // A pending fetch for the API request should be created.
    CheckNearbySharingClientHttpGetRequest(request_as_query_parameters);
  }

  void OnResult(const std::string& result) {
    EXPECT_FALSE(result_ || network_error_);
    result_ = std::make_unique<std::string>(result);
  }

  void OnError(NearbyHttpError network_error) {
    EXPECT_FALSE(result_ || network_error_);
    network_error_ = std::make_unique<NearbyHttpError>(network_error);
  }

  void CheckPlatformTypeHeader(const net::HttpRequestHeaders& headers) {
    EXPECT_EQ("OSType.CHROME_OS", headers.GetHeader("X-Sharing-Platform-Type"));
  }

  void CheckNearbySharingClientHttpPostRequest(
      const std::string& serialized_request) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *test_url_loader_factory_.pending_requests();
    ASSERT_EQ(1u, pending.size());
    const network::ResourceRequest& request = pending[0].request;

    CheckPlatformTypeHeader(request.headers);

    EXPECT_EQ(UrlWithQueryParameters(
                  kRequestUrl, std::nullopt /* request_as_query_parameters */),
              request.url);

    EXPECT_EQ(kPost, request.method);

    EXPECT_EQ(serialized_request, network::GetUploadData(request));

    EXPECT_EQ("application/x-protobuf",
              request.headers.GetHeader(net::HttpRequestHeaders::kContentType));
  }

  void CheckNearbySharingClientHttpPatchRequest(
      const std::string& serialized_request) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *test_url_loader_factory_.pending_requests();
    ASSERT_EQ(1u, pending.size());
    const network::ResourceRequest& request = pending[0].request;

    CheckPlatformTypeHeader(request.headers);

    EXPECT_EQ(UrlWithQueryParameters(
                  kRequestUrl, std::nullopt /* request_as_query_parameters */),
              request.url);

    EXPECT_EQ(kPatch, request.method);

    EXPECT_EQ(serialized_request, network::GetUploadData(request));

    EXPECT_EQ("application/x-protobuf",
              request.headers.GetHeader(net::HttpRequestHeaders::kContentType));
  }

  void CheckNearbySharingClientHttpGetRequest(
      const NearbyApiCallFlow::QueryParameters& request_as_query_parameters) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *test_url_loader_factory_.pending_requests();
    ASSERT_EQ(1u, pending.size());
    const network::ResourceRequest& request = pending[0].request;

    CheckPlatformTypeHeader(request.headers);

    EXPECT_EQ(UrlWithQueryParameters(kRequestUrl, request_as_query_parameters),
              request.url);

    EXPECT_EQ(kGet, request.method);

    // Expect no body.
    EXPECT_TRUE(network::GetUploadData(request).empty());
    EXPECT_FALSE(
        request.headers.HasHeader(net::HttpRequestHeaders::kContentType));
  }

  // Responds to the current HTTP POST request. If the |error| is not net::OK,
  // then the |response_code| and |response_string| are null.
  void CompleteCurrentPostRequest(
      net::Error error,
      std::optional<int> response_code = std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt) {
    network::URLLoaderCompletionStatus completion_status(error);
    auto response_head = network::mojom::URLResponseHead::New();
    std::string content;
    if (error == net::OK) {
      response_head = network::CreateURLResponseHead(
          static_cast<net::HttpStatusCode>(*response_code));
      content = *response_string;
    }

    // Use kUrlMatchPrefix flag to match URL without query parameters.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrl), completion_status, std::move(response_head), content,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));

    task_environment_.RunUntilIdle();
    EXPECT_TRUE(result_ || network_error_);
  }

  // Responds to the current HTTP PATCH request. If the |error| is not net::OK,
  // then the |response_code| and |response_string| are null.
  void CompleteCurrentPatchRequest(
      net::Error error,
      std::optional<int> response_code = std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt) {
    network::URLLoaderCompletionStatus completion_status(error);
    auto response_head = network::mojom::URLResponseHead::New();
    std::string content;
    if (error == net::OK) {
      response_head = network::CreateURLResponseHead(
          static_cast<net::HttpStatusCode>(*response_code));
      content = *response_string;
    }

    // Use kUrlMatchPrefix flag to match URL without query parameters.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrl), completion_status, std::move(response_head), content,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));

    task_environment_.RunUntilIdle();
    EXPECT_TRUE(result_ || network_error_);
  }

  // Responds to the current HTTP GET request. If the |error| is not net::OK,
  // then the |response_code| and |response_string| are null.
  void CompleteCurrentGetRequest(
      net::Error error,
      std::optional<int> response_code = std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt) {
    network::URLLoaderCompletionStatus completion_status(error);
    auto response_head = network::mojom::URLResponseHead::New();
    std::string content;
    if (error == net::OK) {
      response_head = network::CreateURLResponseHead(
          static_cast<net::HttpStatusCode>(*response_code));
      content = *response_string;
    }

    // Use kUrlMatchPrefix flag to match URL without query parameters.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrl), completion_status, std::move(response_head), content,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));

    task_environment_.RunUntilIdle();
    EXPECT_TRUE(result_ || network_error_);
  }

  std::unique_ptr<std::string> result_;
  std::unique_ptr<NearbyHttpError> network_error_;

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  NearbyApiCallFlowImpl flow_;
};

TEST_F(NearbyApiCallFlowImplTest, PostRequestSuccess) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, PatchRequestSuccess) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, GetRequestSuccess) {
  StartGetRequestApiCallFlow();
  CompleteCurrentGetRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, PostRequestFailure) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::ERR_FAILED);
  EXPECT_FALSE(result_);
  EXPECT_EQ(NearbyHttpError::kOffline, *network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, PatchRequestFailure) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::ERR_FAILED);
  EXPECT_FALSE(result_);
  EXPECT_EQ(NearbyHttpError::kOffline, *network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, GetRequestFailure) {
  StartGetRequestApiCallFlow();
  CompleteCurrentPostRequest(net::ERR_FAILED);
  EXPECT_FALSE(result_);
  EXPECT_EQ(NearbyHttpError::kOffline, *network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, RequestStatus500) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                             "Nearby Sharing Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(NearbyHttpError::kInternalServerError, *network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, PatchRequestStatus500) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                              "Nearby Sharing Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(NearbyHttpError::kInternalServerError, *network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, GetRequestStatus500) {
  StartGetRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                             "Nearby Sharing Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(NearbyHttpError::kInternalServerError, *network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(NearbyApiCallFlowImplTest, PostRequestWithNoBody) {
  StartPostRequestApiCallFlowWithSerializedRequest(std::string());
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(NearbyApiCallFlowImplTest, PatchRequestWithNoBody) {
  StartPatchRequestApiCallFlowWithSerializedRequest(std::string());
  CompleteCurrentPatchRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

TEST_F(NearbyApiCallFlowImplTest, GetRequestWithNoQueryParameters) {
  StartGetRequestApiCallFlowWithRequestAsQueryParameters(
      {} /* request_as_query_parameters */);
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(NearbyApiCallFlowImplTest, PostResponseWithNoBody) {
  StartPostRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, std::string());
  EXPECT_EQ(std::string(), *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(NearbyApiCallFlowImplTest, PatchResponseWithNoBody) {
  StartPatchRequestApiCallFlow();
  CompleteCurrentPatchRequest(net::OK, net::HTTP_OK, std::string());
  EXPECT_EQ(std::string(), *result_);
  EXPECT_FALSE(network_error_);
}

// The empty string is a valid protocol buffer message serialization.
TEST_F(NearbyApiCallFlowImplTest, GetResponseWithNoBody) {
  StartGetRequestApiCallFlow();
  CompleteCurrentPostRequest(net::OK, net::HTTP_OK, std::string());
  EXPECT_EQ(std::string(), *result_);
  EXPECT_FALSE(network_error_);
}

}  // namespace ash::nearby
