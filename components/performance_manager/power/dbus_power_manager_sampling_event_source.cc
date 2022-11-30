// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/power/dbus_power_manager_sampling_event_source.h"

namespace performance_manager::power {

DbusPowerManagerSamplingEventSource::DbusPowerManagerSamplingEventSource(
    chromeos::PowerManagerClient* power_manager_client)
    : power_manager_client_(power_manager_client) {
  DCHECK(power_manager_client_);
}

DbusPowerManagerSamplingEventSource::~DbusPowerManagerSamplingEventSource() =
    default;

bool DbusPowerManagerSamplingEventSource::Start(
    base::SamplingEventSource::SamplingEventCallback callback) {
  DCHECK(power_manager_client_);

  callback_ = callback;
  observation_.Observe(power_manager_client_);
  return true;
}

void DbusPowerManagerSamplingEventSource::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  callback_.Run();
}

}  // namespace performance_manager::power
