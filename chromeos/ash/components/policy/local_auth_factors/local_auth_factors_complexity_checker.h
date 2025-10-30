// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_CHECKER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_CHECKER_H_

#include <string_view>

#include "base/component_export.h"

class PrefRegistrySimple;

namespace policy {

// Maps the values of the same-named policy located in
// components/policy/resources/templates/policy_definitions/Signin/LocalAuthFactorsComplexity.yaml.
enum class LocalAuthFactorsComplexity {
  kNone = 1,
  kLow,
  kMedium,
  kHigh,
};

// Used to check local auth factors (pin/password) complexity according to the
// `LocalAuthFactorsComplexity` policy.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    LocalAuthFactorsComplexityChecker {
 public:
  LocalAuthFactorsComplexityChecker() = delete;

  // Checks the complexity of the given password according to the policy and
  // returns true if the password passes the complexity check and false
  // otherwise.
  static bool CheckPasswordComplexity(std::string_view password,
                                      LocalAuthFactorsComplexity complexity);

  // Checks the complexity of the given pin according to the policy and returns
  // true if the pin passes the complexity check and false otherwise.
  static bool CheckPinComplexity(std::string_view pin,
                                 LocalAuthFactorsComplexity complexity);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_COMPLEXITY_CHECKER_H_
