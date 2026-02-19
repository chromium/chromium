// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/platform_auth/url_session_helper.h"

#include <Foundation/Foundation.h>

#include <algorithm>

#include "base/apple/foundation_util.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "components/enterprise/platform_auth/platform_auth_features.h"
#include "components/policy/core/common/policy_logger.h"
#include "net/base/apple/url_conversions.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/http/http_version.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

namespace url_session_helper {

namespace {

constexpr char kWildcardSegment[] = "*";

// Splits both the pattern and the path into segments separated with |/|.
// Compares corresponding segments. Wildcard |*| matches one whole segment.
bool MatchOktaSSOUrlPattern(std::string_view path) {
  static const base::NoDestructor<std::vector<std::string>> pattern_segments(
      base::SplitString(enterprise_auth::kOktaSsoURLPattern.Get(), "/",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  const std::vector<std::string_view> path_segments = base::SplitStringPiece(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (path_segments.size() != pattern_segments->size()) {
    return false;
  }

  for (size_t i = 0; i < path_segments.size(); ++i) {
    if (pattern_segments->at(i) != kWildcardSegment &&
        path_segments.at(i) != pattern_segments->at(i)) {
      return false;
    }
  }

  return true;
}

constexpr NSString* kNsOrigin = @"Origin";

base::flat_set<std::string> ParseCommaSeparatedList(std::string raw_param) {
  std::vector<std::string> parts = base::SplitString(
      raw_param, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return base::flat_set<std::string>(std::move(parts));
}

base::flat_map<std::string, std::string> ParseFixedRequestHeaders() {
  std::string raw_param = enterprise_auth::kOktaSsoFixedRequestHeaders.Get();
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(raw_param, ';', '|', &pairs);
  base::flat_map<std::string, std::string> headers(std::move(pairs));
  return headers;
}

// Will ignore headers where conversion between std::string and NSString failed.
// Never returns nil, if all headers were skipped an empty dictionary will be
// returned.
// Headers are allowlisted, moreover a certain fixed set of headers is added.
NSMutableDictionary* ConvertHttpRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  NSMutableDictionary* headers_dict = [NSMutableDictionary dictionary];

  static const base::NoDestructor<base::flat_set<std::string>>
      kRequestHeadersAllowlist(ParseCommaSeparatedList(
          enterprise_auth::kOktaSsoRequestHeadersAllowlist.Get()));

  for (const auto& header : headers.GetHeaderVector()) {
    if (kRequestHeadersAllowlist->contains(base::ToLowerASCII(header.key))) {
      NSString* ns_key = base::SysUTF8ToNSString(header.key);
      NSString* ns_value = base::SysUTF8ToNSString(header.value);
      if (ns_key && ns_value) {
        [headers_dict setObject:ns_value forKey:ns_key];
      }
    }
  }

  static const base::NoDestructor<base::flat_map<std::string, std::string>>
      kFixedRequestHeaders(ParseFixedRequestHeaders());

  for (const auto& [key, value] : *kFixedRequestHeaders) {
    NSString* ns_key = base::SysUTF8ToNSString(key);
    NSString* ns_value = base::SysUTF8ToNSString(value);
    if (ns_key && ns_value) {
      [headers_dict setObject:ns_value forKey:ns_key];
    }
  }

  return headers_dict;
}

// This function only works with network::DataElementBytes. Returns nil if any
// of the elements are of a different type than bytes.
NSData* ConvertRequestBody(
    const scoped_refptr<network::ResourceRequestBody>& request_body) {
  if (!request_body) {
    return nil;
  }

  const std::vector<network::DataElement>* elements = request_body->elements();
  if (!elements) {
    return nil;
  }

  NSMutableData* body_data = [NSMutableData data];

  for (const auto& data_element : *elements) {
    if (data_element.type() != network::DataElement::Tag::kBytes) {
      return nil;
    }
    base::span<const uint8_t> data_span =
        data_element.As<network::DataElementBytes>().bytes();
    [body_data appendBytes:data_span.data() length:data_span.size()];
  }

  return body_data;
}

// Returns lower-case normalised values specified by the
// Access-Control-Allow-Origin header in the `http_response`.
std::vector<std::string> ParseAccessControlAllowHeaders(
    NSHTTPURLResponse* http_response) {
  NSString* ns_access_control_allow_headers =
      [http_response valueForHTTPHeaderField:@("access-control-allow-headers")];

  std::vector<std::string> allowed_headers;
  if (ns_access_control_allow_headers) {
    const std::string access_control_allow_headers =
        base::SysNSStringToUTF8(ns_access_control_allow_headers);
    net::HttpUtil::ValuesIterator it(access_control_allow_headers, ',');
    while (it.GetNext()) {
      allowed_headers.push_back(base::ToLowerASCII(it.value()));
    }
  } else {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] The Access-Control-Allow-Origin header is "
           "missing from the SSO request's response. No headers will be "
           "propagated to the renderer process.";
  }

  if (allowed_headers.empty()) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] The Access-Control-Allow-Origin header is "
           "empty. No headers will be "
           "propagated to the renderer process.";
  }

  return allowed_headers;
}

// Returns nullptr if:
//  - status code is invalid
//  - Access-Control-Allow-Headers header is missing
//  - Access-Control-Allow-Origin is missing or does not match
//  |request_initiator|.
//
// Headers with invalid names or values are ignored.
scoped_refptr<net::HttpResponseHeaders> ConvertNSHTTPURLResponseToHeaders(
    NSHTTPURLResponse* http_response,
    const url::Origin& request_initiator) {
  // Verify the Access-Control-Allow-Origin header is a wildcard or matches the
  // request_initiator.
  // valueForHTTPHeaderField is NOT case-sensitive.
  NSString* ns_allowed_origin =
      [http_response valueForHTTPHeaderField:@("access-control-allow-origin")];
  if (!ns_allowed_origin) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] SSO URL request response rejected because the "
           "header access-control-allow-origin is missing.";
    return nullptr;
  }
  const std::string allowed_origin = base::SysNSStringToUTF8(ns_allowed_origin);
  if (allowed_origin != "*" &&
      !request_initiator.IsSameOriginWith(GURL(allowed_origin))) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] SSO URL request response rejected because the "
           "header access-control-allow-origin does not match the request "
           "initiator.";
    return nullptr;
  }

  // access-control-allow-headers it specifies which headers are
  // passed to the renderer process.
  const std::vector<std::string> allowed_headers =
      ParseAccessControlAllowHeaders(http_response);

  // Builder.AddHeader() doesn't copy the string, the copy is only made once
  // Build() is called. Because of this we need to make sure the strings are
  // valid until then.
  std::vector<std::pair<std::string, std::string>> headers_to_add;
  headers_to_add.reserve(allowed_headers.size());
  for (const std::string& allowed_header : allowed_headers) {
    NSString* ns_value = [http_response
        valueForHTTPHeaderField:base::SysUTF8ToNSString(allowed_header)];
    if (!ns_value) {
      continue;
    }
    const std::string value = base::SysNSStringToUTF8(ns_value);
    // SysNSStringToUTF8 returns an empty string if argument was nil or
    // invalid.
    if (!value.empty() && net::HttpUtil::IsValidHeaderName(allowed_header) &&
        net::HttpUtil::IsValidHeaderValue(value)) {
      headers_to_add.emplace_back(std::move(allowed_header), std::move(value));
    }
  }

  std::optional<net::HttpStatusCode> status_code =
      net::TryToGetHttpStatusCode(http_response.statusCode);
  if (!status_code.has_value()) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] SSO URL request response rejected because the "
           "status code is missing";
    return nullptr;
  }
  const std::string status_line = base::JoinString(
      {base::ToString(status_code.value()),
       net::GetHttpReasonPhrase(std::move(status_code.value()))},
      " ");
  // There is not public accessor for the HTTP version from NSHTTPURLResponse so
  // we use 1.1 by default, it is enough for our needs.
  net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1),
                                            status_line);
  for (const auto& [key, value] : headers_to_add) {
    builder.AddHeader(key, value);
  }
  return builder.Build();
}

}  // namespace

