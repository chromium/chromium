// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/scheduler_configuration_manager_base.h"

namespace chromeos {

SchedulerConfigurationManagerBase::SchedulerConfigurationManagerBase() =
    default;
SchedulerConfigurationManagerBase::~SchedulerConfigurationManagerBase() =
    default;

void SchedulerConfigurationManagerBase::AddObserver(
    SchedulerConfigurationManagerBase::Observer* obs) {
  observer_list_.AddObserver(obs);
}

void SchedulerConfigurationManagerBase::RemoveObserver(
    const SchedulerConfigurationManagerBase::Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

}  // namespace chromeos
