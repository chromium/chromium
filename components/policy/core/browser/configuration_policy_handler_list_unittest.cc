// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler_list.h"

#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kPolicyName[] = "PolicyName";
const char kPolicyName2[] = "PolicyName2";
const int kPolicyValue = 12;

class StubPolicyHandler : public ConfigurationPolicyHandler {
 public:
  explicit StubPolicyHandler(const std::string& policy_name)
      : policy_name_(policy_name) {}
  ~StubPolicyHandler() override = default;

  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override {
    // It's safe to use `GetValueUnsafe()` as multiple policy types are handled.
    return policies.GetValueUnsafe(policy_name_);
  }

 protected:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {
    prefs->SetInteger(
        policy_name_,
        policies.GetValue(policy_name_, base::Value::Type::INTEGER)->GetInt());
  }

 private:
  std::string policy_name_;
};

}  // namespace

class ConfigurationPolicyHandlerListTest : public ::testing::Test {
 public:
  void SetUp() override { CreateHandlerList(); }

  void AddSimplePolicy() {
    AddPolicy(kPolicyName, /* is_cloud */ true, base::Value(kPolicyValue));
  }

  void AddPolicy(const std::string policy_name,
                 bool is_cloud,
                 base::Value value) {
    policies_.Set(policy_name, PolicyLevel::POLICY_LEVEL_MANDATORY,
                  PolicyScope::POLICY_SCOPE_MACHINE,
                  is_cloud ? PolicySource::POLICY_SOURCE_CLOUD
                           : PolicySource::POLICY_SOURCE_PLATFORM,
                  std::move(value), nullptr);
    if (policy_name != key::kEnableExperimentalPolicies) {
      handler_list_->AddHandler(
          std::make_unique<StubPolicyHandler>(policy_name));
    }
  }

  void ApplySettings() {
    handler_list_->ApplyPolicySettings(
        policies_, &prefs_, &errors_, &deprecated_policies_, &future_policies_);
  }

  void CreateHandlerList(bool are_future_policies_allowed_by_default = false) {
    handler_list_ = std::make_unique<ConfigurationPolicyHandlerList>(
        ConfigurationPolicyHandlerList::
            PopulatePolicyHandlerParametersCallback(),
        base::BindRepeating(
            &ConfigurationPolicyHandlerListTest::GetPolicyDetails,
            base::Unretained(this)),
        are_future_policies_allowed_by_default);
  }

  PrefValueMap* prefs() { return &prefs_; }

  const PolicyDetails* GetPolicyDetails(const std::string& policy_name) {
    return &details_;
  }
  PolicyDetails* details() { return &details_; }

  void VerifyPolicyAndPref(const std::string& policy_name,
                           bool in_pref,
                           bool in_deprecated = false,
                           bool in_future = false) {
    int pref_value;

    ASSERT_EQ(in_pref, prefs_.GetInteger(policy_name, &pref_value));
    if (in_pref)
      EXPECT_EQ(kPolicyValue, pref_value);

    // Pref filter never affects PolicyMap.
    const base::Value* policy_value =
        policies_.GetValue(policy_name, base::Value::Type::INTEGER);
    ASSERT_TRUE(policy_value);
    EXPECT_EQ(kPolicyValue, policy_value->GetInt());

    EXPECT_EQ(in_deprecated, deprecated_policies_.find(policy_name) !=
                                 deprecated_policies_.end());
    EXPECT_EQ(in_future,
              future_policies_.find(policy_name) != future_policies_.end());
  }

 private:
  PrefValueMap prefs_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PoliciesSet deprecated_policies_;
  PoliciesSet future_policies_;
  PolicyDetails details_{false, false, kProfile, 0, 0, {}};

  std::unique_ptr<ConfigurationPolicyHandlerList> handler_list_;
};

TEST_F(ConfigurationPolicyHandlerListTest, ApplySettingsWithNormalPolicy) {
  AddSimplePolicy();
  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /* in_pref */ true);
}

// Future policy will be filter out unless it's whitelisted by
// kEnableExperimentalPolicies.
TEST_F(ConfigurationPolicyHandlerListTest, ApplySettingsWithFuturePolicy) {
  AddSimplePolicy();
  details()->is_future = true;

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /* in_pref */ false,
                      /* in_deprecated */ false, /* in_future */ true);

  // Whitelist a different policy.
  base::Value::List enabled_future_policies;
  enabled_future_policies.Append(kPolicyName2);
  AddPolicy(key::kEnableExperimentalPolicies, /* is_cloud */ true,
            base::Value(enabled_future_policies.Clone()));

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /* in_pref */ false,
                      /* in_deprecated */ false, /* in_future */ true);

  // Whitelist the policy.
  enabled_future_policies.Append(base::Value(kPolicyName));
  AddPolicy(key::kEnableExperimentalPolicies, /* is_cloud */ true,
            base::Value(std::move(enabled_future_policies)));

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /* in_pref */ true);
}

TEST_F(ConfigurationPolicyHandlerListTest,
       ApplySettingsWithoutFutureFilterPolicy) {
  CreateHandlerList(true);
  AddSimplePolicy();
  details()->is_future = true;

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /* in_pref */ true);
}

// Device platform policy will be fitered out.
TEST_F(ConfigurationPolicyHandlerListTest,
       ApplySettingsWithPlatformDevicePolicy) {
  AddSimplePolicy();
  details()->scope = kDevice;

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /* in_pref */ true);

  AddPolicy(kPolicyName2, /* is_cloud */ false, base::Value(kPolicyValue));

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName2, /* in_pref */ false);
}

// Deprecated policy won't be filtered out.
TEST_F(ConfigurationPolicyHandlerListTest, ApplySettingsWithDeprecatedPolicy) {
  AddSimplePolicy();
  details()->is_deprecated = true;

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /* in_pref */ true, /* in_deprecated*/ true);
}

}  // namespace policy
