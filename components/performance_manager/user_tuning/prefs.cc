// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include "components/performance_manager/public/features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace performance_manager::user_tuning::prefs {

const char kHighEfficiencyModeEnabled[] =
    "performance_tuning.high_efficiency_mode.enabled";

const char kHighEfficiencyModeState[] =
    "performance_tuning.high_efficiency_mode.state";

const char kBatterySaverModeState[] =
    "performance_tuning.battery_saver_mode.state";

const char kLastBatteryUseTimestamp[] =
    "performance_tuning.last_battery_use.timestamp";

const char kTabDiscardingExceptions[] =
    "performance_tuning.tab_discarding.exceptions";

const char kManagedTabDiscardingExceptions[] =
    "performance_tuning.tab_discarding.exceptions_managed";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHighEfficiencyModeEnabled, false);
  registry->RegisterIntegerPref(
      kHighEfficiencyModeState,
      static_cast<int>(HighEfficiencyModeState::kDisabled));
  registry->RegisterIntegerPref(
      kBatterySaverModeState,
      static_cast<int>(BatterySaverModeState::kEnabledBelowThreshold));
  registry->RegisterTimePref(kLastBatteryUseTimestamp, base::Time());
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kTabDiscardingExceptions,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(kManagedTabDiscardingExceptions);
}

HighEfficiencyModeState GetCurrentHighEfficiencyModeState(
    PrefService* pref_service) {
  int state = pref_service->GetInteger(kHighEfficiencyModeState);
  if (state < static_cast<int>(HighEfficiencyModeState::kDisabled) ||
      state > static_cast<int>(HighEfficiencyModeState::kEnabledOnTimer)) {
    int disabled_state = static_cast<int>(HighEfficiencyModeState::kDisabled);
    pref_service->SetInteger(kHighEfficiencyModeState, disabled_state);
    state = disabled_state;
  }

  return static_cast<HighEfficiencyModeState>(state);
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

void MigrateHighEfficiencyModePref(PrefService* pref_service) {
  const PrefService::Preference* state_pref =
      pref_service->FindPreference(kHighEfficiencyModeState);
  if (!state_pref->IsDefaultValue()) {
    // The user has changed the new pref, no migration needed. Clear the old
    // pref because it won't be used anymore.
    pref_service->ClearPref(kHighEfficiencyModeEnabled);
    return;
  }

  const PrefService::Preference* bool_pref =
      pref_service->FindPreference(kHighEfficiencyModeEnabled);

  bool enabled = bool_pref->GetValue()->GetBool();
  int equivalent_int_pref =
      enabled ? static_cast<int>(HighEfficiencyModeState::kEnabledOnTimer)
              : static_cast<int>(HighEfficiencyModeState::kDisabled);
  if (!bool_pref->IsDefaultValue()) {
    // The user has changed the old pref, but the new pref is still set to the
    // default value. This means the old pref's state needs to be migrated into
    // the new pref.
    pref_service->SetInteger(kHighEfficiencyModeState, equivalent_int_pref);
    // Clear the old pref because it won't be used anymore.
    pref_service->ClearPref(kHighEfficiencyModeEnabled);
  }
}

}  // namespace performance_manager::user_tuning::prefs
