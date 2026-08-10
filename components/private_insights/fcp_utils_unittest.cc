// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_utils.h"

#include <atomic>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/federated_compute/src/fcp/client/http/in_memory_request_response.h"

namespace private_insights {

TEST(GetMethodString, MethodMapping) {
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kGet),
            "GET");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kPost),
            "POST");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kPut),
            "PUT");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kDelete),
            "DELETE");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kHead),
            "HEAD");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kPatch),
            "PATCH");
}

TEST(ConvertNetErrorToFcpStatus, ErrorMapping) {
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::OK).code(), absl::StatusCode::kOk);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_ABORTED).code(),
            absl::StatusCode::kCancelled);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_TIMED_OUT).code(),
            absl::StatusCode::kDeadlineExceeded);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_TIMED_OUT).code(),
            absl::StatusCode::kDeadlineExceeded);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_INTERNET_DISCONNECTED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_NETWORK_CHANGED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_NAME_NOT_RESOLVED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_NAME_RESOLUTION_FAILED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_REFUSED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_RESET).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_CLOSED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_ABORTED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_ADDRESS_UNREACHABLE).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_TOO_MANY_REDIRECTS).code(),
            absl::StatusCode::kOutOfRange);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_FAILED).code(),
            absl::StatusCode::kInternal);
}

TEST(ProcessRequestHeadersTest, DefaultValues) {
  fcp::client::http::HeaderList headers;
  ProcessedRequestHeaders result = ProcessFcpRequestHeaders(headers);
  EXPECT_FALSE(result.has_explicit_accept_encoding);
  EXPECT_EQ(result.upload_content_type, "application/octet-stream");
  EXPECT_TRUE(result.headers.IsEmpty());
}

TEST(ProcessRequestHeadersTest, HeaderFilteringAndExtraction) {
  fcp::client::http::HeaderList headers = {
      {"Host", "example.com"},
      {"Content-Length", "123"},
      {"Transfer-Encoding", "chunked"},
      {"Accept-Encoding", "gzip, deflate"},
      {"Content-Type", "application/x-protobuf"},
      {"X-Custom-Header", "custom_value"},
  };

  ProcessedRequestHeaders result = ProcessFcpRequestHeaders(headers);
  EXPECT_TRUE(result.has_explicit_accept_encoding);
  EXPECT_EQ(result.upload_content_type, "application/x-protobuf");

  // Unsafe headers (Host, Content-Length, Transfer-Encoding) must be stripped.
  EXPECT_FALSE(result.headers.HasHeader("Host"));
  EXPECT_FALSE(result.headers.HasHeader("Content-Length"));
  EXPECT_FALSE(result.headers.HasHeader("Transfer-Encoding"));

  // Allowed headers must be present in net::HttpRequestHeaders.
  EXPECT_EQ(result.headers.GetHeader("Accept-Encoding"), "gzip, deflate");
  EXPECT_EQ(result.headers.GetHeader("Content-Type"), "application/x-protobuf");
  EXPECT_EQ(result.headers.GetHeader("X-Custom-Header"), "custom_value");
}

TEST(ProcessRequestHeadersTest, MultipleHeaderValues) {
  fcp::client::http::HeaderList headers = {
      {"X-Multi-Header", "value1"},
      {"X-Multi-Header-2", "valueA, valueB"},
      {"X-Multi-Header", "value2"},
  };

  ProcessedRequestHeaders result = ProcessFcpRequestHeaders(headers);
  EXPECT_EQ(result.headers.GetHeader("X-Multi-Header"), "value1, value2");
  EXPECT_EQ(result.headers.GetHeader("X-Multi-Header-2"), "valueA, valueB");
}

TEST(ConvertResponseHeadersToFcp, WithoutExplicitAcceptEncoding) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Content-Length: 100\r\n"
      "Content-Encoding: gzip\r\n"
      "X-Custom-Header: foo\r\n\r\n");
  ASSERT_TRUE(headers);

  // Without explicit accept encoding, Transfer-Encoding, Content-Length, and
  // Content-Encoding are stripped.
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(), /*request_had_explicit_accept_encoding=*/false);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].first, "Content-Type");
  EXPECT_EQ(result[0].second, "application/json");
  EXPECT_EQ(result[1].first, "X-Custom-Header");
  EXPECT_EQ(result[1].second, "foo");
}

