// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/federated_compute/src/fcp/client/http/in_memory_request_response.h"

namespace private_insights {

class FcpHttpClientTest : public testing::Test {
 public:
  FcpHttpClientTest()
      : request_manager_(test_url_loader_factory_.GetSafeWeakWrapper(),
                         task_environment_.GetMainThreadTaskRunner()) {}
  ~FcpHttpClientTest() override = default;

 protected:
  void PostPerformRequestsTask(
      FcpHttpClient* client,
      std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                            fcp::client::http::HttpRequestCallback*>> requests,
      absl::Status* status,
      std::atomic<bool>* done) {
    auto main_thread = task_environment_.GetMainThreadTaskRunner();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
        base::BindOnce(
            [](FcpHttpClient* client,
               std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                                     fcp::client::http::HttpRequestCallback*>>
                   requests,
               absl::Status* status, std::atomic<bool>* done,
               scoped_refptr<base::SequencedTaskRunner> main_thread) {
              *status = client->PerformRequests(std::move(requests));
              *done = true;
              // Wake up the main thread's RunLoop to evaluate the RunUntil
              // condition.
              main_thread->PostTask(FROM_HERE, base::DoNothing());
            },
            client, std::move(requests), status, done, main_thread));
  }

  absl::Status PerformRequestsAndWait(
      FcpHttpClient* client,
      std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                            fcp::client::http::HttpRequestCallback*>>
          requests) {
    absl::Status status;
    std::atomic<bool> done{false};
    PostPerformRequestsTask(client, std::move(requests), &status, &done);
    EXPECT_TRUE(base::test::RunUntil([&]() { return done.load(); }));
    return status;
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FcpHttpRequestManager request_manager_;
};

TEST_F(FcpHttpClientTest, PerformRequestsEmpty) {
  FcpHttpClient client(&request_manager_);
  EXPECT_TRUE(client.PerformRequests({}).ok());
}

TEST_F(FcpHttpClientTest, PerformRequestsCanceledRequest) {
  FcpHttpClient client(&request_manager_);
  auto request =
      fcp::client::http::InMemoryHttpRequest::Create(
          "https://example.com", fcp::client::http::HttpRequest::Method::kGet,
          /*extra_headers=*/{}, /*body=*/"",
          /*use_compression=*/false)
          .value();
  auto handle = client.EnqueueRequest(std::move(request));
  handle->Cancel();
  fcp::client::http::InMemoryHttpRequestCallback callback;

  std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                        fcp::client::http::HttpRequestCallback*>>
      requests = {{handle.get(), &callback}};

  absl::Status status = client.PerformRequests(requests);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(),
            "HttpRequestHandle was already performed or canceled");
}

TEST_F(FcpHttpClientTest, TotalSentReceivedBytesInitial) {
  FcpHttpClient client(&request_manager_);
  auto request =
      fcp::client::http::InMemoryHttpRequest::Create(
          "https://example.com", fcp::client::http::HttpRequest::Method::kGet,
          /*extra_headers=*/{}, /*body=*/"",
          /*use_compression=*/false)
          .value();
  auto handle = client.EnqueueRequest(std::move(request));
  auto bytes = handle->TotalSentReceivedBytes();
  EXPECT_EQ(bytes.sent_bytes, 0);
  EXPECT_EQ(bytes.received_bytes, 0);
}

TEST_F(FcpHttpClientTest, TotalSentReceivedBytesUpdate) {
  FcpHttpClient client(&request_manager_);
  auto request =
      fcp::client::http::InMemoryHttpRequest::Create(
          "https://example.com", fcp::client::http::HttpRequest::Method::kPost,
          /*extra_headers=*/{}, /*body=*/"test data",
          /*use_compression=*/false)
          .value();
  auto handle = client.EnqueueRequest(std::move(request));
  auto* fcp_handle = static_cast<FcpHttpRequestHandle*>(handle.get());

  fcp_handle->sent_bytes()->store(128);
  fcp_handle->received_bytes()->store(1024);

  auto bytes = handle->TotalSentReceivedBytes();
  EXPECT_EQ(bytes.sent_bytes, 128);
  EXPECT_EQ(bytes.received_bytes, 1024);
}

