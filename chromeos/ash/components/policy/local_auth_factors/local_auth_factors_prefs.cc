// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy::local_auth_factors {

constexpr char kFactorsOptionAll[] = "ALL";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      ash::prefs::kLocalAuthFactorsComplexity,
      static_cast<int>(ash::LocalAuthFactorsComplexity::kNone));

  registry->RegisterIntegerPref(
      ash::prefs::kLocalPasswordVerifiedComplexity,
      static_cast<int>(ash::LocalAuthFactorsComplexity::kNone));

  registry->RegisterIntegerPref(
      ash::prefs::kLocalPinVerifiedComplexity,
      static_cast<int>(ash::LocalAuthFactorsComplexity::kNone));

  registry->RegisterListPref(ash::prefs::kAllowedLocalAuthFactors,
                             base::ListValue().Append(kFactorsOptionAll));
}

}  // namespace policy::local_auth_factors
