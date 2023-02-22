// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_allowlist_policy_handler.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
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

namespace policy {

namespace {

const char kTestAllowlistValue[] = "kTestAllowlistValue";

}  // namespace

class URLAllowlistPolicyHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<URLAllowlistPolicyHandler>(key::kURLAllowlist);
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
  base::Value GetURLAllowlistPolicyValueWithEntries(size_t len) {
    base::Value::List allowlist;
    for (size_t i = 0; i < len; ++i)
      allowlist.Append(kTestAllowlistValue);
    return base::Value(std::move(allowlist));
  }

  std::unique_ptr<URLAllowlistPolicyHandler> handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(URLAllowlistPolicyHandlerTest, CheckPolicySettings_WrongType) {
  // The policy expects a list. Give it a boolean.
  EXPECT_TRUE(CheckPolicy(key::kURLAllowlist, base::Value(false)));
  EXPECT_EQ(1U, errors_.size());
  const std::string expected = key::kURLAllowlist;
  const std::string actual = errors_.begin()->first;
  EXPECT_EQ(expected, actual);
}

TEST_F(URLAllowlistPolicyHandlerTest, ApplyPolicySettings_NothingSpecified) {
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlAllowlist, nullptr));
}

TEST_F(URLAllowlistPolicyHandlerTest, ApplyPolicySettings_WrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(key::kURLAllowlist, base::Value(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlAllowlist, nullptr));
}

TEST_F(URLAllowlistPolicyHandlerTest, ApplyPolicySettings_Empty) {
  SetPolicy(key::kURLAllowlist, base::Value(base::Value::Type::LIST));
  ApplyPolicies();
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlAllowlist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(0U, out->GetList().size());
}

TEST_F(URLAllowlistPolicyHandlerTest, ApplyPolicySettings_WrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  base::Value::List in;
  in.Append(false);
  SetPolicy(key::kURLAllowlist, base::Value(std::move(in)));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlAllowlist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(0U, out->GetList().size());
}

TEST_F(URLAllowlistPolicyHandlerTest, ApplyPolicySettings_Successful) {
  base::Value::List in_url_allowlist;
  in_url_allowlist.Append(kTestAllowlistValue);
  SetPolicy(key::kURLAllowlist, base::Value(std::move(in_url_allowlist)));
  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlAllowlist, &out));
  ASSERT_TRUE(out->is_list());
  ASSERT_EQ(1U, out->GetList().size());

  const std::string* out_string = out->GetList()[0].GetIfString();
  ASSERT_TRUE(out_string);
  EXPECT_EQ(kTestAllowlistValue, *out_string);
}

TEST_F(URLAllowlistPolicyHandlerTest,
       ApplyPolicySettings_CheckPolicySettingsMaxFiltersLimitOK) {
  size_t max_filters_per_policy = policy::kMaxUrlFiltersPerPolicy;
  base::Value urls =
      GetURLAllowlistPolicyValueWithEntries(max_filters_per_policy);

  EXPECT_TRUE(CheckPolicy(key::kURLAllowlist, std::move(urls)));
  EXPECT_EQ(0U, errors_.size());

  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlAllowlist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(max_filters_per_policy, out->GetList().size());
}

// Test that the warning message, mapped to
// |IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING|, is added to
// |errors_| when URLAllowlist entries exceed the max filters per policy limit.
TEST_F(URLAllowlistPolicyHandlerTest,
       ApplyPolicySettings_CheckPolicySettingsMaxFiltersLimitExceeded) {
  size_t max_filters_per_policy = policy::kMaxUrlFiltersPerPolicy;
  base::Value urls =
      GetURLAllowlistPolicyValueWithEntries(max_filters_per_policy + 1);

  EXPECT_TRUE(CheckPolicy(key::kURLAllowlist, std::move(urls)));
  EXPECT_EQ(1U, errors_.size());

  ApplyPolicies();

  auto error_str = errors_.GetErrorMessages(key::kURLAllowlist);
  auto expected_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
      base::NumberToString16(max_filters_per_policy));
  EXPECT_TRUE(error_str.find(expected_str) != std::wstring::npos);

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlAllowlist, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(max_filters_per_policy + 1, out->GetList().size());
}

TEST_F(URLAllowlistPolicyHandlerTest, ValidatePolicy) {
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

// When the invalid sequence with '*' in the host is added to the allowlist, the
// policy can still be applied, but an error is added to the error map to
// indicate an invalid URL.
TEST_F(URLAllowlistPolicyHandlerTest, CheckPolicyURLHostWithAsterik) {
  base::Value::List allowed_urls;
  allowed_urls.Append("*.developers.com");
  EXPECT_TRUE(
      CheckPolicy(key::kURLAllowlist, base::Value(std::move(allowed_urls))));
  EXPECT_EQ(1U, errors_.size());
}

}  // namespace policy
