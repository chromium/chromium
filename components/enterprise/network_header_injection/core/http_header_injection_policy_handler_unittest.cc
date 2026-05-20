// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_policy_handler.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_custom_headers {

namespace {

constexpr char kKeyPatterns[] = "patterns";
constexpr char kKeyHeaders[] = "headers";
constexpr char kKeyHeaderName[] = "name";
constexpr char kKeyHeaderValue[] = "value";

}  // namespace

class HttpHeaderInjectionPolicyHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    policy::Schema chrome_schema =
        policy::Schema::Wrap(policy::GetChromeSchemaData());
    handler_ =
        std::make_unique<HttpHeaderInjectionPolicyHandler>(chrome_schema);
  }

 protected:
  void SetPolicy(base::Value value) {
    policies_.Set(policy::key::kHttpHeaderInjection,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }

  bool CheckPolicy() {
    return handler_->CheckPolicySettings(policies_, &errors_);
  }

  void ApplyPolicies() { handler_->ApplyPolicySettings(policies_, &prefs_); }

  struct TestRule {
    std::vector<std::string> patterns;
    std::vector<std::pair<std::string, std::string>> headers;
  };

  base::ListValue CreateRulesList(const std::vector<TestRule>& rules) {
    base::ListValue rules_list;
    for (const auto& test_rule : rules) {
      base::DictValue rule_dict;

      base::ListValue pattern_list;
      for (const auto& pattern : test_rule.patterns) {
        pattern_list.Append(pattern);
      }
      rule_dict.Set(kKeyPatterns, std::move(pattern_list));

      base::ListValue header_list;
      for (const auto& [name, value] : test_rule.headers) {
        base::DictValue header_dict;
        header_dict.Set(kKeyHeaderName, name);
        header_dict.Set(kKeyHeaderValue, value);
        header_list.Append(std::move(header_dict));
      }
      rule_dict.Set(kKeyHeaders, std::move(header_list));

      rules_list.Append(std::move(rule_dict));
    }
    return rules_list;
  }

  [[nodiscard]] testing::AssertionResult VerifyPrefEmpty() {
    base::Value* out = nullptr;
    if (!prefs_.GetValue(prefs::kHttpHeaderInjection, &out)) {
      return testing::AssertionFailure()
             << "No prefs::kHttpHeaderInjection in prefs";
    }
    if (!out) {
      return testing::AssertionFailure() << "Pref value is null";
    }
    if (!out->is_list()) {
      return testing::AssertionFailure()
             << "Pref value is not a list: " << out->type();
    }
    if (!out->GetList().empty()) {
      return testing::AssertionFailure()
             << "Pref list is not empty, contains " << out->GetList().size()
             << " elements";
    }
    return testing::AssertionSuccess();
  }

  std::unique_ptr<HttpHeaderInjectionPolicyHandler> handler_;
  policy::PolicyErrorMap errors_;
  policy::PolicyMap policies_;
  PrefValueMap prefs_;
};

// Tests that a valid policy is correctly applied to the preference store.
TEST_F(HttpHeaderInjectionPolicyHandlerTest, ValidPolicyIsCorrectlyApplied) {
  base::ListValue rules =
      CreateRulesList({{.patterns = {"example.com"},
                        .headers = {{"X-Test-Header", "TestValue"}}}});

  base::Value policy_value(rules.Clone());
  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_TRUE(errors_.GetErrors(policy::key::kHttpHeaderInjection).empty());
  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHttpHeaderInjection, &out));
  ASSERT_TRUE(out);
  EXPECT_EQ(*out, policy_value);
}