TEST(ConvertResponseHeadersToFcp, WithExplicitAcceptEncoding) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Content-Length: 100\r\n"
      "Content-Encoding: gzip\r\n"
      "X-Custom-Header: foo\r\n\r\n");
  ASSERT_TRUE(headers);

  // With explicit accept encoding, Content-Length and Content-Encoding are
  // kept, Transfer-Encoding is stripped.
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(),
      /*request_had_explicit_accept_encoding=*/true);
  ASSERT_EQ(result.size(), 4u);
  EXPECT_EQ(result[0].first, "Content-Type");
  EXPECT_EQ(result[0].second, "application/json");
  EXPECT_EQ(result[1].first, "Content-Length");
  EXPECT_EQ(result[1].second, "100");
  EXPECT_EQ(result[2].first, "Content-Encoding");
  EXPECT_EQ(result[2].second, "gzip");
  EXPECT_EQ(result[3].first, "X-Custom-Header");
  EXPECT_EQ(result[3].second, "foo");
}

TEST(ConvertResponseHeadersToFcp, CaseInsensitiveHeaderMatching) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "content-type: application/json\r\n"
      "transfer-encoding: chunked\r\n"
      "CONTENT-LENGTH: 100\r\n"
      "Content-encoding: gzip\r\n"
      "x-custom-header: foo\r\n\r\n");
  ASSERT_TRUE(headers);

  // Header matching is case-insensitive, so transfer-encoding, CONTENT-LENGTH,
  // and Content-encoding are correctly stripped even with varied casing.
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(), /*request_had_explicit_accept_encoding=*/false);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].first, "content-type");
  EXPECT_EQ(result[0].second, "application/json");
  EXPECT_EQ(result[1].first, "x-custom-header");
  EXPECT_EQ(result[1].second, "foo");
}

TEST(ConvertResponseHeadersToFcp, MultipleHeaderValues) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-Multi-Header: value1\r\n"
      "X-Multi-Header-2: valueA, valueB\r\n"
      "X-Multi-Header: value2\r\n\r\n");
  ASSERT_TRUE(headers);

  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(), /*request_had_explicit_accept_encoding=*/false);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].first, "X-Multi-Header");
  EXPECT_EQ(result[0].second, "value1");
  EXPECT_EQ(result[1].first, "X-Multi-Header-2");
  EXPECT_EQ(result[1].second, "valueA, valueB");
  EXPECT_EQ(result[2].first, "X-Multi-Header");
  EXPECT_EQ(result[2].second, "value2");
}

TEST(ConvertResponseHeadersToFcp, NullHeaders) {
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      nullptr, /*request_had_explicit_accept_encoding=*/false);
  EXPECT_TRUE(result.empty());
}

TEST(ReadRequestBodyTest, EmptyBody) {
  auto request_or = fcp::client::http::InMemoryHttpRequest::Create(
      "https://example.com", fcp::client::http::HttpRequest::Method::kGet,
      /*extra_headers=*/{}, /*body=*/"", /*use_compression=*/false);
  ASSERT_TRUE(request_or.ok());
  auto body_or = ReadRequestBody(**request_or);
  ASSERT_TRUE(body_or.ok());
  EXPECT_TRUE(body_or->empty());
}

TEST(ReadRequestBodyTest, NonEmptyBody) {
  std::string expected_body = "test payload data";
  auto request_or = fcp::client::http::InMemoryHttpRequest::Create(
      "https://example.com", fcp::client::http::HttpRequest::Method::kPost,
      /*extra_headers=*/{}, expected_body, /*use_compression=*/false);
  ASSERT_TRUE(request_or.ok());
  auto body_or = ReadRequestBody(**request_or);
  ASSERT_TRUE(body_or.ok());
  EXPECT_EQ(*body_or, expected_body);
}

