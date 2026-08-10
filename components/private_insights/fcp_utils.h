// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_FCP_UTILS_H_
#define COMPONENTS_PRIVATE_INSIGHTS_FCP_UTILS_H_

#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>

#include "base/synchronization/waitable_event.h"
#include "net/http/http_request_headers.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/federated_compute/src/fcp/client/http/http_client.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace private_insights {

struct ProcessedRequestHeaders {
  net::HttpRequestHeaders headers;
  bool has_explicit_accept_encoding = false;
  std::string upload_content_type = "application/octet-stream";
};

// Converts an FCP HTTP request method enum to the standard HTTP method string.
std::string_view GetMethodString(fcp::client::http::HttpRequest::Method method);

// Converts a Chromium net error code to an Abseil status as expected by FCP.
absl::Status ConvertNetErrorToFcpStatus(int net_error);

// Processes FCP request extra headers by filtering out unsafe headers and
// extracting metadata such as explicit Accept-Encoding and Content-Type.
ProcessedRequestHeaders ProcessFcpRequestHeaders(
    const fcp::client::http::HeaderList& headers);

// Converts Chromium response headers to FCP header list, applying FCP-specific
// header stripping rules.
fcp::client::http::HeaderList ConvertResponseHeadersToFcp(
    const net::HttpResponseHeaders* headers,
    bool request_had_explicit_accept_encoding);

// Reads the request body from an FCP HTTP request into a string. Returns an
// error if reading the request body fails, propagating the status returned by
// `HttpRequest::ReadBody`.
absl::StatusOr<std::string> ReadRequestBody(  // nocheck
    fcp::client::http::HttpRequest& request);

// Thread-safe synchronization primitive that allows threads to wait until a
// set of operations decrement the counter to zero.
//
// WHY THIS IS NEEDED:
// The external FCP library interface (`fcp::client::http::HttpClient`) imposes
// a synchronous `PerformRequests` contract, requiring the caller thread to
// block until a batch of HTTP requests completes. Conversely, Chromium's
// network stack (`SharedURLLoaderFactory`) executes requests asynchronously on
// Chrome's UI thread. `CountdownLatch` bridges this synchronous-to-asynchronous
// boundary by allowing the calling thread to block on `Wait()` until all posted
// network request tasks complete on the UI thread and call `CountDown()`.
class CountdownLatch {
 public:
  // Initializes the latch with `count`. If `count` is 0, the latch is
  // initialized in a signaled state so `Wait()` will return immediately.
  explicit CountdownLatch(size_t count);

  CountdownLatch(const CountdownLatch&) = delete;
  CountdownLatch& operator=(const CountdownLatch&) = delete;
  CountdownLatch(CountdownLatch&&) = delete;
  CountdownLatch& operator=(CountdownLatch&&) = delete;

  ~CountdownLatch();

  // Decrements the counter by 1. When the counter reaches 0, signals the
  // underlying event to unblock all threads waiting in `Wait()`.
  void CountDown();

  // Blocks the calling thread until `CountDown()` has been called `count` times
  // and the counter reaches 0.
  void Wait();

 private:
  std::atomic<size_t> count_;
  base::WaitableEvent done_event_;
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_FCP_UTILS_H_
