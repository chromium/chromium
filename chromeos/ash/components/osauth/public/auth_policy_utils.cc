// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "auth_policy_utils.h"

#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/containers/fixed_flat_map.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// Maps auth factors policy values to their corresponding `AshAuthFactor` enum.
// LINT.IfChange(LocalAuthFactorsPolicyMap)
constexpr auto kAuthFactorPolicyMap =
    base::MakeFixedFlatMap<std::string_view, AuthFactorsSet>(
        {{"ALL",
          {AshAuthFactor::kLocalPassword, AshAuthFactor::kCryptohomePin}},
         {"LOCAL_PASSWORD", {AshAuthFactor::kLocalPassword}},
         {"PIN", {AshAuthFactor::kCryptohomePin}}});
// LINT.ThenChange(//components/policy/resources/templates/policy_definitions/Signin/AllowedLocalAuthFactors.yaml:LocalAuthFactorsPolicySchema)

}  // namespace

std::optional<AuthFactorsSet> GetAuthFactorsSetFromPolicyList(
    const base::ListValue* policy_allowed_auth_factors) {
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

bool IsPinEnabledAsMainFactorByPolicy(const PrefService* pref_service) {
  const base::ListValue* factors =
      &pref_service->GetList(prefs::kAllowedLocalAuthFactors);
  std::optional<AuthFactorsSet> policy_list =
      GetAuthFactorsSetFromPolicyList(factors);

  return policy_list.has_value() &&
         policy_list->Has(ash::AshAuthFactor::kCryptohomePin);
}

bool HasPinFactor(const base::ListValue* auth_factors) {
  return auth_factors->contains("PIN");
}

bool IsGaiaPassword(const cryptohome::AuthFactor& factor) {
  if (factor.ref().type() != cryptohome::AuthFactorType::kPassword) {
    return false;
  }

  const std::string& label = factor.ref().label().value();
  return label == kCryptohomeGaiaKeyLabel ||
         label.find(kCryptohomeGaiaKeyLegacyLabelPrefix) == 0;
}

bool IsLocalPassword(const cryptohome::AuthFactor& factor) {
  if (factor.ref().type() != cryptohome::AuthFactorType::kPassword) {
    return false;
  }

  const std::string& label = factor.ref().label().value();
  return label == kCryptohomeLocalPasswordKeyLabel;
}

}  // namespace ash
