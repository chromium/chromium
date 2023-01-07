// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CREDIT_CARD_POLICY_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CREDIT_CARD_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace autofill {

// ConfigurationPolicyHandler for the AutofillCreditCardEnabled policy.
class AutofillCreditCardPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  AutofillCreditCardPolicyHandler();

  AutofillCreditCardPolicyHandler(const AutofillCreditCardPolicyHandler&) =
      delete;
  AutofillCreditCardPolicyHandler& operator=(
      const AutofillCreditCardPolicyHandler&) = delete;

  ~AutofillCreditCardPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CREDIT_CARD_POLICY_HANDLER_H_
