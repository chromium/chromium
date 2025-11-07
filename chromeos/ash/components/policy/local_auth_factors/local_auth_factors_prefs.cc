// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy::local_auth_factors {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(ash::prefs::kLocalAuthFactorsComplexity,
                                static_cast<int>(Complexity::kNone));
}

}  // namespace policy::local_auth_factors