class TestHttpRequestCallback : public fcp::client::http::HttpRequestCallback {
 public:
  absl::Status OnResponseStarted(
      const fcp::client::http::HttpRequest& request,
      const fcp::client::http::HttpResponse& response) override {
    response_started_called_ = true;
    status_code_ = response.code();
    response_headers_ = response.headers();
    return on_response_started_status_;
  }

  void OnResponseError(const fcp::client::http::HttpRequest& request,
                       const absl::Status& error) override {
    response_error_called_ = true;
    error_status_ = error;
  }

  absl::Status OnResponseBody(const fcp::client::http::HttpRequest& request,
                              const fcp::client::http::HttpResponse& response,
                              absl::string_view data) override {
    response_body_called_ = true;
    body_received_ += std::string(data);
    return on_response_body_status_;
  }

  void OnResponseBodyError(const fcp::client::http::HttpRequest& request,
                           const fcp::client::http::HttpResponse& response,
                           const absl::Status& error) override {
    response_body_error_called_ = true;
    error_status_ = error;
  }

  void OnResponseCompleted(
      const fcp::client::http::HttpRequest& request,
      const fcp::client::http::HttpResponse& response) override {
    response_completed_called_ = true;
  }

  bool response_started_called_ = false;
  bool response_error_called_ = false;
  bool response_body_called_ = false;
  bool response_body_error_called_ = false;
  bool response_completed_called_ = false;
  int status_code_ = 0;
  std::string body_received_;
  absl::Status error_status_;

  fcp::client::http::HeaderList response_headers_;
  absl::Status on_response_started_status_ = absl::OkStatus();
  absl::Status on_response_body_status_ = absl::OkStatus();
};

class ErrorHttpRequest : public fcp::client::http::HttpRequest {
 public:
  absl::string_view uri() const override { return "https://example.com/error"; }

  Method method() const override { return Method::kPost; }

  const fcp::client::http::HeaderList& extra_headers() const override {
    return headers_;
  }

  bool HasBody() const override { return true; }

  absl::StatusOr<int64_t> ReadBody(  // nocheck
      char* buffer,
      int64_t requested) override {
    return absl::InternalError("ReadBody failed");
  }

  std::unique_ptr<HttpRequest> Clone() const override {
    return std::make_unique<ErrorHttpRequest>();
  }

 private:
  fcp::client::http::HeaderList headers_;
};

TEST_F(FcpHttpClientTest, PerformRequestsSuccessfulSingle) {
  FcpHttpClient client(&request_manager_);
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/test",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  auto handle = client.EnqueueRequest(std::move(request));
  TestHttpRequestCallback callback;

  test_url_loader_factory_.AddResponse("https://example.com/test",
                                       "response payload");

  base::HistogramTester histogram_tester;

  absl::Status status =
      PerformRequestsAndWait(&client, {{handle.get(), &callback}});

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_EQ(callback.status_code_, 200);
  EXPECT_TRUE(callback.response_body_called_);
  EXPECT_EQ(callback.body_received_, "response payload");
  EXPECT_TRUE(callback.response_completed_called_);
  histogram_tester.ExpectUniqueSample(kFcpHttpClientHttpResponseCodeHistogram,
                                      200, 1);
  histogram_tester.ExpectUniqueSample(kFcpHttpClientNetErrorHistogram, -net::OK,
                                      1);
  histogram_tester.ExpectTotalCount(kFcpHttpClientRequestDurationHistogram, 1);
  histogram_tester.ExpectUniqueSample(kFcpHttpClientBatchSizeHistogram, 1, 1);
  histogram_tester.ExpectTotalCount(kFcpHttpClientBytesReceivedHistogram, 1);
  histogram_tester.ExpectTotalCount(kFcpHttpClientBytesSentHistogram, 1);
}

