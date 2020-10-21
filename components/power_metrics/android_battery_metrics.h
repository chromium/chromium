// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_ANDROID_BATTERY_METRICS_H_
#define COMPONENTS_POWER_METRICS_ANDROID_BATTERY_METRICS_H_

#include "base/macros.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"

namespace power_metrics {

// Records metrics around battery usage on Android. The metrics are only tracked
// while the device is not charging and the app is visible (embedder should call
// OnAppVisibilityChanged()). This class is not thread-safe.
class AndroidBatteryMetrics : public base::PowerObserver {
 public:
  AndroidBatteryMetrics();
  ~AndroidBatteryMetrics() override;

  // Should be called by the embedder when the embedder app becomes visible or
  // invisible.
  void OnAppVisibilityChanged(bool visible);

 private:
  // base::PowerObserver implementation:
  void OnPowerStateChange(bool on_battery_power) override;

  void UpdateMetricsEnabled();
  void CaptureAndReportMetrics();
  void UpdateAndReportRadio();

  // Whether or not we've seen at least two consecutive capacity drops while
  // the embedding app was visible. Battery drain reported prior to this could
  // be caused by a different app.
  bool IsMeasuringDrainExclusively() const;

  // Battery drain is captured and reported periodically in this interval while
  // the device is on battery power and the app is visible.
  static constexpr base::TimeDelta kMetricsInterval =
      base::TimeDelta::FromSeconds(30);

  bool app_visible_;
  bool on_battery_power_;
  int last_remaining_capacity_uah_ = 0;
  int64_t last_tx_bytes_ = -1;
  int64_t last_rx_bytes_ = -1;
  base::RepeatingTimer metrics_timer_;
  int skipped_timers_ = 0;

  // Number of consecutive charge drops seen while the app has been visible.
  int observed_capacity_drops_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AndroidBatteryMetrics);
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_ANDROID_BATTERY_METRICS_H_
