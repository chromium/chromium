// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_scheme_list_policy_handler.h"

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

const char kTestPolicyName[] = "kTestPolicyName";
const char kTestPrefName[] = "kTestPrefName";
const char kTestUrl[] = "https://www.example.com";
const char kNotAUrl[] = "htttps:///abce.NotAUrl..fgh";

}  // namespace

class URLSchemeListPolicyHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    std::string error;
    DCHECK(error.empty());
    handler_ = std::make_unique<URLSchemeListPolicyHandler>(kTestPolicyName,
                                                            kTestPrefName);
  }

 protected:
  void SetPolicy(const std::string& key, base::Value value) {
    policies_.Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }
  bool CheckPolicy(const std::string& key, std::optional<base::Value> value) {
    if (value)
      SetPolicy(key, value.value().Clone());
    return handler_->CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicies() { handler_->ApplyPolicySettings(policies_, &prefs_); }
  base::Value GetPolicyValueWithEntries(size_t len) {
    base::Value::List blocklist;
    for (size_t i = 0; i < len; ++i)
      blocklist.Append(kTestUrl);
    return base::Value(std::move(blocklist));
  }

  std::unique_ptr<URLSchemeListPolicyHandler> handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(URLSchemeListPolicyHandlerTest, CheckPolicySettings_EmptyPolicy) {
  EXPECT_TRUE(
      CheckPolicy(kTestPolicyName, base::Value(base::Value::Type::LIST)));
  EXPECT_TRUE(errors_.empty());
}

TEST_F(URLSchemeListPolicyHandlerTest, CheckPolicySettings_WrongType) {
  // The policy expects a list. Give it a boolean.
  EXPECT_FALSE(CheckPolicy(kTestPolicyName, base::Value(false)));
  EXPECT_EQ(1U, errors_.size());
  const std::string expected = kTestPolicyName;
  const std::string actual = errors_.begin()->first;
  EXPECT_EQ(expected, actual);
}

TEST_F(URLSchemeListPolicyHandlerTest, CheckPolicySettings_NoPolicy) {
  // The policy expects a list. Give it a boolean.
  EXPECT_TRUE(CheckPolicy(kTestPolicyName, std::nullopt));
  EXPECT_TRUE(errors_.empty());
}

TEST_F(URLSchemeListPolicyHandlerTest, CheckPolicySettings_OneBadValue) {
  // The policy expects a list. Give it a boolean.
  base::Value::List in;
  in.Append(kNotAUrl);
  in.Append(kTestUrl);
  EXPECT_TRUE(CheckPolicy(kTestPolicyName, base::Value(std::move(in))));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(URLSchemeListPolicyHandlerTest, CheckPolicySettings_SingleBadValue) {
  // The policy expects a list. Give it a boolean.
  base::Value::List in;
  in.Append(kNotAUrl);
  EXPECT_FALSE(CheckPolicy(kTestPolicyName, base::Value(std::move(in))));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(URLSchemeListPolicyHandlerTest, ApplyPolicySettings_NothingSpecified) {
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(kTestPolicyName, nullptr));
}

TEST_F(URLSchemeListPolicyHandlerTest, ApplyPolicySettings_WrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(kTestPolicyName, base::Value(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(kTestPrefName, nullptr));
}

TEST_F(URLSchemeListPolicyHandlerTest, ApplyPolicySettings_Empty) {
  SetPolicy(kTestPolicyName, base::Value(base::Value::Type::LIST));
  ApplyPolicies();
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(kTestPrefName, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_TRUE(out->GetList().empty());
}

TEST_F(URLSchemeListPolicyHandlerTest, ApplyPolicySettings_WrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  base::Value::List in;
  in.Append(kNotAUrl);
  in.Append(kTestUrl);
  SetPolicy(kTestPolicyName, base::Value(std::move(in)));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(kTestPrefName, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(1U, out->GetList().size());

  const std::string* out_string = out->GetList()[0].GetIfString();
  ASSERT_TRUE(out_string);
  EXPECT_EQ(kTestUrl, *out_string);
}

TEST_F(URLSchemeListPolicyHandlerTest, ApplyPolicySettings_BadUrl) {
  // The policy expects a gvalid url schema.
  base::Value::List in;
  in.Append(false);
  in.Append(kTestUrl);
  SetPolicy(kTestPolicyName, base::Value(std::move(in)));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(kTestPrefName, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(1U, out->GetList().size());

  const std::string* out_string = out->GetList()[0].GetIfString();
  ASSERT_TRUE(out_string);
  EXPECT_EQ(kTestUrl, *out_string);
}

TEST_F(URLSchemeListPolicyHandlerTest, ApplyPolicySettings_Successful) {
  base::Value::List in_url_blocklist;
  in_url_blocklist.Append(kTestUrl);
  SetPolicy(kTestPolicyName, base::Value(std::move(in_url_blocklist)));
  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(kTestPrefName, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(1U, out->GetList().size());

  const std::string* out_string = out->GetList()[0].GetIfString();
  ASSERT_TRUE(out_string);
  EXPECT_EQ(kTestUrl, *out_string);
}

TEST_F(URLSchemeListPolicyHandlerTest,
       ApplyPolicySettings_CheckPolicySettingsMaxFiltersLimitOK) {
  base::Value urls = GetPolicyValueWithEntries(policy::kMaxUrlFiltersPerPolicy);

  EXPECT_TRUE(CheckPolicy(kTestPolicyName, std::move(urls)));
  EXPECT_TRUE(errors_.empty());

  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(kTestPrefName, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(policy::kMaxUrlFiltersPerPolicy, out->GetList().size());
}

// Test that the warning message, mapped to
// |IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING|, is added to
// |errors_| when URLBlocklist entries exceed the max filters per policy limit.
TEST_F(URLSchemeListPolicyHandlerTest,
       ApplyPolicySettings_CheckPolicySettingsMaxFiltersLimitExceeded) {
  base::Value urls =
      GetPolicyValueWithEntries(policy::kMaxUrlFiltersPerPolicy + 1);

  EXPECT_TRUE(CheckPolicy(kTestPolicyName, std::move(urls)));
  EXPECT_EQ(1U, errors_.size());

  ApplyPolicies();

  auto error_str = errors_.GetErrorMessages(kTestPolicyName);
  auto expected_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
      base::NumberToString16(policy::kMaxUrlFiltersPerPolicy));
  EXPECT_TRUE(error_str.find(expected_str) != std::wstring::npos);

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(kTestPrefName, &out));
  ASSERT_TRUE(out->is_list());
  EXPECT_EQ(policy::kMaxUrlFiltersPerPolicy, out->GetList().size());
}

TEST_F(URLSchemeListPolicyHandlerTest, ValidatePolicyEntry) {
  std::vector<std::string> good{"http://*", "http:*",
                                "ws://example.org/component.js", "127.0.0.1:1",
                                "127.0.0.1:65535"};
  for (const auto& it : good)
    EXPECT_TRUE(handler_->ValidatePolicyEntry(&it));

  std::vector<std::string> bad{"wsgi:///rancom,org/", "127.0.0.1:65536"};
  for (const auto& it : bad)
    EXPECT_FALSE(handler_->ValidatePolicyEntry(&it));
}

}  // namespace policy
