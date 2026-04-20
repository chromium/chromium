// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_REQUEST_UTILS_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_REQUEST_UTILS_H_

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "net/http/http_request_headers.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
}

namespace url_pattern {
class SimpleUrlPatternMatcher;
}

namespace guest_view {

// These types are equivalent to the resource types in the extensions API
// defined in extensions/browser/api/web_request/web_request_resource_type.cc
enum class RequestResourceType : uint8_t {
  kMainFrame,
  kSubFrame,
  kStylesheet,
  kScript,
  kImage,
  kFont,
  kObject,
  kXhr,
  kPing,
  kCspReport,
  kMedia,
  kWebSocket,
  kWebTransport,
  kWebBundle,
  kOther,
};

struct BeforeSendHeadersParams {
  BeforeSendHeadersParams();
  ~BeforeSendHeadersParams();
  BeforeSendHeadersParams(const BeforeSendHeadersParams&) = delete;
  BeforeSendHeadersParams(BeforeSendHeadersParams&& other);
  BeforeSendHeadersParams& operator=(const BeforeSendHeadersParams&) = delete;
  BeforeSendHeadersParams& operator=(BeforeSendHeadersParams&& other);

  absl::flat_hash_set<RequestResourceType> resource_types;
  bool include_sub_frame_requests = true;
  net::HttpRequestHeaders add_headers;

  bool IsEmpty() const {
    return resource_types.empty() || add_headers.IsEmpty();
  }
};

struct OriginCheckParams {
  OriginCheckParams();
  ~OriginCheckParams();
  OriginCheckParams(const OriginCheckParams&) = delete;
  OriginCheckParams(OriginCheckParams&& other);
  OriginCheckParams& operator=(const OriginCheckParams&) = delete;
  OriginCheckParams& operator=(OriginCheckParams&& other);

  absl::flat_hash_set<RequestResourceType> resource_types;
  std::vector<std::unique_ptr<url_pattern::SimpleUrlPatternMatcher>>
      allowed_origin_patterns;

  bool IsEmpty() const {
    return resource_types.empty() || allowed_origin_patterns.empty();
  }
};

std::optional<RequestResourceType> ParseRequestResourceType(
    std::string_view text);

RequestResourceType RequestResourceTypeFromResourceRequest(
    const network::ResourceRequest& request);

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_REQUEST_UTILS_H_
