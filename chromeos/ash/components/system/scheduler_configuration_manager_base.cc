// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/scheduler_configuration_manager_base.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

// static
void SchedulerConfigurationManagerBase::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Ideally the pref would be registered specifying the default provided via
  // the feature parameter. This is unfortunately not possible though because
  // the feature API initialization depends on the local state PrefService, so
  // this function runs before feature parameters are available.
  registry->RegisterStringPref(prefs::kSchedulerConfiguration, std::string());
}

SchedulerConfigurationManagerBase::SchedulerConfigurationManagerBase() =
    default;
SchedulerConfigurationManagerBase::~SchedulerConfigurationManagerBase() =
    default;

void SchedulerConfigurationManagerBase::AddObserver(
    SchedulerConfigurationManagerBase::Observer* obs) {
  observer_list_.AddObserver(obs);
}

void SchedulerConfigurationManagerBase::RemoveObserver(
    SchedulerConfigurationManagerBase::Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

}  // namespace ash
