// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_policy_handler.h"

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace autofill {

AutofillPolicyHandler::AutofillPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kAutoFillEnabled,
                                        base::Value::Type::BOOLEAN) {}

AutofillPolicyHandler::~AutofillPolicyHandler() = default;

void AutofillPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* autofill_credit_card_policy_value = policies.GetValue(
      policy::key::kAutofillCreditCardEnabled, base::Value::Type::BOOLEAN);
  const base::Value* autofill_address_policy_value = policies.GetValue(
      policy::key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN);
  // Ignore the old policy if either of the new fine-grained policies are set.
  if (autofill_credit_card_policy_value || autofill_address_policy_value) {
    return;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value && !value->GetBool()) {
    prefs->SetBoolean(prefs::kAutofillEnabledDeprecated, false);
    // Disable the fine-grained prefs if the main pref is disabled by policy.
    prefs->SetBoolean(prefs::kAutofillCreditCardEnabled, false);
    prefs->SetBoolean(prefs::kAutofillProfileEnabled, false);
  }
}

}  // namespace autofill