TEST_F(FcpHttpClientTest, PerformRequestsSuccessfulBatch) {
  FcpHttpClient client(&request_manager_);
  auto request1 = fcp::client::http::InMemoryHttpRequest::Create(
                      "https://example.com/req1",
                      fcp::client::http::HttpRequest::Method::kGet,
                      /*extra_headers=*/{}, /*body=*/"",
                      /*use_compression=*/false)
                      .value();
  auto request2 = fcp::client::http::InMemoryHttpRequest::Create(
                      "https://example.com/req2",
                      fcp::client::http::HttpRequest::Method::kPost,
                      /*extra_headers=*/{}, /*body=*/"post data",
                      /*use_compression=*/false)
                      .value();

  auto handle1 = client.EnqueueRequest(std::move(request1));
  auto handle2 = client.EnqueueRequest(std::move(request2));

  TestHttpRequestCallback callback1;
  TestHttpRequestCallback callback2;

  test_url_loader_factory_.AddResponse("https://example.com/req1", "resp1");
  test_url_loader_factory_.AddResponse("https://example.com/req2", "resp2");

  base::HistogramTester histogram_tester;

  absl::Status status = PerformRequestsAndWait(
      &client, {{handle1.get(), &callback1}, {handle2.get(), &callback2}});

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(callback1.response_completed_called_);
  EXPECT_EQ(callback1.body_received_, "resp1");
  EXPECT_TRUE(callback2.response_completed_called_);
  EXPECT_EQ(callback2.body_received_, "resp2");
  histogram_tester.ExpectUniqueSample(kFcpHttpClientBatchSizeHistogram, 2, 1);
}

TEST_F(FcpHttpClientTest, PerformRequestsNetError) {
  FcpHttpClient client(&request_manager_);
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/failed",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  auto handle = client.EnqueueRequest(std::move(request));
  TestHttpRequestCallback callback;

  network::URLLoaderCompletionStatus status(net::ERR_FAILED);
  test_url_loader_factory_.AddResponse(GURL("https://example.com/failed"),
                                       network::mojom::URLResponseHead::New(),
                                       "", status);

  base::HistogramTester histogram_tester;

  absl::Status req_status =
      PerformRequestsAndWait(&client, {{handle.get(), &callback}});

  EXPECT_TRUE(req_status.ok());
  EXPECT_TRUE(callback.response_error_called_);
  histogram_tester.ExpectUniqueSample(kFcpHttpClientNetErrorHistogram,
                                      -net::ERR_FAILED, 1);
}

TEST_F(FcpHttpClientTest, PerformRequestsReadBodyError) {
  FcpHttpClient client(&request_manager_);
  auto handle = client.EnqueueRequest(std::make_unique<ErrorHttpRequest>());
  TestHttpRequestCallback callback;

  absl::Status status =
      PerformRequestsAndWait(&client, {{handle.get(), &callback}});

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(callback.response_error_called_);
  EXPECT_EQ(callback.error_status_.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(callback.error_status_.message(), "ReadBody failed");
}

TEST_F(FcpHttpClientTest, PerformRequestsCancelViaHandle) {
  FcpHttpClient client(&request_manager_);
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/pending",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  auto handle = client.EnqueueRequest(std::move(request));
  TestHttpRequestCallback callback;

  absl::Status status;
  std::atomic<bool> done{false};
  PostPerformRequestsTask(&client, {{handle.get(), &callback}}, &status, &done);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_url_loader_factory_.NumPending() == 1; }));

  handle->Cancel();

  EXPECT_TRUE(base::test::RunUntil([&]() { return done.load(); }));

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(callback.response_error_called_);
  EXPECT_EQ(callback.error_status_.code(), absl::StatusCode::kCancelled);
}

class FcpHttpRequestRunnerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(FcpHttpRequestRunnerTest, SuccessfulGetRequest) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/test",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  bool complete_called = false;
  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);
  runner->set_on_complete_callback(
      base::BindOnce([](bool* called) { *called = true; }, &complete_called));

  test_url_loader_factory_.AddResponse("https://example.com/test",
                                       "Hello World");
  EXPECT_TRUE(base::test::RunUntil([&]() { return complete_called; }));

  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_EQ(callback.status_code_, 200);
  EXPECT_TRUE(callback.response_body_called_);
  EXPECT_EQ(callback.body_received_, "Hello World");
  EXPECT_TRUE(callback.response_completed_called_);
  EXPECT_TRUE(complete_called);
  EXPECT_EQ(handle.TotalSentReceivedBytes().sent_bytes, 0);
  EXPECT_EQ(handle.TotalSentReceivedBytes().received_bytes, 11);
}

