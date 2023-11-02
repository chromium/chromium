// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_POLICY_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace autofill {

// ConfigurationPolicyHandler for the AutofillAddressEnabled policy.
class AutofillAddressPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  AutofillAddressPolicyHandler();

  AutofillAddressPolicyHandler(const AutofillAddressPolicyHandler&) = delete;
  AutofillAddressPolicyHandler& operator=(const AutofillAddressPolicyHandler&) =
      delete;

  ~AutofillAddressPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_POLICY_HANDLER_H_
