// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include "base/containers/contains.h"
#include "components/performance_manager/public/features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace performance_manager::user_tuning::prefs {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kMemorySaverModeEnabled, false);
  registry->RegisterIntegerPref(
      kMemorySaverModeTimeBeforeDiscardInMinutes,
      kDefaultMemorySaverModeTimeBeforeDiscardInMinutes);
  registry->RegisterIntegerPref(
      kMemorySaverModeState, static_cast<int>(MemorySaverModeState::kDisabled));
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

MemorySaverModeState GetCurrentMemorySaverModeState(PrefService* pref_service) {
  int state = pref_service->GetInteger(kMemorySaverModeState);
  if (state < static_cast<int>(MemorySaverModeState::kDisabled) ||
      state > static_cast<int>(MemorySaverModeState::kEnabledOnTimer)) {
    int disabled_state = static_cast<int>(MemorySaverModeState::kDisabled);
    pref_service->SetInteger(kMemorySaverModeState, disabled_state);
    state = disabled_state;
  }

  return static_cast<MemorySaverModeState>(state);
}

base::TimeDelta GetCurrentMemorySaverModeTimeBeforeDiscard(
    PrefService* pref_service) {
  int time_before_discard_in_minutes =
      pref_service->GetInteger(kMemorySaverModeTimeBeforeDiscardInMinutes);
  if (time_before_discard_in_minutes < 0) {
    pref_service->ClearPref(kMemorySaverModeTimeBeforeDiscardInMinutes);
    time_before_discard_in_minutes =
        pref_service->GetInteger(kMemorySaverModeTimeBeforeDiscardInMinutes);
  }

  return base::Minutes(time_before_discard_in_minutes);
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

void MigrateMemorySaverModePref(PrefService* pref_service) {
  const PrefService::Preference* state_pref =
      pref_service->FindPreference(kMemorySaverModeState);
  if (!state_pref->IsDefaultValue()) {
    // The user has changed the new pref, no migration needed. Clear the old
    // pref because it won't be used anymore.
    pref_service->ClearPref(kMemorySaverModeEnabled);
    return;
  }

  const PrefService::Preference* bool_pref =
      pref_service->FindPreference(kMemorySaverModeEnabled);

  bool enabled = bool_pref->GetValue()->GetBool();
  int equivalent_int_pref =
      enabled ? static_cast<int>(MemorySaverModeState::kEnabledOnTimer)
              : static_cast<int>(MemorySaverModeState::kDisabled);
  if (!bool_pref->IsDefaultValue()) {
    // The user has changed the old pref, but the new pref is still set to the
    // default value. This means the old pref's state needs to be migrated into
    // the new pref.
    pref_service->SetInteger(kMemorySaverModeState, equivalent_int_pref);
    // Clear the old pref because it won't be used anymore.
    pref_service->ClearPref(kMemorySaverModeEnabled);
  }
}

bool IsSiteInTabDiscardExceptionsList(PrefService* pref_service,
                                      const std::string& site) {
  const base::Value::List& discard_exception_list =
      pref_service->GetList(kTabDiscardingExceptions);
  return base::Contains(discard_exception_list, site);
}

void AddSiteToTabDiscardExceptionsList(PrefService* pref_service,
                                       const std::string& site) {
  base::Value::List discard_exception_list =
      pref_service->GetList(kTabDiscardingExceptions).Clone();
  if (!base::Contains(discard_exception_list, site)) {
    discard_exception_list.Append(site);
    pref_service->SetList(kTabDiscardingExceptions,
                          std::move(discard_exception_list));
  }
}

void ClearTabDiscardExceptionsList(PrefService* pref_service) {
  pref_service->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions, {});
}

}  // namespace performance_manager::user_tuning::prefs
