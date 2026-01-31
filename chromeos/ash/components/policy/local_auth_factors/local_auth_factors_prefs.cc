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
  auto local_auth_factors_default = base::ListValue().Append(kFactorsOptionAll);

  registry->RegisterIntegerPref(
      ash::prefs::kLocalAuthFactorsComplexity,
      static_cast<int>(ash::LocalAuthFactorsComplexity::kNone));
  registry->RegisterListPref(ash::prefs::kAllowedLocalAuthFactors,
                             std::move(local_auth_factors_default));
}

}  // namespace policy::local_auth_factors
