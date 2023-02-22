// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_policy_handler.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_matcher/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// Note: this file should move to components/policy/core/browser, but the
// components_unittests runner does not load the ResourceBundle as
// ChromeTestSuite::Initialize does, which leads to failures using
// PolicyErrorMap.

namespace policy {

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
const char kTestDisabledScheme[] = "kTestDisabledScheme";
#endif
const char kTestBlocklistValue[] = "kTestBlocklistValue";

}  // namespace

class URLBlocklistPolicyHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<URLBlocklistPolicyHandler>(key::kURLBlocklist);
  }

 protected:
  void SetPolicy(const std::string& key, base::Value value) {
    policies_.Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }
  bool CheckPolicy(const std::string& key, base::Value value) {
    SetPolicy(key, std::move(value));
    return handler_->CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicies() { handler_->ApplyPolicySettings(policies_, &prefs_); }
  bool ValidatePolicy(const std::string& policy) {
    return handler_->ValidatePolicy(policy);
  }
  base::Value GetURLBlocklistPolicyValueWithEntries(size_t len) {
    base::Value::List blocklist;
    for (size_t i = 0; i < len; ++i)
      blocklist.Append(kTestBlocklistValue);
    return base::Value(std::move(blocklist));
  }

  std::unique_ptr<URLBlocklistPolicyHandler> handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(URLBlocklistPolicyHandlerTest,
       CheckPolicySettings_DisabledSchemesUnspecified) {
  EXPECT_TRUE(
      CheckPolicy(key::kURLBlocklist, base::Value(base::Value::Type::LIST)));
  EXPECT_EQ(0U, errors_.size());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(URLBlocklistPolicyHandlerTest,
       CheckPolicySettings_URLBlocklistUnspecified) {
  EXPECT_TRUE(
      CheckPolicy(key::kDisabledSchemes, base::Value(base::Value::Type::LIST)));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       CheckPolicySettings_DisabledSchemesWrongType) {
  // The policy expects a list. Give it a boolean.
  EXPECT_TRUE(CheckPolicy(key::kDisabledSchemes, base::Value(false)));
  EXPECT_EQ(1U, errors_.size());
  const std::string expected = key::kDisabledSchemes;
  const std::string actual = errors_.begin()->first;
  EXPECT_EQ(expected, actual);
}
#endif

TEST_F(URLBlocklistPolicyHandlerTest,
       CheckPolicySettings_URLBlocklistWrongType) {
  // The policy expects a list. Give it a boolean.
  EXPECT_TRUE(CheckPolicy(key::kURLBlocklist, base::Value(false)));
  EXPECT_EQ(1U, errors_.size());
  const std::string expected = key::kURLBlocklist;
  const std::string actual = errors_.begin()->first;
  EXPECT_EQ(expected, actual);
}

TEST_F(URLBlocklistPolicyHandlerTest, ApplyPolicySettings_NothingSpecified) {
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlocklist, nullptr));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesWrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(key::kDisabledSchemes, base::Value(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlocklist, nullptr));
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesEmpty) {
  SetPolicy(key::kDisabledSchemes, base::Value(base::Value::Type::LIST));
  ApplyPolicies();
  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(0U, out->GetList().size());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesWrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  base::Value::List in;
  in.Append(false);
  SetPolicy(key::kDisabledSchemes, base::Value(std::move(in)));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(0U, out->GetList().size());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesSuccessful) {
  base::Value::List in_disabled_schemes;
  in_disabled_schemes.Append(kTestDisabledScheme);
  SetPolicy(key::kDisabledSchemes, base::Value(std::move(in_disabled_schemes)));
  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(1U, out->GetList().size());

  const std::string* out_string = out->GetList()[0].GetIfString();
  ASSERT_TRUE(out_string);
  EXPECT_EQ(kTestDisabledScheme + std::string("://*"), *out_string);
}

TEST_F(URLBlocklistPolicyHandlerTest, ApplyPolicySettings_MergeSuccessful) {
  base::Value::List in_disabled_schemes;
  in_disabled_schemes.Append(kTestDisabledScheme);
  SetPolicy(key::kDisabledSchemes, base::Value(std::move(in_disabled_schemes)));

  base::Value::List in_url_blocklist;
  in_url_blocklist.Append(kTestBlocklistValue);
  SetPolicy(key::kURLBlocklist, base::Value(std::move(in_url_blocklist)));
  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  ASSERT_EQ(2U, out->GetList().size());

  const std::string* out_string1 = out->GetList()[0].GetIfString();
  ASSERT_TRUE(out_string1);
  EXPECT_EQ(kTestDisabledScheme + std::string("://*"), *out_string1);

  const std::string* out_string2 = out->GetList()[1].GetIfString();
  ASSERT_TRUE(out_string2);
  EXPECT_EQ(kTestBlocklistValue, *out_string2);
}

