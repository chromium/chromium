// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_H_

#include <string_view>

#include "base/component_export.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace policy::local_auth_factors {

// LINT.IfChange(PasswordComplexityResult)
enum class PasswordComplexityResult {
  kOk,
  kTooShort,
  kMissesCharacters,
  kContainsTrivialSequence,
};
// LINT.ThenChange(//chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom:PasswordComplexity)

// Checks the complexity of the given password according to the
// `LocalAuthFactorsComplexity` policy and returns the result of the complexity
// check.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
PasswordComplexityResult CheckPasswordComplexity(
    std::string_view password,
    ash::LocalAuthFactorsComplexity complexity);

// Checks the complexity of the given pin according to the
// `LocalAuthFactorsComplexity` policy and returns true if the pin passes the
// complexity check and false otherwise.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool CheckPinComplexity(std::string_view pin,
                        ash::LocalAuthFactorsComplexity complexity);

}  // namespace policy::local_auth_factors

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_H_
