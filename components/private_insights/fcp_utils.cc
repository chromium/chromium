// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_utils.h"

#include <utility>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

namespace private_insights {

std::string_view GetMethodString(
    fcp::client::http::HttpRequest::Method method) {
  switch (method) {
    case fcp::client::http::HttpRequest::Method::kHead:
      return net::HttpRequestHeaders::kHeadMethod;
    case fcp::client::http::HttpRequest::Method::kGet:
      return net::HttpRequestHeaders::kGetMethod;
    case fcp::client::http::HttpRequest::Method::kPost:
      return net::HttpRequestHeaders::kPostMethod;
    case fcp::client::http::HttpRequest::Method::kPut:
      return net::HttpRequestHeaders::kPutMethod;
    case fcp::client::http::HttpRequest::Method::kPatch:
      return net::HttpRequestHeaders::kPatchMethod;
    case fcp::client::http::HttpRequest::Method::kDelete:
      return net::HttpRequestHeaders::kDeleteMethod;
  }
  NOTREACHED();
}

absl::Status ConvertNetErrorToFcpStatus(int net_error) {
  switch (net_error) {
    case net::OK:
      return absl::OkStatus();
    case net::ERR_ABORTED:
      // From FCP docs:
      // > If the `HttpRequestHandle::Cancel` method was called [...] then this
      // > method will be called with a `CANCELLED` status.
      //
      // Note: it's about user-driven cancellations, so ERR_CONNECTION_ABORTED
      // doesn't apply here.
      return absl::CancelledError(net::ErrorToString(net_error));
    case net::ERR_TIMED_OUT:
    case net::ERR_CONNECTION_TIMED_OUT:
      // From FCP docs:
      // > If the implementation hit an implementation-specific timeout (even
      // > though implementations are discouraged from imposing such timeouts),
      // > then this should be `DEADLINE_EXCEEDED`.
      return absl::DeadlineExceededError(net::ErrorToString(net_error));
    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_NETWORK_CHANGED:
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_NAME_RESOLUTION_FAILED:
    case net::ERR_CONNECTION_REFUSED:
    case net::ERR_CONNECTION_RESET:
    case net::ERR_CONNECTION_CLOSED:
    case net::ERR_CONNECTION_ABORTED:
    case net::ERR_ADDRESS_UNREACHABLE:
      // From FCP docs:
      // > If the implementation is able to discern that the error may have been
      // > transient, they should return `UNAVAILABLE`.
      return absl::UnavailableError(net::ErrorToString(net_error));
    case net::ERR_TOO_MANY_REDIRECTS:
      // From FCP docs:
      // > If more than the implementation's defined max number of redirects
      // > occurred (without reaching the final response), then implementations
      // > should return `OUT_OF_RANGE` here.
      return absl::OutOfRangeError(net::ErrorToString(net_error));
    default:
      return absl::InternalError(net::ErrorToString(net_error));
  }
}

ProcessedRequestHeaders ProcessFcpRequestHeaders(
    const fcp::client::http::HeaderList& headers) {
  ProcessedRequestHeaders result;
  for (const auto& header : headers) {
    const std::string& name = header.first;
    const std::string& value = header.second;

    if (base::EqualsCaseInsensitiveASCII(
            name, net::HttpRequestHeaders::kContentLength) ||
        base::EqualsCaseInsensitiveASCII(name,
                                         net::HttpRequestHeaders::kHost) ||
        base::EqualsCaseInsensitiveASCII(
            name, net::HttpRequestHeaders::kTransferEncoding)) {
      // We cannot pass those headers to Chrome's net stack, because they are
      // unsafe and will be computed internally.
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(
            name, net::HttpRequestHeaders::kAcceptEncoding)) {
      result.has_explicit_accept_encoding = true;
    }

    if (base::EqualsCaseInsensitiveASCII(
            name, net::HttpRequestHeaders::kContentType)) {
      result.upload_content_type = value;
    }

    // HeaderList can contain multiple values for the same header.
    // Concatenate them to prevent overwriting with the last value.
    std::optional<std::string> existing = result.headers.GetHeader(name);
    if (existing.has_value()) {
      result.headers.SetHeader(name, base::StrCat({*existing, ", ", value}));
    } else {
      result.headers.SetHeader(name, value);
    }
  }
  return result;
}

fcp::client::http::HeaderList ConvertResponseHeadersToFcp(
    const net::HttpResponseHeaders* headers,
    bool request_had_explicit_accept_encoding) {
  fcp::client::http::HeaderList result;
  if (!headers) {
    return result;
  }

  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    if (base::EqualsCaseInsensitiveASCII(name, "Transfer-Encoding")) {
      // FCP expects Transfer-Encoding to be stripped when decoded (see
      // "Response body decompression & decoding" on
      // fcp::client::http::HttpClient).
      continue;
    }

    if (!request_had_explicit_accept_encoding) {
      // We have to strip these headers if no explicit Accept-Encoding is
      // specified, as per "Response body decompression & decoding" on
      // fcp::client::http::HttpClient.
      if (base::EqualsCaseInsensitiveASCII(name, "Content-Encoding") ||
          base::EqualsCaseInsensitiveASCII(name, "Content-Length")) {
        continue;
      }
    }

    result.emplace_back(std::move(name), std::move(value));
  }

  return result;
}

absl::StatusOr<std::string> ReadRequestBody(  // nocheck
    fcp::client::http::HttpRequest& request) {
  std::string body;
  if (!request.HasBody()) {
    return body;
  }

  char buffer[4096];
  while (true) {
    auto read_or = request.ReadBody(buffer, sizeof(buffer));
    if (!read_or.ok()) {
      // As per `ReadBody`'s contract, kOutOfRange means no more data.
      if (read_or.status().code() == absl::StatusCode::kOutOfRange) {
        break;
      }

      // As per `OnResponseError`'s docs, if `ReadBody` returned an unexpected
      // error, then `OnResponseError` should be called with that error, that's
      // why we need to return it.
      return read_or.status();
    }

    // ReadBody should return at least one byte. If that doesn't happen, handle
    // this case gracefully instead of falling into a potentially infinite loop.
    DCHECK_GT(*read_or, 0);
    if (*read_or == 0) {
      break;
    }

    body.append(buffer, *read_or);
  }

  return body;
}

CountdownLatch::CountdownLatch(size_t count)
    : count_(count),
      done_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                  base::WaitableEvent::InitialState::NOT_SIGNALED) {
  if (count_ == 0) {
    done_event_.Signal();
  }
}

CountdownLatch::~CountdownLatch() = default;

void CountdownLatch::CountDown() {
  if (--count_ == 0) {
    done_event_.Signal();
  }
}

void CountdownLatch::Wait() {
  done_event_.Wait();
}

}  // namespace private_insights
