// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_policy_handler.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

// Note: this file should move to components/policy/core/browser, but the
// components_unittests runner does not load the ResourceBundle as
// ChromeTestSuite::Initialize does, which leads to failures using
// PolicyErrorMap.

namespace policy {

namespace {

const char kTestDisabledScheme[] = "kTestDisabledScheme";
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

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesWrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(key::kDisabledSchemes, base::Value(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlocklist, nullptr));
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlocklistWrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(key::kURLBlocklist, base::Value(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlocklist, nullptr));
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesEmpty) {
  SetPolicy(key::kDisabledSchemes, base::Value(base::Value::Type::LIST));
  ApplyPolicies();
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlocklistPolicyHandlerTest, ApplyPolicySettings_URLBlocklistEmpty) {
  SetPolicy(key::kURLBlocklist, base::Value(base::Value::Type::LIST));
  ApplyPolicies();
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesWrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  base::Value in(base::Value::Type::LIST);
  in.Append(false);
  SetPolicy(key::kDisabledSchemes, std::move(in));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlocklistWrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  base::Value in(base::Value::Type::LIST);
  in.Append(false);
  SetPolicy(key::kURLBlocklist, std::move(in));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesSuccessful) {
  base::Value in_disabled_schemes(base::Value::Type::LIST);
  in_disabled_schemes.Append(kTestDisabledScheme);
  SetPolicy(key::kDisabledSchemes, std::move(in_disabled_schemes));
  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(1U, out_list->GetSize());

  std::string out_string;
  EXPECT_TRUE(out_list->GetString(0U, &out_string));
  EXPECT_EQ(kTestDisabledScheme + std::string("://*"), out_string);
}

TEST_F(URLBlocklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlocklistSuccessful) {
  base::Value in_url_blocklist(base::Value::Type::LIST);
  in_url_blocklist.Append(kTestBlocklistValue);
  SetPolicy(key::kURLBlocklist, std::move(in_url_blocklist));
  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(1U, out_list->GetSize());

  std::string out_string;
  EXPECT_TRUE(out_list->GetString(0U, &out_string));
  EXPECT_EQ(kTestBlocklistValue, out_string);
}

TEST_F(URLBlocklistPolicyHandlerTest, ApplyPolicySettings_MergeSuccessful) {
  base::Value in_disabled_schemes(base::Value::Type::LIST);
  in_disabled_schemes.Append(kTestDisabledScheme);
  SetPolicy(key::kDisabledSchemes, std::move(in_disabled_schemes));

  base::Value in_url_blocklist(base::Value::Type::LIST);
  in_url_blocklist.Append(kTestBlocklistValue);
  SetPolicy(key::kURLBlocklist, std::move(in_url_blocklist));
  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlocklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(2U, out_list->GetSize());

  std::string out1;
  EXPECT_TRUE(out_list->GetString(0U, &out1));
  EXPECT_EQ(kTestDisabledScheme + std::string("://*"), out1);

  std::string out2;
  EXPECT_TRUE(out_list->GetString(1U, &out2));
  EXPECT_EQ(kTestBlocklistValue, out2);
}

}  // namespace policy
