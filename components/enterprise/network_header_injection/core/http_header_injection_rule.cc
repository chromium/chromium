// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"

namespace enterprise_custom_headers {

// static
std::optional<HttpHeaderInjectionRule> HttpHeaderInjectionRule::FromValue(
    const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }
  const base::DictValue& rule_dict = value.GetDict();

  const base::ListValue* policy_patterns = rule_dict.FindList(kKeyPatterns);
  const base::ListValue* headers = rule_dict.FindList(kKeyHeaders);

  if (!policy_patterns || !headers) {
    return std::nullopt;
  }

  HttpHeaderInjectionRule rule;

  for (const auto& pattern_value : *policy_patterns) {
    if (pattern_value.is_string()) {
      rule.url_patterns.push_back(pattern_value.GetString());
    }
  }

  for (const auto& header_value : *headers) {
    if (!header_value.is_dict()) {
      continue;
    }
    const base::DictValue& header_dict = header_value.GetDict();

    const std::string* name = header_dict.FindString(kKeyHeaderName);
    const std::string* val = header_dict.FindString(kKeyHeaderValue);

    if (name && val) {
      rule.headers.emplace_back(*name, *val);
    }
  }

  if (rule.url_patterns.empty() || rule.headers.empty()) {
    return std::nullopt;
  }

  return rule;
}

}  // namespace enterprise_custom_headers
