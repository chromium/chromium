// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_

#include "base/timer/timer.h"

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace performance_manager::user_tuning::prefs {

// DEPRECATED: being replaced by kHighEfficiencyModeState
extern const char kHighEfficiencyModeEnabled[];

enum class HighEfficiencyModeState {
  kDisabled = 0,
  kEnabled = 1,
  kEnabledOnTimer = 2,
};

extern const char kHighEfficiencyModeState[];

extern const char kHighEfficiencyModeTimeBeforeDiscardInMinutes[];

extern const int kDefaultHighEfficiencyModeTimeBeforeDiscardInMinutes;

enum class BatterySaverModeState {
  kDisabled = 0,
  kEnabledBelowThreshold = 1,
  kEnabledOnBattery = 2,
  kEnabled = 3,
};

extern const char kBatterySaverModeState[];

// Stores the timestamp of the last battery usage while unplugged.
extern const char kLastBatteryUseTimestamp[];

// The pref storing the list of URL patterns that prevent a tab from being
// discarded.
extern const char kTabDiscardingExceptions[];

// The pref storing the enterprise-managed list of URL patterns that prevent a
// tab from being discarded. This list is merged with
// `kTabDiscardingExceptions`.
extern const char kManagedTabDiscardingExceptions[];

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

HighEfficiencyModeState GetCurrentHighEfficiencyModeState(
    PrefService* pref_service);

base::TimeDelta GetCurrentHighEfficiencyModeTimeBeforeDiscard(
    PrefService* pref_service);

BatterySaverModeState GetCurrentBatterySaverModeState(
    PrefService* pref_service);

// This function migrates the old, boolean High Efficiency (Memory Saver)
// preference to the new, integer one that represents a value of the
// `HighEfficiencyModeState` enum. This is done once at startup.
void MigrateHighEfficiencyModePref(PrefService* pref_service);

}  // namespace performance_manager::user_tuning::prefs

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PREFS_H_