TEST(ReadRequestBodyTest, MultipleChunks) {
  class MultipleChunksHttpRequest : public fcp::client::http::HttpRequest {
   public:
    absl::string_view uri() const override { return "https://example.com"; }

    Method method() const override { return Method::kPost; }

    const fcp::client::http::HeaderList& extra_headers() const override {
      return headers_;
    }

    bool HasBody() const override { return true; }

    absl::StatusOr<int64_t> ReadBody(  // nocheck
        char* buffer,
        int64_t requested) override {
      if (chunk_index_ >= chunks_.size()) {
        return absl::OutOfRangeError("End of sequence");
      }
      const std::string& chunk = chunks_[chunk_index_++];
      EXPECT_GT(requested, static_cast<int64_t>(chunk.size()));
      std::copy(chunk.begin(), chunk.end(), buffer);
      return static_cast<int64_t>(chunk.size());
    }

    std::unique_ptr<HttpRequest> Clone() const override {
      ADD_FAILURE() << "Clone() should not be called";
      return nullptr;
    }

   private:
    std::vector<std::string> chunks_ = {"hello ", "world! ", "multi-chunk ",
                                        "test"};
    size_t chunk_index_ = 0;
    fcp::client::http::HeaderList headers_;
  };

  MultipleChunksHttpRequest request;
  auto body_or = ReadRequestBody(request);
  ASSERT_TRUE(body_or.ok());
  EXPECT_EQ(*body_or, "hello world! multi-chunk test");
}

TEST(ReadRequestBodyTest, ZeroBytesReturned) {
  class ZeroByteHttpRequest : public fcp::client::http::HttpRequest {
   public:
    absl::string_view uri() const override { return "https://example.com"; }

    Method method() const override { return Method::kPost; }

    const fcp::client::http::HeaderList& extra_headers() const override {
      return headers_;
    }

    bool HasBody() const override { return true; }

    absl::StatusOr<int64_t> ReadBody(  // nocheck
        char* buffer,
        int64_t requested) override {
      return 0;
    }

    std::unique_ptr<HttpRequest> Clone() const override {
      ADD_FAILURE() << "Clone() should not be called";
      return nullptr;
    }

   private:
    fcp::client::http::HeaderList headers_;
  };

  ZeroByteHttpRequest request;
  EXPECT_DCHECK_DEATH((void)ReadRequestBody(request));
}

TEST(ReadRequestBodyTest, ErrorReadingBody) {
  class ErrorHttpRequest : public fcp::client::http::HttpRequest {
   public:
    absl::string_view uri() const override { return "https://example.com"; }

    Method method() const override { return Method::kPost; }

    const fcp::client::http::HeaderList& extra_headers() const override {
      return headers_;
    }

    bool HasBody() const override { return true; }

    absl::StatusOr<int64_t> ReadBody(  // nocheck
        char* buffer,
        int64_t requested) override {
      return absl::ResourceExhaustedError("Custom quota exceeded error");
    }

    std::unique_ptr<HttpRequest> Clone() const override {
      ADD_FAILURE() << "Clone() should not be called";
      return nullptr;
    }

   private:
    fcp::client::http::HeaderList headers_;
  };

  ErrorHttpRequest request;
  auto body_or = ReadRequestBody(request);
  EXPECT_FALSE(body_or.ok());
  EXPECT_EQ(body_or.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(body_or.status().message(), "Custom quota exceeded error");
}

TEST(CountdownLatchTest, ZeroInitialCount) {
  CountdownLatch latch(0);
  // Wait() should return immediately for count 0.
  latch.Wait();
}

TEST(CountdownLatchTest, SingleThreadCountDown) {
  CountdownLatch latch(1);
  latch.CountDown();
  latch.Wait();
}

TEST(CountdownLatchTest, MultiThreadedCountDown) {
  base::test::TaskEnvironment task_environment;

  constexpr size_t kNumTasks = 5;
  CountdownLatch latch(kNumTasks);
  std::atomic<bool> wait_completed{false};
  base::RunLoop run_loop;

  // Post a background task that calls latch.Wait().
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](CountdownLatch* l, std::atomic<bool>* completed) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;
            l->Wait();
            completed->store(true);
          },
          base::Unretained(&latch), base::Unretained(&wait_completed)),
      run_loop.QuitClosure());

  // Count down 4 times.
  for (size_t i = 0; i < kNumTasks - 1; ++i) {
    latch.CountDown();
  }

  // Verify that after 4 countdowns, the waiting task is NOT completed yet.
  EXPECT_FALSE(wait_completed.load());

  // Perform the 5th and final countdown.
  latch.CountDown();

  // Wait for the background task to complete and reply.
  run_loop.Run();

  // Verify that after the 5th countdown, the waiting task completed.
  EXPECT_TRUE(wait_completed.load());
}

}  // namespace private_insights
