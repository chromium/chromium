// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace performance_manager::user_tuning::prefs {

const char kHighEfficiencyModeEnabled[] =
    "performance_tuning.high_efficiency_mode.enabled";

const char kBatterySaverModeState[] =
    "performance_tuning.battery_saver_mode.state";

const char kTabDiscardingExceptions[] =
    "performance_tuning.tab_discarding.exceptions";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHighEfficiencyModeEnabled, false);
  registry->RegisterIntegerPref(
      kBatterySaverModeState,
      static_cast<int>(BatterySaverModeState::kDisabled));
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kTabDiscardingExceptions);
}

BatterySaverModeState GetCurrentBatterySaverModeState(
    PrefService* pref_service) {
  int state = pref_service->GetInteger(kBatterySaverModeState);
  if (state < static_cast<int>(BatterySaverModeState::kDisabled) ||
      state > static_cast<int>(BatterySaverModeState::kEnabled)) {
    int disabled_state = static_cast<int>(BatterySaverModeState::kDisabled);
    pref_service->SetInteger(kBatterySaverModeState, disabled_state);
    state = disabled_state;
  }

  return static_cast<BatterySaverModeState>(state);
}

}  // namespace performance_manager::user_tuning::prefs