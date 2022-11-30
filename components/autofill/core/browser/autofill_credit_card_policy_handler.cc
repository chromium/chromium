// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_credit_card_policy_handler.h"

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace autofill {

AutofillCreditCardPolicyHandler::AutofillCreditCardPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kAutofillCreditCardEnabled,
                                        base::Value::Type::BOOLEAN) {}

AutofillCreditCardPolicyHandler::~AutofillCreditCardPolicyHandler() {}

void AutofillCreditCardPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value && !value->GetBool())
    prefs->SetBoolean(prefs::kAutofillCreditCardEnabled, false);
}

}  // namespace autofill
