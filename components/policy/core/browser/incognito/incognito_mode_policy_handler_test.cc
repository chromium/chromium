// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/incognito/incognito_mode_policy_handler_test.h"

#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

IncognitoModePolicyHandlerTestBase::IncognitoModePolicyHandlerTestBase() {
  default_blocklist_.Append("blockedUrl.com");
  default_allowlist_.Append("allowedUrl.com");
}

void IncognitoModePolicyHandlerTestBase::SetIncognitoModeAvailability(
    policy::IncognitoModeAvailability availability) {
  policies_.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(availability)), nullptr);
}

void IncognitoModePolicyHandlerTestBase::SetIncognitoModeUrlAllowlist(
    base::ListValue allowlist) {
  policies_.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                base::Value(std::move(allowlist)), nullptr);
}

void IncognitoModePolicyHandlerTestBase::SetIncognitoModeUrlBlocklist(
    base::ListValue blocklist) {
  policies_.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                base::Value(std::move(blocklist)), nullptr);
}

void IncognitoModePolicyHandlerTestBase::ApplyPolicies() {
  UpdateProviderPolicy(policies_);
}

void IncognitoModePolicyHandlerTestBase::VerifyAvailabilityPref(
    policy::IncognitoModeAvailability availability) {
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(policy::policy_prefs::kIncognitoModeAvailability,
                               &value));
  EXPECT_EQ(base::Value(static_cast<int>(availability)), *value);
}

void IncognitoModePolicyHandlerTestBase::VerifyBlocklistPref(
    const base::ListValue& expected_blocklist) {
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(policy::policy_prefs::kIncognitoModeUrlBlocklist,
                               &value));
  EXPECT_EQ(expected_blocklist, *value);
}

void IncognitoModePolicyHandlerTestBase::VerifyAllowlistPref(
    const base::ListValue& expected_allowlist) {
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(policy::policy_prefs::kIncognitoModeUrlAllowlist,
                               &value));
  EXPECT_EQ(expected_allowlist, *value);
}

}  // namespace policy
