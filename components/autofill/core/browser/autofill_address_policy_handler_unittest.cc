// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_address_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Test cases for the Autofill address policy setting.
class AutofillAddressPolicyHandlerTest : public testing::Test {};

TEST_F(AutofillAddressPolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  PrefValueMap prefs;
  AutofillAddressPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, nullptr));
}

TEST_F(AutofillAddressPolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  PrefValueMap prefs;
  AutofillAddressPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Autofill for profiles should not set the prefs.
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, nullptr));
}

TEST_F(AutofillAddressPolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillAddressPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Disabling Autofill for profiles should switch the prefs to managed.
  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, &value));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_FALSE(value->GetBool());
}

}  // namespace autofill
