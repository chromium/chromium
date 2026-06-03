// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_RULE_H_
#define COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_RULE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"

namespace enterprise_custom_headers {

inline constexpr char kKeyPatterns[] = "patterns";
inline constexpr char kKeyHeaders[] = "headers";
inline constexpr char kKeyHeaderName[] = "name";
inline constexpr char kKeyHeaderValue[] = "value";

// Structured representation of a policy rule for HTTP header injection.
struct HttpHeaderInjectionRule {
  // Helper to construct a rule from a base::Value representation.
  // Returns std::nullopt if the value doesn't represent a valid rule.
  static std::optional<HttpHeaderInjectionRule> FromValue(
      const base::Value& value);

  std::vector<std::string> url_patterns;
  std::vector<std::pair<std::string, std::string>> headers;
};

}  // namespace enterprise_custom_headers

#endif  // COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_RULE_H_
