// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/values.h"
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
      kMemorySaverModeAggressiveness,
      static_cast<int>(MemorySaverModeAggressiveness::kMedium));
  registry->RegisterIntegerPref(
      kBatterySaverModeState,
      static_cast<int>(BatterySaverModeState::kEnabledBelowThreshold));
  registry->RegisterTimePref(kLastBatteryUseTimestamp, base::Time());
  registry->RegisterBooleanPref(kDiscardRingTreatmentEnabled, true);
  registry->RegisterBooleanPref(kPerformanceInterventionNotificationEnabled,
                                true);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kTabDiscardingExceptions,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kTabDiscardingExceptionsWithTime,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(kManagedTabDiscardingExceptions);
}

MemorySaverModeState GetCurrentMemorySaverModeState(PrefService* pref_service) {
  int state = pref_service->GetInteger(kMemorySaverModeState);
  if (state < static_cast<int>(MemorySaverModeState::kDisabled) ||
      state > static_cast<int>(MemorySaverModeState::kEnabled)) {
    int disabled_state = static_cast<int>(MemorySaverModeState::kDisabled);
    pref_service->SetInteger(kMemorySaverModeState, disabled_state);
    state = disabled_state;
  }

  return static_cast<MemorySaverModeState>(state);
}

MemorySaverModeAggressiveness GetCurrentMemorySaverMode(
    PrefService* pref_service) {
  int mode = pref_service->GetInteger(kMemorySaverModeAggressiveness);
  if (mode < static_cast<int>(MemorySaverModeAggressiveness::kConservative) ||
      mode > static_cast<int>(MemorySaverModeAggressiveness::kAggressive)) {
    int medium_mode = static_cast<int>(MemorySaverModeAggressiveness::kMedium);
    pref_service->SetInteger(kMemorySaverModeAggressiveness, medium_mode);
    mode = medium_mode;
  }

  return static_cast<MemorySaverModeAggressiveness>(mode);
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

bool ShouldShowDiscardRingTreatment(PrefService* pref_service) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return pref_service->GetBoolean(kDiscardRingTreatmentEnabled);
#endif
}

bool ShouldShowPerformanceInterventionNotification(PrefService* pref_service) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return pref_service->GetBoolean(kPerformanceInterventionNotificationEnabled);
#endif
}

