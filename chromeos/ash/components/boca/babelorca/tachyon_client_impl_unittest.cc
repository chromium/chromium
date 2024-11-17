// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_client_impl.h"

#include <algorithm>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/proto/testing_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::babelorca {
namespace {

using RequestDataPtr = std::unique_ptr<RequestDataWrapper>;

constexpr char kOAuthToken[] = "oauth-token";
constexpr char kUrl[] = "https://test.com";
const net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("babelorca-testid",
                                        R"(semantics { sender "client test"})");

class TachyonClientImplTest : public testing::Test {
 protected:
  RequestDataPtr request_data() {
    auto request_data = std::make_unique<RequestDataWrapper>(
        kTrafficAnnotationTag, kUrl, /*max_retries_param=*/1,
        result_future_.GetCallback());
    request_data->content_data = "request-body";
    return request_data;
  }

  base::test::TestFuture<RequestDataPtr>* auth_failure_future() {
    return &auth_failure_future_;
  }

  base::test::TestFuture<TachyonResponse>* result_future() {
    return &result_future_;
  }

 private:
  base::test::TestFuture<RequestDataPtr> auth_failure_future_;
  base::test::TestFuture<TachyonResponse> result_future_;
  base::test::TaskEnvironment task_env_;
};

TEST_F(TachyonClientImplTest, SuccessfulRequest) {
  network::TestURLLoaderFactory url_loader_factory;
  TestingMessage response;
  response.set_int_field(9999);
  url_loader_factory.AddResponse(kUrl, response.SerializeAsString());

  TachyonClientImpl client(url_loader_factory.GetSafeWeakWrapper());
  client.StartRequest(request_data(), kOAuthToken,
                      auth_failure_future()->GetCallback());

  auto result = result_future()->Take();
  EXPECT_TRUE(result.ok());
  TestingMessage result_proto;
  ASSERT_TRUE(result_proto.ParseFromString(result.response_body()));
  EXPECT_EQ(result_proto.int_field(), 9999);
  EXPECT_FALSE(auth_failure_future()->IsReady());
}

TEST_F(TachyonClientImplTest, NetworkFailure) {
  network::TestURLLoaderFactory url_loader_factory;
  url_loader_factory.AddResponse(
      GURL(kUrl), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::Error::ERR_NETWORK_CHANGED));

  TachyonClientImpl client(url_loader_factory.GetSafeWeakWrapper());
  client.StartRequest(request_data(), kOAuthToken,
                      auth_failure_future()->GetCallback());

  auto result = result_future()->Take();
  EXPECT_EQ(result.status(), TachyonResponse::Status::kNetworkError);
  EXPECT_FALSE(auth_failure_future()->IsReady());
}

TEST_F(TachyonClientImplTest, HttpError) {
  network::TestURLLoaderFactory url_loader_factory;
  url_loader_factory.AddResponse(kUrl, "error",
                                 net::HttpStatusCode::HTTP_PRECONDITION_FAILED);

  TachyonClientImpl client(url_loader_factory.GetSafeWeakWrapper());
  client.StartRequest(request_data(), kOAuthToken,
                      auth_failure_future()->GetCallback());

  auto result = result_future()->Take();
  EXPECT_EQ(result.status(), TachyonResponse::Status::kHttpError);
  EXPECT_FALSE(auth_failure_future()->IsReady());
}

TEST_F(TachyonClientImplTest, AuthError) {
  network::TestURLLoaderFactory url_loader_factory;
  url_loader_factory.AddResponse(kUrl, "error",
                                 net::HttpStatusCode::HTTP_UNAUTHORIZED);

  TachyonClientImpl client(url_loader_factory.GetSafeWeakWrapper());
  RequestDataPtr data = request_data();
  auto* request_data_ptr = data.get();
  client.StartRequest(std::move(data), kOAuthToken,
                      auth_failure_future()->GetCallback());

  RequestDataPtr auth_request_data = auth_failure_future()->Take();
  EXPECT_EQ(auth_request_data->annotation_tag,
            request_data_ptr->annotation_tag);
  EXPECT_EQ(auth_request_data->url, request_data_ptr->url);
  EXPECT_EQ(auth_request_data->max_retries, request_data_ptr->max_retries);
  EXPECT_FALSE(result_future()->IsReady());
}

}  // namespace
}  // namespace ash::babelorca
