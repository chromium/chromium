// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_http_client.h"

#include <initializer_list>
#include <string_view>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-shared.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace mojom {

// This HttpResponsePtr ostream printer pretty prints the response in test
// failures.
std::ostream& operator<<(std::ostream& os, const HttpResponsePtr& response) {
  os << "response_code: " << response->response_code << std::endl;
  os << "headers: " << std::endl;
  for (const auto& header : response->headers) {
    os << "  " << header->key << ": " << header->value << std::endl;
  }
  return os;
}

}  // namespace mojom

namespace {

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Pointee;

using HttpHeaders = std::vector<mojom::HttpHeaderPtr>;
using HttpHeadersInitializerList =
    std::initializer_list<std::pair<std::string_view, std::string_view>>;

class MockHttpDelegate : public mojom::HttpDelegate {
 public:
  MOCK_METHOD(void,
              GetRequestBody,
              (mojo::ScopedDataPipeProducerHandle),
              (override));
  MOCK_METHOD(void, OnReceiveResponse, (mojom::HttpResponsePtr), (override));
  MOCK_METHOD(void,
              OnReceiveBody,
              (mojo::ScopedDataPipeConsumerHandle),
              (override));
  MOCK_METHOD(void,
              OnRequestComplete,
              (mojom::HttpCompletionStatusPtr),
              (override));
};

class DriveFsHttpClientTest : public testing::Test {
 public:
  DriveFsHttpClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        http_client_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void BlockingCopyToString(std::string* result,
                            mojo::ScopedDataPipeConsumerHandle source) {
    EXPECT_TRUE(mojo::BlockingCopyToString(std::move(source), result));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DriveFsHttpClient http_client_;
};

struct ConsumeAllDataAndCompareWith {
  void operator()(mojo::ScopedDataPipeConsumerHandle handle) {
    std::string data;
    EXPECT_TRUE(mojo::BlockingCopyToString(std::move(handle), &data));
    EXPECT_EQ(data, expected_data);
  }

  std::string_view expected_data;
};

struct ProduceAllData {
  void operator()(mojo::ScopedDataPipeProducerHandle handle) {
    EXPECT_TRUE(mojo::BlockingCopyFromString(std::string(data), handle));
  }

  std::string_view data;
};

HttpHeaders HttpHeadersFromInitializerList(
    HttpHeadersInitializerList input_headers) {
  HttpHeaders headers;
  for (const auto& header : input_headers) {
    headers.push_back(mojom::HttpHeader::New(std::string{header.first},
                                             std::string{header.second}));
  }
  return headers;
}

network::mojom::URLResponseHeadPtr CreateURLResponseHead(
    net::HttpStatusCode http_status,
    HttpHeadersInitializerList input_headers) {
  auto head = network::mojom::URLResponseHead::New();
  const std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", static_cast<int>(http_status),
                         net::GetHttpReasonPhrase(http_status)));
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(status_line));
  for (const auto& header : input_headers) {
    head->headers->AddHeader(header.first, header.second);
  }
  return head;
}

TEST_F(DriveFsHttpClientTest, RequestHandlesRequestData) {
  constexpr char kTestURL[] = "https://drive.google.com";
  constexpr std::string_view kTestRequestData = "Test Request Data";
  const HttpHeadersInitializerList kTestRequestHeaders = {
      {"Test-Request-Header", "Test-Value"},
  };
  base::RunLoop run_loop;
  NiceMock<MockHttpDelegate> http_delegate;
  mojom::HttpCompletionStatus expected_status(mojom::NetError::kOk, 0);
  EXPECT_CALL(http_delegate,
              OnRequestComplete(Pointee(Eq(std::ref(expected_status)))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(http_delegate, GetRequestBody)
      .WillOnce(ProduceAllData{kTestRequestData});
  mojo::Receiver<mojom::HttpDelegate> http_receiver(&http_delegate);
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        const std::vector<network::DataElement>* elements =
            request.request_body->elements();
        ASSERT_EQ(elements->size(), 1UL);
        const network::DataElement& element = elements->front();
        mojo::Remote<network::mojom::DataPipeGetter> remote(
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter());
        remote->Read(std::move(producer),
                     base::BindOnce([](int32_t, uint64_t) {}));
      }));
  http_client_.ExecuteHttpRequest(
      mojom::HttpRequest::New(
          /*method=*/"GET",
          /*url=*/kTestURL,
          /*headers=*/HttpHeadersFromInitializerList(kTestRequestHeaders),
          /*request_body_bytes=*/kTestRequestData.size()),
      http_receiver.BindNewPipeAndPassRemote());
  EXPECT_TRUE(
      test_url_loader_factory_.SimulateResponseForPendingRequest(kTestURL, ""));
  run_loop.Run();
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  std::string data;
  EXPECT_TRUE(mojo::BlockingCopyToString(std::move(consumer), &data));
  EXPECT_EQ(data, kTestRequestData);
}

