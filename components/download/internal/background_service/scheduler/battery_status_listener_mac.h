// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_MAC_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_MAC_H_

#include "components/download/internal/background_service/scheduler/battery_status_listener.h"

namespace download {

// Mac implementation of BatteryStatusListener. Currently always Mac device
// full battery and charging.
// We should investigate if platform code can be correctly hooked to
// base::PowerMonitor on Mac. See https://crbug.com/825878.
class BatteryStatusListenerMac : public BatteryStatusListener {
 public:
  BatteryStatusListenerMac();

  BatteryStatusListenerMac(const BatteryStatusListenerMac&) = delete;
  BatteryStatusListenerMac& operator=(const BatteryStatusListenerMac&) = delete;

  ~BatteryStatusListenerMac() override;

 private:
  // BatteryStatusListener implementation.
  int GetBatteryPercentage() override;
  base::PowerStateObserver::BatteryPowerStatus GetBatteryPowerStatus()
      const override;
  void Start(Observer* observer) override;
  void Stop() override;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_MAC_H_