TEST_F(FcpHttpRequestRunnerTest, SuccessfulPostRequestWithUploadBody) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/post",
                     fcp::client::http::HttpRequest::Method::kPost,
                     /*extra_headers=*/{{"Content-Type", "text/plain"}},
                     /*body=*/"upload payload",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  bool complete_called = false;
  auto runner = std::make_unique<FcpHttpRequestRunner>(
      &test_url_loader_factory_, &handle,
      /*upload_body=*/"upload payload", &callback);
  runner->set_on_complete_callback(
      base::BindOnce([](bool* called) { *called = true; }, &complete_called));

  test_url_loader_factory_.AddResponse("https://example.com/post", "ok");
  EXPECT_TRUE(base::test::RunUntil([&]() { return complete_called; }));

  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_TRUE(callback.response_body_called_);
  EXPECT_EQ(callback.body_received_, "ok");
  EXPECT_TRUE(callback.response_completed_called_);
  EXPECT_TRUE(complete_called);
  EXPECT_EQ(handle.TotalSentReceivedBytes().received_bytes, 2);
}

TEST_F(FcpHttpRequestRunnerTest, HttpError404Response) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/404",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);

  test_url_loader_factory_.AddResponse("https://example.com/404", "Not Found",
                                       net::HTTP_NOT_FOUND);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return callback.response_completed_called_; }));

  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_EQ(callback.status_code_, 404);
  EXPECT_TRUE(callback.response_body_called_);
  EXPECT_EQ(callback.body_received_, "Not Found");
  EXPECT_TRUE(callback.response_completed_called_);
}

TEST_F(FcpHttpRequestRunnerTest, NetworkErrorBeforeResponseStarted) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/err",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);

  test_url_loader_factory_.AddResponse(
      GURL("https://example.com/err"), network::mojom::URLResponseHead::New(),
      "", network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return callback.response_error_called_; }));

  EXPECT_FALSE(callback.response_started_called_);
  EXPECT_TRUE(callback.response_error_called_);
  EXPECT_EQ(callback.error_status_.code(), absl::StatusCode::kUnavailable);
}

TEST_F(FcpHttpRequestRunnerTest, NetworkErrorDuringResponseBody) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/stream-err",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n\r\n");
  test_url_loader_factory_.GetPendingRequest(0)->client->OnReceiveResponse(
      std::move(response_head), mojo::ScopedDataPipeConsumerHandle(),
      std::nullopt);
  test_url_loader_factory_.GetPendingRequest(0)->client->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return callback.response_body_error_called_; }));

  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_TRUE(callback.response_body_error_called_);
  EXPECT_EQ(callback.error_status_.code(), absl::StatusCode::kInternal);
}

TEST_F(FcpHttpRequestRunnerTest, OnResponseStartedReturnsError) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/test",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;
  callback.on_response_started_status_ =
      absl::InternalError("error in started");

  bool complete_called = false;
  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);
  runner->set_on_complete_callback(
      base::BindOnce([](bool* called) { *called = true; }, &complete_called));

  test_url_loader_factory_.AddResponse("https://example.com/test", "hello");
  EXPECT_TRUE(base::test::RunUntil([&]() { return complete_called; }));

  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_FALSE(callback.response_body_called_);
  EXPECT_FALSE(callback.response_error_called_);
  EXPECT_FALSE(callback.response_body_error_called_);
  EXPECT_FALSE(callback.response_completed_called_);
  EXPECT_TRUE(complete_called);
}

TEST_F(FcpHttpRequestRunnerTest, OnResponseBodyReturnsError) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/test",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;
  callback.on_response_body_status_ = absl::InternalError("error in body");

  bool complete_called = false;
  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);
  runner->set_on_complete_callback(
      base::BindOnce([](bool* called) { *called = true; }, &complete_called));

  test_url_loader_factory_.AddResponse("https://example.com/test", "hello");
  EXPECT_TRUE(base::test::RunUntil([&]() { return complete_called; }));

  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_TRUE(callback.response_body_called_);
  EXPECT_FALSE(callback.response_error_called_);
  EXPECT_FALSE(callback.response_body_error_called_);
  EXPECT_FALSE(callback.response_completed_called_);
  EXPECT_TRUE(complete_called);
}

