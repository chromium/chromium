// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_MATCHER_H_
#define COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_MATCHER_H_

#include <memory>
#include <vector>

#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace enterprise_custom_headers {

// Matches URLs against patterns defined in the HttpHeaderInjection policy
// and returns the headers that should be injected.
class HttpHeaderInjectionMatcher {
 public:
  static std::unique_ptr<HttpHeaderInjectionMatcher> Create();

  HttpHeaderInjectionMatcher(const HttpHeaderInjectionMatcher&) = delete;
  HttpHeaderInjectionMatcher& operator=(const HttpHeaderInjectionMatcher&) =
      delete;
  virtual ~HttpHeaderInjectionMatcher() = default;

  // Updates the matcher with a new set of `rules`.
  virtual void UpdateRules(
      const std::vector<HttpHeaderInjectionRule>& rules) = 0;

  // Returns the list of headers that should be injected for the given `url`.
  virtual net::HttpRequestHeaders GetHeadersForUrl(const GURL& url) const = 0;

  // Returns true if there are no rules in the matcher.
  virtual bool IsEmpty() const = 0;

 protected:
  HttpHeaderInjectionMatcher() = default;
};

}  // namespace enterprise_custom_headers

#endif  // COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_MATCHER_H_