// Tests that an invalid URL pattern in the policy yields a parsing error.
TEST_F(HttpHeaderInjectionPolicyHandlerTest, InvalidUrlPatternYieldsError) {
  base::ListValue rules =
      CreateRulesList({{.patterns = {"wsgi:///rancom,org/"},
                        .headers = {{"X-Test-Header", "TestValue"}}}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"Policy parsing error: wsgi:///rancom,org/");

  ApplyPolicies();
  EXPECT_TRUE(VerifyPrefEmpty());
}

// Tests that a URL pattern with an invalid wildcard yields a parsing error.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       UrlPatternWithInvalidWildcardYieldsError) {
  base::ListValue rules =
      CreateRulesList({{.patterns = {"*.example.com"},
                        .headers = {{"X-Test-Header", "TestValue"}}}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"Policy parsing error: *.example.com");

  ApplyPolicies();
  EXPECT_TRUE(VerifyPrefEmpty());
}

// Tests that a URL pattern that is exactly "*" is valid.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       UrlPatternWithValidWildcardYieldsNoError) {
  base::ListValue rules = CreateRulesList(
      {{.patterns = {"*"}, .headers = {{"X-Test-Header", "TestValue"}}}});

  base::Value policy_value(rules.Clone());
  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_TRUE(errors_.GetErrors(policy::key::kHttpHeaderInjection).empty());

  ApplyPolicies();
  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHttpHeaderInjection, &out));
  ASSERT_TRUE(out);
  EXPECT_EQ(*out, policy_value);
}

// Tests that exceeding the limit of headers per rule yields an error.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       CheckPolicySettings_MaxHeadersPerRuleExceeded) {
  std::vector<std::pair<std::string, std::string>> headers;
  for (int i = 0; i < 21; ++i) {
    headers.emplace_back("X-Test-Header-" + base::NumberToString(i), "Value");
  }
  base::ListValue rules =
      CreateRulesList({{.patterns = {"example.com"}, .headers = headers}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"This policy allows up to 20 headers per rule. Only the first 20 "
            u"headers per rule will be used.");

  ApplyPolicies();
  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHttpHeaderInjection, &out));
  ASSERT_TRUE(out);
  ASSERT_TRUE(out->is_list());
  const auto& list = out->GetList();
  ASSERT_EQ(list.size(), 1u);
  const base::Value& rule = *list.begin();
  ASSERT_TRUE(rule.is_dict());
  const base::ListValue* out_headers = rule.GetDict().FindList(kKeyHeaders);
  ASSERT_TRUE(out_headers);
  EXPECT_EQ(out_headers->size(), 20u);
}

// Tests that exceeding the size limit for a header yields an error.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       CheckPolicySettings_MaxHeaderSizeExceeded) {
  std::string large_value(8192, 'a');
  base::ListValue rules =
      CreateRulesList({{.patterns = {"example.com"},
                        .headers = {{"X-Large-Header", large_value}}}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"This policy should not have headers exceeding 8KB.");

  ApplyPolicies();
  EXPECT_TRUE(VerifyPrefEmpty());
}

// Tests that an invalid header name yields an error.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       CheckPolicySettings_InvalidHeaderName) {
  base::ListValue rules = CreateRulesList(
      {{.patterns = {"example.com"}, .headers = {{"Invalid Name", "Value"}}}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"Policy parsing error: Invalid Name");

  ApplyPolicies();
  EXPECT_TRUE(VerifyPrefEmpty());
}

// Tests that an invalid header value yields an error.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       CheckPolicySettings_InvalidHeaderValue) {
  base::ListValue rules =
      CreateRulesList({{.patterns = {"example.com"},
                        .headers = {{"X-Test-Header", "Invalid\nValue"}}}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"Policy parsing error: Invalid\nValue");

  ApplyPolicies();
  EXPECT_TRUE(VerifyPrefEmpty());
}

