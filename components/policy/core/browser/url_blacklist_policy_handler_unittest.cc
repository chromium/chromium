// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blacklist_policy_handler.h"

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
const char kTestBlacklistValue[] = "kTestBlacklistValue";

}  // namespace

class URLBlacklistPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicy(const std::string& key, std::unique_ptr<base::Value> value) {
    policies_.Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }
  bool CheckPolicy(const std::string& key, std::unique_ptr<base::Value> value) {
    SetPolicy(key, std::move(value));
    return handler_.CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicies() {
    handler_.ApplyPolicySettings(policies_, &prefs_);
  }

  URLBlacklistPolicyHandler handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(URLBlacklistPolicyHandlerTest,
       CheckPolicySettings_DisabledSchemesUnspecified) {
  EXPECT_TRUE(
      CheckPolicy(key::kURLBlacklist, std::make_unique<base::ListValue>()));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(URLBlacklistPolicyHandlerTest,
       CheckPolicySettings_URLBlacklistUnspecified) {
  EXPECT_TRUE(
      CheckPolicy(key::kDisabledSchemes, std::make_unique<base::ListValue>()));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(URLBlacklistPolicyHandlerTest,
       CheckPolicySettings_DisabledSchemesWrongType) {
  // The policy expects a list. Give it a boolean.
  EXPECT_TRUE(
      CheckPolicy(key::kDisabledSchemes, std::make_unique<base::Value>(false)));
  EXPECT_EQ(1U, errors_.size());
  const std::string expected = key::kDisabledSchemes;
  const std::string actual = errors_.begin()->first;
  EXPECT_EQ(expected, actual);
}

TEST_F(URLBlacklistPolicyHandlerTest,
       CheckPolicySettings_URLBlacklistWrongType) {
  // The policy expects a list. Give it a boolean.
  EXPECT_TRUE(
      CheckPolicy(key::kURLBlacklist, std::make_unique<base::Value>(false)));
  EXPECT_EQ(1U, errors_.size());
  const std::string expected = key::kURLBlacklist;
  const std::string actual = errors_.begin()->first;
  EXPECT_EQ(expected, actual);
}

TEST_F(URLBlacklistPolicyHandlerTest, ApplyPolicySettings_NothingSpecified) {
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlacklist, nullptr));
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesWrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(key::kDisabledSchemes, std::make_unique<base::Value>(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlacklist, nullptr));
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlacklistWrongType) {
  // The policy expects a list. Give it a boolean.
  SetPolicy(key::kURLBlacklist, std::make_unique<base::Value>(false));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(policy_prefs::kUrlBlacklist, nullptr));
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesEmpty) {
  SetPolicy(key::kDisabledSchemes, std::make_unique<base::ListValue>());
  ApplyPolicies();
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlacklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlacklistEmpty) {
  SetPolicy(key::kURLBlacklist, std::make_unique<base::ListValue>());
  ApplyPolicies();
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlacklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesWrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  std::unique_ptr<base::ListValue> in(new base::ListValue);
  in->AppendBoolean(false);
  SetPolicy(key::kDisabledSchemes, std::move(in));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlacklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlacklistWrongElementType) {
  // The policy expects string-valued elements. Give it booleans.
  std::unique_ptr<base::ListValue> in(new base::ListValue);
  in->AppendBoolean(false);
  SetPolicy(key::kURLBlacklist, std::move(in));
  ApplyPolicies();

  // The element should be skipped.
  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlacklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(0U, out_list->GetSize());
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_DisabledSchemesSuccessful) {
  std::unique_ptr<base::ListValue> in_disabled_schemes(new base::ListValue);
  in_disabled_schemes->AppendString(kTestDisabledScheme);
  SetPolicy(key::kDisabledSchemes, std::move(in_disabled_schemes));
  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlacklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(1U, out_list->GetSize());

  std::string out_string;
  EXPECT_TRUE(out_list->GetString(0U, &out_string));
  EXPECT_EQ(kTestDisabledScheme + std::string("://*"), out_string);
}

TEST_F(URLBlacklistPolicyHandlerTest,
       ApplyPolicySettings_URLBlacklistSuccessful) {
  std::unique_ptr<base::ListValue> in_url_blacklist(new base::ListValue);
  in_url_blacklist->AppendString(kTestBlacklistValue);
  SetPolicy(key::kURLBlacklist, std::move(in_url_blacklist));
  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlacklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(1U, out_list->GetSize());

  std::string out_string;
  EXPECT_TRUE(out_list->GetString(0U, &out_string));
  EXPECT_EQ(kTestBlacklistValue, out_string);
}

TEST_F(URLBlacklistPolicyHandlerTest, ApplyPolicySettings_MergeSuccessful) {
  std::unique_ptr<base::ListValue> in_disabled_schemes(new base::ListValue);
  in_disabled_schemes->AppendString(kTestDisabledScheme);
  SetPolicy(key::kDisabledSchemes, std::move(in_disabled_schemes));

  std::unique_ptr<base::ListValue> in_url_blacklist(new base::ListValue);
  in_url_blacklist->AppendString(kTestBlacklistValue);
  SetPolicy(key::kURLBlacklist, std::move(in_url_blacklist));

  ApplyPolicies();

  base::Value* out;
  EXPECT_TRUE(prefs_.GetValue(policy_prefs::kUrlBlacklist, &out));
  base::ListValue* out_list;
  EXPECT_TRUE(out->GetAsList(&out_list));
  EXPECT_EQ(2U, out_list->GetSize());

  std::string out1;
  EXPECT_TRUE(out_list->GetString(0U, &out1));
  EXPECT_EQ(kTestDisabledScheme + std::string("://*"), out1);

  std::string out2;
  EXPECT_TRUE(out_list->GetString(1U, &out2));
  EXPECT_EQ(kTestBlacklistValue, out2);
}

}  // namespace policy
