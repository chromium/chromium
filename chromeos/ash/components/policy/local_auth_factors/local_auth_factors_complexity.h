// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_H_

#include <string_view>

#include "base/component_export.h"

namespace policy::local_auth_factors {

// Maps the values of the same-named policy located in
// components/policy/resources/templates/policy_definitions/Signin/LocalAuthFactorsComplexity.yaml.
enum class Complexity {
  kNone = 1,
  kLow,
  kMedium,
  kHigh,
};

// Checks the complexity of the given password according to the
// `LocalAuthFactorsComplexity` policy and returns true if the password passes
// the complexity check and false otherwise.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool CheckPasswordComplexity(std::string_view password, Complexity complexity);

// Checks the complexity of the given pin according to the
// `LocalAuthFactorsComplexity` policy and returns true if the pin passes the
// complexity check and false otherwise.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool CheckPinComplexity(std::string_view pin, Complexity complexity);

}  // namespace policy::local_auth_factors

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_H_
