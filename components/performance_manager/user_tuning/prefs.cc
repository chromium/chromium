// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace performance_manager::user_tuning::prefs {

const char kHighEfficiencyModeEnabled[] =
    "performance_tuning.high_efficiency_mode.enabled";

const char kBatterySaverModeEnabled[] =
    "performance_tuning.battery_saver_mode.enabled";

const char kTabDiscardingExceptions[] =
    "performance_tuning.tab_discarding.exceptions";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHighEfficiencyModeEnabled, false);
  registry->RegisterBooleanPref(kBatterySaverModeEnabled, false);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kTabDiscardingExceptions);
}

}  // namespace performance_manager::user_tuning::prefs