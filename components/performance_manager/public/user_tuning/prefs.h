// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/timer/timer.h"

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace performance_manager::user_tuning::prefs {

// DEPRECATED: being replaced by kMemorySaverModeState
inline constexpr char kMemorySaverModeEnabled[] =
    "performance_tuning.high_efficiency_mode.enabled";

enum class MemorySaverModeState {
  kDisabled = 0,
  kEnabled = 1,
  kEnabledOnTimer = 2,
};

inline constexpr char kMemorySaverModeState[] =
    "performance_tuning.high_efficiency_mode.state";

inline constexpr char kMemorySaverModeTimeBeforeDiscardInMinutes[] =
    "performance_tuning.high_efficiency_mode.time_before_discard_in_minutes";

constexpr int kDefaultMemorySaverModeTimeBeforeDiscardInMinutes = 120;

enum class BatterySaverModeState {
  kDisabled = 0,
  kEnabledBelowThreshold = 1,
  kEnabledOnBattery = 2,
  kEnabled = 3,
};

inline constexpr char kBatterySaverModeState[] =
    "performance_tuning.battery_saver_mode.state";

// Stores the timestamp of the last battery usage while unplugged.
inline constexpr char kLastBatteryUseTimestamp[] =
    "performance_tuning.last_battery_use.timestamp";

// The pref storing the list of URL patterns that prevent a tab from being
// discarded.
inline constexpr char kTabDiscardingExceptions[] =
    "performance_tuning.tab_discarding.exceptions";

// The pref storing the list of URL patterns that prevent a tab from being
// discarded.
inline constexpr char kTabDiscardingExceptionsWithTime[] =
    "performance_tuning.tab_discarding.exceptions_with_time";

// The pref storing the enterprise-managed list of URL patterns that prevent a
// tab from being discarded. This list is merged with
// `kTabDiscardingExceptions`.
inline constexpr char kManagedTabDiscardingExceptions[] =
    "performance_tuning.tab_discarding.exceptions_managed";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

MemorySaverModeState GetCurrentMemorySaverModeState(PrefService* pref_service);

base::TimeDelta GetCurrentMemorySaverModeTimeBeforeDiscard(
    PrefService* pref_service);

BatterySaverModeState GetCurrentBatterySaverModeState(
    PrefService* pref_service);

// This function migrates the old, boolean Memory Saver preference to the new,
// integer one that represents a value of the `MemorySaverModeState` enum. This
// is done once at startup.
void MigrateMemorySaverModePref(PrefService* pref_service);

// This function migrates the old, list tab discarding exceptions preference to
// the new, dictionary one that includes the time of the last edit of the
// preference. This is done once at startup.
void MigrateTabDiscardingExceptionsPref(PrefService* pref_service);

// Returns if the given site is in the discard exception list
bool IsSiteInTabDiscardExceptionsList(PrefService* pref_service,
                                      const std::string& site);

// Adds the given site to the discard exception list
void AddSiteToTabDiscardExceptionsList(PrefService* pref_service,
                                       const std::string& site);

// Returns a list of tab discard exception patterns during the time range.
std::vector<std::string> GetTabDiscardExceptionsBetween(
    PrefService* pref_service,
    base::Time period_start,
    base::Time period_end);

// Clears all discard exception prefs modified or created during the time range.
void ClearTabDiscardExceptions(PrefService* pref_service,
                               base::Time delete_begin,
                               base::Time delete_end);
}  // namespace performance_manager::user_tuning::prefs

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_