void MigrateMemorySaverModePref(PrefService* pref_service) {
  const PrefService::Preference* state_pref =
      pref_service->FindPreference(kMemorySaverModeState);
  if (!state_pref->IsDefaultValue()) {
    // The user has changed the new pref, no migration needed. Clear the old
    // pref because it won't be used anymore. Note that this case should not
    // occur.
    pref_service->ClearPref(kMemorySaverModeEnabled);
    return;
  }

  const PrefService::Preference* bool_pref =
      pref_service->FindPreference(kMemorySaverModeEnabled);

  bool enabled = bool_pref->GetValue()->GetBool();
  int equivalent_int_pref =
      enabled ? static_cast<int>(MemorySaverModeState::kEnabled)
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

void MigrateMultiStateMemorySaverModePref(PrefService* pref_service) {
  const PrefService::Preference* state_pref =
      pref_service->FindPreference(kMemorySaverModeState);
  if (!state_pref->IsDefaultValue() &&
      static_cast<MemorySaverModeState>(state_pref->GetValue()->GetInt()) ==
          MemorySaverModeState::kDeprecated) {
    pref_service->SetInteger(kMemorySaverModeState,
                             static_cast<int>(MemorySaverModeState::kEnabled));
  }

  pref_service->ClearPref(kMemorySaverModeTimeBeforeDiscardInMinutes);
}

void MigrateTabDiscardingExceptionsPref(PrefService* pref_service) {
  const PrefService::Preference* exceptions_with_time_pref =
      pref_service->FindPreference(kTabDiscardingExceptionsWithTime);
  if (!exceptions_with_time_pref->IsDefaultValue()) {
    // The user has changed the new pref, no migration needed. Clear the old
    // pref because it won't be used anymore.
    pref_service->ClearPref(kTabDiscardingExceptions);
    return;
  }

  const PrefService::Preference* exceptions_pref =
      pref_service->FindPreference(kTabDiscardingExceptions);

  if (exceptions_pref->IsDefaultValue()) {
    // The old pref is the default value so no migration is needed.
    return;
  }

  // The user has changed the old pref, but the new pref is still set to the
  // default value. This means the old pref's state needs to be migrated into
  // the new pref.
  std::vector<std::pair<std::string, base::Value>> migrated_exceptions;
  migrated_exceptions.reserve(exceptions_pref->GetValue()->GetList().size());

  for (const base::Value& value : exceptions_pref->GetValue()->GetList()) {
    CHECK(value.is_string());
    // Set the timestamp to now when performing the migration. When these prefs
    // are cleared, it is based on a time window that ends at the present time
    // and goes back some number of hours. Setting the time to the time of
    // migration ensures that every entry that was migrated during the last N
    // hours will be cleared properly and as time passes the number of entries
    // that will be cleared despite the last edit being before the time window
    // will decreased.
    migrated_exceptions.emplace_back(value.GetString(),
                                     base::TimeToValue(base::Time::Now()));
  }
  pref_service->SetDict(
      kTabDiscardingExceptionsWithTime,
      base::Value::Dict(std::make_move_iterator(migrated_exceptions.begin()),
                        std::make_move_iterator(migrated_exceptions.end())));
  // Clear the old pref because it won't be used anymore.
  pref_service->ClearPref(kTabDiscardingExceptions);
}

bool IsSiteInTabDiscardExceptionsList(PrefService* pref_service,
                                      const std::string& site) {
  const base::Value::Dict& discard_exceptions_map =
      pref_service->GetDict(kTabDiscardingExceptionsWithTime);
  return base::Contains(discard_exceptions_map, site);
}

void AddSiteToTabDiscardExceptionsList(PrefService* pref_service,
                                       const std::string& site) {
  const base::Value::Dict& discard_exceptions_original =
      pref_service->GetDict(kTabDiscardingExceptionsWithTime);
  if (!base::Contains(discard_exceptions_original, site)) {
    base::Value::Dict discard_exceptions_map =
        discard_exceptions_original.Clone();
    discard_exceptions_map.Set(site, base::TimeToValue(base::Time::Now()));
    pref_service->SetDict(kTabDiscardingExceptionsWithTime,
                          std::move(discard_exceptions_map));
  }
}

std::vector<std::string> GetTabDiscardExceptionsBetween(
    PrefService* pref_service,
    base::Time period_start,
    base::Time period_end) {
  std::vector<std::string> discard_exceptions;

  const base::Value::Dict& discard_exceptions_map =
      pref_service->GetDict(performance_manager::user_tuning::prefs::
                                kTabDiscardingExceptionsWithTime);
  for (const std::pair<const std::string&, const base::Value&> it :
       discard_exceptions_map) {
    std::optional<base::Time> time = base::ValueToTime(it.second);
    if ((!time || (time > period_start && time < period_end))) {
      discard_exceptions.push_back(it.first);
    }
  }

  return discard_exceptions;
}

void ClearTabDiscardExceptions(PrefService* pref_service,
                               base::Time delete_begin,
                               base::Time delete_end) {
  const base::Value::Dict& discard_exceptions_map =
      pref_service->GetDict(kTabDiscardingExceptionsWithTime);

  std::vector<std::pair<std::string, base::Value>> saved_exceptions;
  saved_exceptions.reserve(discard_exceptions_map.size());

  for (std::pair<const std::string&, const base::Value&> it :
       discard_exceptions_map) {
    std::optional<base::Time> time = base::ValueToTime(it.second);
    if (time && (time.value() < delete_begin || time.value() > delete_end)) {
      saved_exceptions.emplace_back(it.first, it.second.Clone());
    }
  }
  pref_service->SetDict(
      kTabDiscardingExceptionsWithTime,
      base::Value::Dict(std::make_move_iterator(saved_exceptions.begin()),
                        std::make_move_iterator(saved_exceptions.end())));
}

}  // namespace performance_manager::user_tuning::prefs