TEST_F(FcpHttpRequestRunnerTest, CancelBeforeResponseStarted) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/test",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  bool complete_called = false;
  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);
  runner->set_on_complete_callback(
      base::BindOnce([](bool* called) { *called = true; }, &complete_called));

  runner->Cancel();

  EXPECT_FALSE(callback.response_started_called_);
  EXPECT_TRUE(callback.response_error_called_);
  EXPECT_FALSE(callback.response_body_called_);
  EXPECT_FALSE(callback.response_body_error_called_);
  EXPECT_FALSE(callback.response_completed_called_);
  EXPECT_TRUE(complete_called);
  EXPECT_EQ(callback.error_status_.code(), absl::StatusCode::kCancelled);
}

TEST_F(FcpHttpRequestRunnerTest, CancelAfterResponseStarted) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/pending",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  bool complete_called = false;
  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);
  runner->set_on_complete_callback(
      base::BindOnce([](bool* called) { *called = true; }, &complete_called));

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n\r\n");
  test_url_loader_factory_.GetPendingRequest(0)->client->OnReceiveResponse(
      std::move(response_head), mojo::ScopedDataPipeConsumerHandle(),
      std::nullopt);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return callback.response_started_called_; }));

  // At this point response started has been called. Now cancel.
  runner->Cancel();

  EXPECT_TRUE(callback.response_started_called_);
  EXPECT_FALSE(callback.response_error_called_);
  EXPECT_FALSE(callback.response_body_called_);
  EXPECT_TRUE(callback.response_body_error_called_);
  EXPECT_FALSE(callback.response_completed_called_);
  EXPECT_TRUE(complete_called);
  EXPECT_EQ(callback.error_status_.code(), absl::StatusCode::kCancelled);
}

TEST_F(FcpHttpRequestRunnerTest, ExplicitAcceptEncodingHeader) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/accept",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{{"Accept-Encoding", "gzip"}},
                     /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& resource_request =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_TRUE(resource_request.devtools_accepted_stream_types.has_value());
  EXPECT_TRUE(resource_request.devtools_accepted_stream_types->empty());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\n"
                                        "Content-Encoding: gzip\n"
                                        "Content-Length: 5\n"
                                        "Transfer-Encoding: chunked\n"
                                        "X-Custom-Header: test\n\n"));
  test_url_loader_factory_.AddResponse(
      GURL("https://example.com/accept"), std::move(response_head), "hello",
      network::URLLoaderCompletionStatus(net::OK));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return callback.response_completed_called_; }));

  fcp::client::http::HeaderList expected_headers = {
      {"Content-Encoding", "gzip"},
      {"Content-Length", "5"},
      {"X-Custom-Header", "test"}};
  EXPECT_EQ(callback.response_headers_, expected_headers);
}

TEST_F(FcpHttpRequestRunnerTest, ImplicitAcceptEncodingHeaderStripsHeaders) {
  auto request = fcp::client::http::InMemoryHttpRequest::Create(
                     "https://example.com/no-accept",
                     fcp::client::http::HttpRequest::Method::kGet,
                     /*extra_headers=*/{}, /*body=*/"",
                     /*use_compression=*/false)
                     .value();
  FcpHttpRequestHandle handle(nullptr, /*request_id=*/1, std::move(request));
  TestHttpRequestCallback callback;

  auto runner =
      std::make_unique<FcpHttpRequestRunner>(&test_url_loader_factory_, &handle,
                                             /*upload_body=*/"", &callback);

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\n"
                                        "Content-Encoding: gzip\n"
                                        "Content-Length: 5\n"
                                        "Transfer-Encoding: chunked\n"
                                        "X-Custom-Header: test\n\n"));
  test_url_loader_factory_.AddResponse(
      GURL("https://example.com/no-accept"), std::move(response_head), "hello",
      network::URLLoaderCompletionStatus(net::OK));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return callback.response_completed_called_; }));

  fcp::client::http::HeaderList expected_headers = {
      {"X-Custom-Header", "test"}};
  EXPECT_EQ(callback.response_headers_, expected_headers);
}

}  // namespace private_insights