// Test that the warning message, mapped to
// |IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING|, is added to
// |errors_| when URLBlocklist + DisabledScheme entries exceed the max filters
// per policy limit.
TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_CheckPolicySettingsMaxFiltersLimitExceeded_2) {
  base::Value::List in_disabled_schemes;
  in_disabled_schemes.Append(kTestDisabledScheme);
  SetPolicy(key::kDisabledSchemes, base::Value(std::move(in_disabled_schemes)));

  size_t max_filters_per_policy = policy::kMaxUrlFiltersPerPolicy;
  base::Value urls =
      GetURLBlocklistPolicyValueWithEntries(max_filters_per_policy);

  EXPECT_TRUE(CheckPolicy(key::kURLBlocklist, std::move(urls)));
  EXPECT_EQ(1U, errors_.size());

  ApplyPolicies();

  auto error_str = errors_.GetErrorMessages(key::kURLBlocklist);
  auto expected_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
      base::NumberToString16(max_filters_per_policy));
  EXPECT_TRUE(error_str.find(expected_str) != std::wstring::npos);

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(max_filters_per_policy + 1, out->GetList().size());
}
#endif

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlocklistWrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(key::kURLBlocklist, base::Value(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlocklist, nullptr));
}

TEST_F(URLBlocklistPolicyHandlerTest, ApplyPolicySettings_URLBlocklistEmpty) {
  SetPolicy(key::kURLBlocklist, base::Value(base::Value::Type::LIST));
  ApplyPolicies();
  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(0U, out->GetList().size());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlocklistWrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  base::Value::List in;
  in.Append(false);
  SetPolicy(key::kURLBlocklist, base::Value(std::move(in)));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(0U, out->GetList().size());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlocklistSuccessful) {
  base::Value::List in_url_blocklist;
  in_url_blocklist.Append(kTestBlocklistValue);
  SetPolicy(key::kURLBlocklist, base::Value(std::move(in_url_blocklist)));
  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(1U, out->GetList().size());

  const std::string* out_string = out->GetList()[0].GetIfString();
  ASSERT_TRUE(out_string);
  EXPECT_EQ(kTestBlocklistValue, *out_string);
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_CheckPolicySettingsMaxFiltersLimitOK) {
  size_t max_filters_per_policy = policy::kMaxUrlFiltersPerPolicy;
  base::Value urls =
      GetURLBlocklistPolicyValueWithEntries(max_filters_per_policy);

  EXPECT_TRUE(CheckPolicy(key::kURLBlocklist, std::move(urls)));
  EXPECT_EQ(0U, errors_.size());

  ApplyPolicies();

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(max_filters_per_policy, out->GetList().size());
}

// Test that the warning message, mapped to
// |IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING|, is added to
// |errors_| when URLBlocklist entries exceed the max filters per policy limit.
TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_CheckPolicySettingsMaxFiltersLimitExceeded_1) {
  size_t max_filters_per_policy = policy::kMaxUrlFiltersPerPolicy;
  base::Value urls =
      GetURLBlocklistPolicyValueWithEntries(max_filters_per_policy + 1);

  EXPECT_TRUE(CheckPolicy(key::kURLBlocklist, std::move(urls)));
  EXPECT_EQ(1U, errors_.size());

  ApplyPolicies();

  auto error_str = errors_.GetErrorMessages(key::kURLBlocklist);
  auto expected_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
      base::NumberToString16(max_filters_per_policy));
  EXPECT_TRUE(error_str.find(expected_str) != std::wstring::npos);

  base::Value* out = nullptr;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(max_filters_per_policy + 1, out->GetList().size());
}

TEST_F(URLBlocklistPolicyHandlerTest, ValidatePolicy) {
  EXPECT_TRUE(ValidatePolicy("http://*"));
  EXPECT_TRUE(ValidatePolicy("http:*"));

  EXPECT_TRUE(ValidatePolicy("ws://example.org/component.js"));
  EXPECT_FALSE(ValidatePolicy("wsgi:///rancom,org/"));

  EXPECT_TRUE(ValidatePolicy("127.0.0.1:1"));
  EXPECT_TRUE(ValidatePolicy("127.0.0.1:65535"));
  EXPECT_FALSE(ValidatePolicy("127.0.0.1:65536"));

  EXPECT_TRUE(ValidatePolicy("*"));
  EXPECT_FALSE(ValidatePolicy("*.developers.com"));
}

// When the invalid sequence with '*' in the host is added to the blocklist, the
// policy can still be applied, but an error is added to the error map to
// indicate an invalid URL.
TEST_F(URLBlocklistPolicyHandlerTest, CheckPolicyURLHostWithAsterik) {
  base::Value::List blocked_urls;
  blocked_urls.Append("android.*.com");
  EXPECT_TRUE(
      CheckPolicy(key::kURLBlocklist, base::Value(std::move(blocked_urls))));
  EXPECT_EQ(1U, errors_.size());
}

}  // namespace policy
