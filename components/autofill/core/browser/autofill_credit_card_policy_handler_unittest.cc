// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_credit_card_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Test cases for the Autofill credit card policy setting.
class AutofillCreditCardPolicyHandlerTest : public testing::Test {};

TEST_F(AutofillCreditCardPolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  PrefValueMap prefs;
  AutofillCreditCardPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, nullptr));
}

TEST_F(AutofillCreditCardPolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillCreditCardEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  PrefValueMap prefs;
  AutofillCreditCardPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Autofill for credit cards should not set the prefs.
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, nullptr));
}

TEST_F(AutofillCreditCardPolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillCreditCardEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillCreditCardPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Disabling Autofill for credit cards should switch the prefs to managed.
  const base::Value* value = nullptr;
  EXPECT_TRUE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, &value));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_FALSE(value->GetBool());
}

}  // namespace autofill