NSURLRequest* ConvertResourceRequest(const network::ResourceRequest& request,
                                     base::TimeDelta timeout) {
  NSURL* native_url = net::NSURLWithGURL(request.url);
  if (!native_url) {
    return nil;
  }

  NSString* method = base::SysUTF8ToNSString(request.method);
  if (!method) {
    return nil;
  }

  NSData* body = ConvertRequestBody(request.request_body);

  NSMutableDictionary* headers = ConvertHttpRequestHeaders(request.headers);
  // Set the Origin header. This function assumes ResourceRequest contains a
  // valid |request_initiator|.
  CHECK(request.request_initiator.has_value())
      << "request_initiator must be valid.";
  NSString* ns_origin =
      base::SysUTF8ToNSString(request.request_initiator.value().Serialize());
  if (ns_origin) {
    [headers setObject:ns_origin forKey:kNsOrigin];
  }

  NSMutableURLRequest* ns_request =
      [NSMutableURLRequest requestWithURL:native_url];
  ns_request.HTTPMethod = method;
  ns_request.allHTTPHeaderFields = headers;
  ns_request.cachePolicy = NSURLRequestUseProtocolCachePolicy;
  ns_request.timeoutInterval = timeout.InSeconds();
  ns_request.HTTPBody = body;
  return ns_request;
}

network::mojom::URLResponseHeadPtr ConvertNSURLResponse(
    NSURLResponse* ns_response,
    const url::Origin& request_initiator) {
  CHECK(ns_response);
  network::mojom::URLResponseHeadPtr response =
      network::mojom::URLResponseHead::New();

  if (ns_response.MIMEType) {
    response->mime_type = base::SysNSStringToUTF8(ns_response.MIMEType);
  }
  response->content_length = ns_response.expectedContentLength;
  response->network_accessed = true;

  if ([ns_response isKindOfClass:[NSHTTPURLResponse class]]) {
    NSHTTPURLResponse* http_response = (NSHTTPURLResponse*)ns_response;
    scoped_refptr<net::HttpResponseHeaders> headers =
        ConvertNSHTTPURLResponseToHeaders(http_response, request_initiator);
    if (!headers) {
      return nullptr;
    }
    response->headers = std::move(headers);
  } else {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] SSO URL request response rejected because it "
           "was not a NSHTTPURLResponse";
    return nullptr;
  }

  return response;
}

bool IsOktaSSORequest(const network::ResourceRequest& request) {
  // Only match POST requests.
  if (request.method != "POST") {
    return false;
  }

  const GURL& gurl = request.url;
  // Only match HTTPS requests.
  if (!gurl.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  // Reject URLs with query parameters, fragments, or user credentials.
  if (gurl.has_query() || gurl.has_ref() || gurl.has_username() ||
      gurl.has_password()) {
    return false;
  }

  return MatchOktaSSOUrlPattern(gurl.path());
}

}  // namespace url_session_helper
