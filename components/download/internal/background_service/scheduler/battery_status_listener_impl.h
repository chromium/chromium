// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/download/internal/background_service/scheduler/battery_status_listener.h"

namespace download {

// Default implementation of BatteryStatusListener.
class BatteryStatusListenerImpl : public BatteryStatusListener,
                                  public base::PowerStateObserver {
 public:
  explicit BatteryStatusListenerImpl(
      const base::TimeDelta& battery_query_interval);

  BatteryStatusListenerImpl(const BatteryStatusListenerImpl&) = delete;
  BatteryStatusListenerImpl& operator=(const BatteryStatusListenerImpl&) =
      delete;

  ~BatteryStatusListenerImpl() override;

 protected:
  // Platform specific code should override to query the actual battery state.
  virtual int GetBatteryPercentageInternal();

 private:
  // BatteryStatusListener implementation.
  int GetBatteryPercentage() override;
  base::PowerStateObserver::BatteryPowerStatus GetBatteryPowerStatus()
      const override;
  void Start(Observer* observer) override;
  void Stop() override;

  // Updates battery percentage. Will throttle based on
  // |battery_query_interval_| when |force| is false.
  void UpdateBatteryPercentage(bool force);

  // base::PowerStateObserver implementation.
  void OnBatteryPowerStatusChange(base::PowerStateObserver::BatteryPowerStatus
                                      battery_power_status) override;

  // Cached battery percentage.
  int battery_percentage_;

  // Interval to throttle battery queries. Cached value will be returned inside
  // this interval.
  base::TimeDelta battery_query_interval_;

  // Time stamp to record last battery query.
  base::Time last_battery_query_;

  raw_ptr<Observer> observer_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_IMPL_H_
