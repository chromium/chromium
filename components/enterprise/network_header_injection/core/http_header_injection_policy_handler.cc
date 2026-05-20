// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_policy_handler.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_matcher/url_util.h"
#include "net/http/http_util.h"

namespace enterprise_custom_headers {

namespace {

constexpr char kKeyPatterns[] = "patterns";
constexpr char kKeyHeaders[] = "headers";
constexpr char kKeyHeaderName[] = "name";
constexpr char kKeyHeaderValue[] = "value";

constexpr size_t kMaxUrlPatterns = 500;
constexpr size_t kMaxHeadersPerRule = 20;
constexpr size_t kMaxHeaderSize = 1024 * 8;  // 8KB

// Has an additional check for host containing an * (asterisk) that would have
// no effect on the domain or subdomain. It is a common mistake that admins
// allow sites with * as a wildcard in the hostname although it has no effect on
// the domain and subdomains. Two example for such a common mistake are: 1-
// *.android.com 2- developer.*.com which allow neither android.com nor
// developer.android.com
bool IsValidUrlPattern(const std::string& url_pattern) {
  url_matcher::util::FilterComponents components;
  return url_matcher::util::FilterToComponents(
             url_pattern, &components.scheme, &components.host,
             &components.match_subdomains, &components.port, &components.path,
             &components.query) &&
         (components.host == "*" || !components.host.contains('*'));
}

// Validates that the policy complies with limits on the number of URL patterns,
// number of headers per rule, and header size. It also checks that URL
// patterns, header names, and header values are valid.
//
// If `errors` is not null, any validation errors are added to it.
// If `filtered_rules` is not null, valid rules (and valid components within
// rules) are added to this list, effectively filtering out invalid entries.
void ProcessPolicy(const std::string& policy_name,
                   const base::Value& policy_value,
                   policy::PolicyErrorMap* errors,
                   base::ListValue* filtered_rules) {
  size_t total_patterns = 0;
  bool rule_header_limit_exceeded = false;
  bool header_size_limit_exceeded = false;
  std::vector<std::string> invalid_patterns;
  std::vector<std::string> invalid_header_names;
  std::vector<std::string> invalid_header_values;

  for (const base::Value& rule_value : policy_value.GetList()) {
    if (!rule_value.is_dict()) {
      continue;
    }
    const base::DictValue& rule_dict = rule_value.GetDict();

    const base::ListValue* patterns = rule_dict.FindList(kKeyPatterns);
    const base::ListValue* headers = rule_dict.FindList(kKeyHeaders);

    // Store validated values that will be added to the policy store.
    base::DictValue filtered_rule;
    base::ListValue filtered_patterns;
    base::ListValue filtered_headers;

    if (patterns) {
      for (const base::Value& pattern_value : *patterns) {
        if (pattern_value.is_string()) {
          const std::string& pattern = pattern_value.GetString();
          if (IsValidUrlPattern(pattern)) {
            if (filtered_rules && total_patterns < kMaxUrlPatterns) {
              filtered_patterns.Append(pattern);
            }
            total_patterns++;
          } else if (errors) {
            invalid_patterns.push_back(pattern);
          }
        }
      }
    }

    if (headers) {
      size_t rule_headers_count = headers->size();
      if (rule_headers_count > kMaxHeadersPerRule) {
        rule_header_limit_exceeded = true;
      }
      size_t headers_added = 0;
      for (const base::Value& header_value : *headers) {
        if (header_value.is_dict()) {
          const base::DictValue& header_dict = header_value.GetDict();
          const std::string* name = header_dict.FindString(kKeyHeaderName);
          const std::string* val = header_dict.FindString(kKeyHeaderValue);

          if (name && val) {
            bool valid_size =
                (name->length() + val->length() <= kMaxHeaderSize);
            bool valid_name = net::HttpUtil::IsValidHeaderName(*name);
            bool valid_val = net::HttpUtil::IsValidHeaderValue(*val);

            if (!valid_size) {
              header_size_limit_exceeded = true;
            }
            if (!valid_name && errors) {
              invalid_header_names.push_back(*name);
            }
            if (!valid_val && errors) {
              invalid_header_values.push_back(*val);
            }

            if (valid_size && valid_name && valid_val &&
                headers_added < kMaxHeadersPerRule && filtered_rules) {
              filtered_headers.Append(header_dict.Clone());
              headers_added++;
            }
          }
        }
      }
    }

    if (filtered_rules && !filtered_patterns.empty() &&
        !filtered_headers.empty()) {
      filtered_rule.Set(kKeyPatterns, std::move(filtered_patterns));
      filtered_rule.Set(kKeyHeaders, std::move(filtered_headers));
      filtered_rules->Append(std::move(filtered_rule));
    }
  }

  if (errors) {
    if (total_patterns > kMaxUrlPatterns) {
      errors->AddError(
          policy_name,
          IDS_POLICY_HTTP_HEADER_INJECTION_MAX_URL_PATTERNS_LIMIT_ERROR,
          base::NumberToString(kMaxUrlPatterns));
    }

    if (rule_header_limit_exceeded) {
      errors->AddError(
          policy_name,
          IDS_POLICY_HTTP_HEADER_INJECTION_MAX_HEADERS_PER_RULE_LIMIT_ERROR,
          base::NumberToString(kMaxHeadersPerRule));
    }

    if (header_size_limit_exceeded) {
      errors->AddError(
          policy_name,
          IDS_POLICY_HTTP_HEADER_INJECTION_MAX_HEADER_SIZE_LIMIT_ERROR,
          base::NumberToString(kMaxHeaderSize / 1024));
    }

    if (!invalid_header_names.empty()) {
      errors->AddError(policy_name, IDS_POLICY_PROTO_PARSING_ERROR,
                       base::JoinString(invalid_header_names, ", "));
    }

    if (!invalid_header_values.empty()) {
      errors->AddError(policy_name, IDS_POLICY_PROTO_PARSING_ERROR,
                       base::JoinString(invalid_header_values, ", "));
    }

    if (!invalid_patterns.empty()) {
      errors->AddError(policy_name, IDS_POLICY_PROTO_PARSING_ERROR,
                       base::JoinString(invalid_patterns, ", "));
    }
  }
}

}  // namespace

HttpHeaderInjectionPolicyHandler::HttpHeaderInjectionPolicyHandler(
    policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy::key::kHttpHeaderInjection,
          prefs::kHttpHeaderInjection,
          schema,
          policy::SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

HttpHeaderInjectionPolicyHandler::~HttpHeaderInjectionPolicyHandler() = default;

bool HttpHeaderInjectionPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                errors)) {
    return false;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value) {
    return true;
  }

  ProcessPolicy(policy_name(), *value, errors, nullptr);

  return true;  // Always return true to always apply the policy. Wrong values
                // will be filtered out in ApplyPolicySettings method.
}

void HttpHeaderInjectionPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!policy_value) {
    return;
  }

  base::ListValue filtered_rules;
  ProcessPolicy(policy_name(), *policy_value, nullptr, &filtered_rules);

  prefs->SetValue(prefs::kHttpHeaderInjection,
                  base::Value(std::move(filtered_rules)));
}

}  // namespace enterprise_custom_headers
