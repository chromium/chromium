// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_POWER_DBUS_POWER_MANAGER_SAMPLING_EVENT_SOURCE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_POWER_DBUS_POWER_MANAGER_SAMPLING_EVENT_SOURCE_H_

#include "base/power_monitor/sampling_event_source.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace performance_manager::power {

class DbusPowerManagerSamplingEventSource
    : public base::SamplingEventSource,
      public chromeos::PowerManagerClient::Observer {
 public:
  explicit DbusPowerManagerSamplingEventSource(
      chromeos::PowerManagerClient* power_manager_client);
  ~DbusPowerManagerSamplingEventSource() override;

  bool Start(
      base::SamplingEventSource::SamplingEventCallback callback) override;

 private:
  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  base::SamplingEventSource::SamplingEventCallback callback_;
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      observation_{this};
  raw_ptr<chromeos::PowerManagerClient> const power_manager_client_;
};

}  // namespace performance_manager::power

#endif  // COMPONENTS_PERFORMANCE_MANAGER_POWER_DBUS_POWER_MANAGER_SAMPLING_EVENT_SOURCE_H_
