// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "auth_policy_utils.h"

#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// Maps auth factors policy values to their corresponding `AshAuthFactor` enum.
// LINT.IfChange(LocalAuthFactorsPolicyMap)
constexpr auto kAuthFactorPolicyMap =
    base::MakeFixedFlatMap<std::string_view, AuthFactorsSet>(
        {{"ALL",
          {AshAuthFactor::kLocalPassword, AshAuthFactor::kCryptohomePin}},
         {"LOCAL_PASSWORD", {AshAuthFactor::kLocalPassword}},
         {"PIN", {AshAuthFactor::kCryptohomePin}}});
// LINT.ThenChange(//components/policy/resources/templates/policy_definitions/Signin/LocalAuthFactors.yaml:LocalAuthFactorsPolicySchema)

std::optional<AuthFactorsSet> GetAuthFactorsSetFromPolicyList(
    const base::Value::List* policy_allowed_auth_factors) {
  AuthFactorsSet result;
  if (policy_allowed_auth_factors == nullptr) {
    return std::nullopt;
  }
  for (auto& auth_factor : *policy_allowed_auth_factors) {
    auto auth_factor_lookup =
        kAuthFactorPolicyMap.find(auth_factor.GetString());
    if (auth_factor_lookup != kAuthFactorPolicyMap.end()) {
      result.PutAll(auth_factor_lookup->second);
    }
  }
  return result;
}

}  // namespace ash
