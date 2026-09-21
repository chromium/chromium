// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_VALUE_VALIDATION_ONC_USER_POLICY_VALUE_VALIDATOR_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_VALUE_VALIDATION_ONC_USER_POLICY_VALUE_VALIDATOR_H_

#include "base/component_export.h"
#include "chromeos/ash/components/policy/value_validation/onc_policy_value_validator_base.h"
#include "components/policy/proto/cloud_policy.pb.h"

namespace policy {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    ONCUserPolicyValueValidator
    : public ONCPolicyValueValidatorBase<
          enterprise_management::CloudPolicySettings> {
 public:
  ONCUserPolicyValueValidator();

  ONCUserPolicyValueValidator(const ONCUserPolicyValueValidator&) = delete;
  ONCUserPolicyValueValidator& operator=(const ONCUserPolicyValueValidator&) =
      delete;

 protected:
  // ONCPolicyValueValidatorBase:
  std::optional<std::string> GetONCStringFromPayload(
      const enterprise_management::CloudPolicySettings& policy_payload)
      const override;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_VALUE_VALIDATION_ONC_USER_POLICY_VALUE_VALIDATOR_H_