TEST_F(DriveFsHttpClientTest, RequestHandlesResponseData) {
  constexpr char kTestURL[] = "https://drive.google.com";
  constexpr std::string_view kTestResponseData = "Test Response Data";
  const HttpHeadersInitializerList kTestResponseHeaders = {
      {"Test-Response-Header", "Test-Value"},
  };
  base::RunLoop run_loop;
  NiceMock<MockHttpDelegate> http_delegate;
  mojom::HttpCompletionStatus expected_status(mojom::NetError::kOk,
                                              kTestResponseData.size());
  EXPECT_CALL(http_delegate,
              OnRequestComplete(Pointee(Eq(std::ref(expected_status)))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  mojom::HttpResponse expected_response(
      net::HTTP_OK, HttpHeadersFromInitializerList(kTestResponseHeaders));
  EXPECT_CALL(http_delegate,
              OnReceiveResponse(Pointee(Eq(std::ref(expected_response)))));
  EXPECT_CALL(http_delegate, OnReceiveBody)
      .WillOnce(ConsumeAllDataAndCompareWith{kTestResponseData});
  mojo::Receiver<mojom::HttpDelegate> http_receiver(&http_delegate);
  http_client_.ExecuteHttpRequest(mojom::HttpRequest::New(
                                      /*method=*/"GET",
                                      /*url=*/kTestURL,
                                      /*headers=*/HttpHeaders(),
                                      /*request_body_bytes=*/0),
                                  http_receiver.BindNewPipeAndPassRemote());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kTestURL), network::URLLoaderCompletionStatus(net::OK),
      CreateURLResponseHead(net::HTTP_OK, kTestResponseHeaders),
      std::string(kTestResponseData)));
  run_loop.Run();
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(DriveFsHttpClientTest, RequestCancelledOnRedirect) {
  constexpr char kTestURL[] = "https://drive.google.com";
  // Populate the URLLoaderFactory with a redirect.
  net::RedirectInfo redirect;
  redirect.status_code = 301;
  redirect.new_url = GURL("https://not_a_virus.google.com/");
  network::TestURLLoaderFactory::Redirects redirects;
  redirects.push_back({redirect, network::mojom::URLResponseHead::New()});
  test_url_loader_factory_.AddResponse(
      GURL(kTestURL), network::mojom::URLResponseHead::New(),
      /*content=*/"", network::URLLoaderCompletionStatus(),
      std::move(redirects),
      network::TestURLLoaderFactory::kResponseOnlyRedirectsNoDestination);
  // Run until the delegate is disconnected, there should be no callbacks.
  base::RunLoop run_loop;
  NiceMock<MockHttpDelegate> http_delegate;
  EXPECT_CALL(http_delegate, OnReceiveResponse).Times(0);
  EXPECT_CALL(http_delegate, OnRequestComplete).Times(0);
  mojo::Receiver<mojom::HttpDelegate> http_receiver(&http_delegate);
  mojo::PendingRemote<mojom::HttpDelegate> http_remote =
      http_receiver.BindNewPipeAndPassRemote();
  http_receiver.set_disconnect_handler(run_loop.QuitClosure());
  http_client_.ExecuteHttpRequest(mojom::HttpRequest::New(
                                      /*method=*/"GET",
                                      /*url=*/kTestURL,
                                      /*headers=*/HttpHeaders(),
                                      /*request_body_bytes=*/0),
                                  std::move(http_remote));
  run_loop.Run();
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

}  // namespace
}  // namespace drivefs
