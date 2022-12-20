// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/scheduler_configuration_manager_base.h"

namespace ash {

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
