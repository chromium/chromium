// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tpcd_pref_names.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/privacy_sandbox/tpcd_utils.h"

namespace tpcd::experiment {
namespace prefs {

// Local State Prefs
const char kTPCDExperimentClientState[] = "tpcd_experiment.client_state";
const char kTPCDExperimentClientStateVersion[] =
    "tpcd_experiment.client_state_version";

// Profile Prefs
const char kTPCDExperimentProfileState[] = "tpcd_experiment.profile_state";

}  // namespace prefs

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kTPCDExperimentClientState,
      static_cast<int>(utils::ExperimentState::kUnknownEligibility));
  registry->RegisterIntegerPref(prefs::kTPCDExperimentClientStateVersion, 0);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kTPCDExperimentProfileState,
      static_cast<int>(utils::ExperimentState::kUnknownEligibility));
}

}  // namespace tpcd::experiment