// Tests that invalid patterns and oversized headers
// are filtered out when applying the policy settings to preferences.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       ApplyPolicySettings_FiltersViolatingValues) {
  std::vector<std::pair<std::string, std::string>> headers;
  headers.emplace_back("Cookie", "Value");  // Forbidden
  std::string large_value(8192, 'a');
  headers.emplace_back("X-Large-Header", large_value);      // Oversized
  headers.emplace_back("Invalid Name", "Value");            // Invalid name
  headers.emplace_back("X-Test-Header", "Invalid\nValue");  // Invalid value
  for (int i = 0; i < 25; ++i) {
    headers.emplace_back("X-Test-Header-" + base::NumberToString(i), "Value");
  }

  base::ListValue rules =
      CreateRulesList({{.patterns = {"example.com", "wsgi:///rancom,org/"},
                        .headers = headers}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHttpHeaderInjection, &out));
  ASSERT_TRUE(out);
  ASSERT_TRUE(out->is_list());

  const auto& list = out->GetList();
  ASSERT_EQ(list.size(),
            1u);  // Rule should still exist because it has some valid parts

  const base::Value& rule = *list.begin();
  ASSERT_TRUE(rule.is_dict());
  const base::DictValue& dict = rule.GetDict();

  const base::ListValue* out_patterns = dict.FindList(kKeyPatterns);
  ASSERT_TRUE(out_patterns);
  EXPECT_EQ(out_patterns->size(), 1u);
  EXPECT_EQ((*out_patterns)[0].GetString(), "example.com");

  const base::ListValue* out_headers = dict.FindList(kKeyHeaders);
  ASSERT_TRUE(out_headers);
  EXPECT_EQ(out_headers->size(), 20u);  // Limited to 20
  // Check that invalid headers are not there.
  for (const auto& h : *out_headers) {
    const std::string* name = h.GetDict().FindString(kKeyHeaderName);
    EXPECT_NE(*name, "X-Large-Header");
    EXPECT_NE(*name, "Invalid Name");
    EXPECT_NE(*name, "X-Test-Header");
  }
}

// Tests that ApplyPolicySettings truncates the total number of URL patterns to
// 500.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       ApplyPolicySettings_MaxUrlPatternsTruncated) {
  std::vector<std::string> patterns;
  for (int i = 0; i < 505; ++i) {
    patterns.emplace_back("example" + base::NumberToString(i) + ".com");
  }
  base::ListValue rules = CreateRulesList(
      {{.patterns = patterns, .headers = {{"X-Test-Header", "TestValue"}}}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"This policy has more than 500 url patterns. Only the first 500 "
            u"url patterns will be used.");
  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHttpHeaderInjection, &out));
  ASSERT_TRUE(out);
  ASSERT_TRUE(out->is_list());

  const auto& list = out->GetList();
  ASSERT_EQ(list.size(), 1u);

  const base::Value& rule = *list.begin();
  ASSERT_TRUE(rule.is_dict());
  const base::DictValue& dict = rule.GetDict();

  const base::ListValue* out_patterns = dict.FindList(kKeyPatterns);
  ASSERT_TRUE(out_patterns);
  EXPECT_EQ(out_patterns->size(), 500u);
}

// Tests that ApplyPolicySettings truncates the total number of URL patterns to
// 500 across multiple rules.
TEST_F(HttpHeaderInjectionPolicyHandlerTest,
       ApplyPolicySettings_MaxUrlPatternsTruncatedAcrossRules) {
  std::vector<std::string> patterns1;
  for (int i = 0; i < 300; ++i) {
    patterns1.emplace_back("rule1-" + base::NumberToString(i) + ".com");
  }
  std::vector<std::string> patterns2;
  for (int i = 0; i < 250; ++i) {
    patterns2.emplace_back("rule2-" + base::NumberToString(i) + ".com");
  }
  base::ListValue rules = CreateRulesList(
      {{.patterns = patterns1, .headers = {{"X-Test-Header-1", "Value1"}}},
       {.patterns = patterns2, .headers = {{"X-Test-Header-2", "Value2"}}}});

  SetPolicy(base::Value(std::move(rules)));

  EXPECT_TRUE(CheckPolicy());
  EXPECT_EQ(errors_.GetErrorMessages(policy::key::kHttpHeaderInjection),
            u"This policy has more than 500 url patterns. Only the first 500 "
            u"url patterns will be used.");
  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHttpHeaderInjection, &out));
  ASSERT_TRUE(out);
  ASSERT_TRUE(out->is_list());

  const auto& list = out->GetList();
  ASSERT_EQ(list.size(), 2u);

  const base::Value& rule1 = list[0];
  const base::ListValue* out_patterns1 = rule1.GetDict().FindList(kKeyPatterns);
  ASSERT_TRUE(out_patterns1);
  EXPECT_EQ(out_patterns1->size(), 300u);

  const base::Value& rule2 = list[1];
  const base::ListValue* out_patterns2 = rule2.GetDict().FindList(kKeyPatterns);
  ASSERT_TRUE(out_patterns2);
  EXPECT_EQ(out_patterns2->size(), 200u);
}

}  // namespace enterprise_custom_headers
