// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/incognito/incognito_mode_policy_handler.h"

#include "base/memory/ptr_util.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/incognito/incognito_mode_policy_handler_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

class IncognitoModePolicyHandlerTest
    : public IncognitoModePolicyHandlerTestBase {
 public:
  void SetUp() override {
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new IncognitoModePolicyHandler));
  }
};

TEST_F(IncognitoModePolicyHandlerTest, NoPolicySet) {
  ApplyPolicies();

  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeAvailability, &value));
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlBlocklist, &value));
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlAllowlist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AvailabilityDisabled) {
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kDisabled);
}

TEST_F(IncognitoModePolicyHandlerTest, BlocklistSetAndAvailabilityDisabled) {
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kDisabled);
}

// Checks that if only the allowlist is set, the blocklist is set to "*".
TEST_F(IncognitoModePolicyHandlerTest, AllowlistSet) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  ApplyPolicies();
  VerifyAllowlistPref(default_allowlist_);
  VerifyBlocklistPref(base::ListValue().Append("*"));
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistSetWithBlocklistSet) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  ApplyPolicies();
  VerifyBlocklistPref(default_blocklist_);
  VerifyAllowlistPref(default_allowlist_);
}

// Checks that if allowlist is set and availability is disabled, the Incognito
// mode is enabled and blocklist is set to "*".
TEST_F(IncognitoModePolicyHandlerTest, AllowlistSetWithAvailabilityDisabled) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
}

// Checks that if allowlist is set, blocklist is set and availability is
// disabled, then Incognito mode is enabled and the blocklist is set to "*".
TEST_F(IncognitoModePolicyHandlerTest,
       AllowlistSetWithBlocklistAndAvailabilityDisabled) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyAllowlistPref(default_allowlist_);
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistSetWithAvailabilityForced) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kForced);
  ApplyPolicies();
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kForced);
}

// Checks that if allowlist is set, blocklist is set and availability is
// forced, then Incognito mode is forced and the blocklist is not changed.
TEST_F(IncognitoModePolicyHandlerTest,
       AllowlistSetWithBlocklistAndAvailabilityForced) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kForced);
  ApplyPolicies();
  VerifyBlocklistPref(default_blocklist_);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kForced);
}

TEST_F(IncognitoModePolicyHandlerTest, AvailabilityInvalidType) {
  policies_.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value("invalid"),
                nullptr);
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeAvailability, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AvailabilityOutOfRange) {
  policies_.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(
                    policy::IncognitoModeAvailability::kNumTypes)),
                nullptr);
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeAvailability, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistInvalidType) {
  policies_.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(123),
                nullptr);
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlAllowlist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, BlocklistInvalidType) {
  policies_.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(123),
                nullptr);
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlBlocklist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, CheckPolicySettingsErrors) {
  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;

  // Invalid availability type
  PolicyMap policies;
  policies.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value("invalid"),
               nullptr);
  EXPECT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kIncognitoModeAvailability));

  // Out of range availability
  errors.Clear();
  policies.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   policy::IncognitoModeAvailability::kNumTypes)),
               nullptr);
  EXPECT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kIncognitoModeAvailability));

  // Invalid allowlist
  errors.Clear();
  policies.Clear();
  policies.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(123),
               nullptr);
  EXPECT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kIncognitoModeUrlAllowlist));

  // Invalid blocklist
  errors.Clear();
  policies.Clear();
  policies.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(123),
               nullptr);
  EXPECT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kIncognitoModeUrlBlocklist));
}

}  // namespace policy
