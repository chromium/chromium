// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/android_battery_metrics.h"

#include "base/android/radio_utils.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/power_monitor/power_monitor.h"
#include "net/android/network_library.h"
#include "net/android/traffic_stats.h"

namespace power_metrics {
namespace {

void Report30SecondRadioUsage(int64_t tx_bytes, int64_t rx_bytes) {
  if (!base::android::RadioUtils::IsSupported())
    return;

  if (base::android::RadioUtils::IsWifiConnected()) {
    base::Optional<int32_t> maybe_level = net::android::GetWifiSignalLevel();
    if (!maybe_level.has_value())
      return;

    base::android::RadioSignalLevel wifi_level =
        static_cast<base::android::RadioSignalLevel>(*maybe_level);
    UMA_HISTOGRAM_ENUMERATION("Power.ForegroundRadio.SignalLevel.Wifi",
                              wifi_level);

    // Traffic sent over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.SentKiB.Wifi.30Seconds", wifi_level, tx_bytes,
        1024);

    // Traffic received over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.ReceivedKiB.Wifi.30Seconds", wifi_level,
        rx_bytes, 1024);
  } else {
    base::Optional<base::android::RadioSignalLevel> maybe_level =
        base::android::RadioUtils::GetCellSignalLevel();
    if (!maybe_level.has_value())
      return;

    base::android::RadioSignalLevel cell_level = *maybe_level;
    UMA_HISTOGRAM_ENUMERATION("Power.ForegroundRadio.SignalLevel.Cell",
                              cell_level);

    // Traffic sent over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.SentKiB.Cell.30Seconds", cell_level, tx_bytes,
        1024);

    // Traffic received over network during the last 30 seconds in kibibytes.
    UMA_HISTOGRAM_SCALED_ENUMERATION(
        "Power.ForegroundRadio.ReceivedKiB.Cell.30Seconds", cell_level,
        rx_bytes, 1024);
  }
}

void Report30SecondDrain(int capacity_consumed, bool is_exclusive_measurement) {
  // Drain over the last 30 seconds in uAh. We assume a max current of 10A which
  // translates to a little under 100mAh capacity drain over 30 seconds.
  UMA_HISTOGRAM_COUNTS_100000("Power.ForegroundBatteryDrain.30Seconds",
                              capacity_consumed);

  // Record a separate metric for power drain that was completely observed while
  // we were the foreground app. This avoids attributing power draw from other
  // apps to us.
  if (is_exclusive_measurement) {
    UMA_HISTOGRAM_COUNTS_100000(
        "Power.ForegroundBatteryDrain.30Seconds.Exclusive", capacity_consumed);
  }
}

void ReportAveragedDrain(int capacity_consumed,
                         bool is_exclusive_measurement,
                         int num_sampling_periods) {
  // Averaged drain over 30 second intervals in uAh. We assume a max current of
  // 10A which translates to a little under 100mAh capacity drain over 30
  // seconds.
  static const char kName[] = "Power.ForegroundBatteryDrain.30SecondsAvg";
  STATIC_HISTOGRAM_POINTER_BLOCK(
      kName,
      AddCount(capacity_consumed / num_sampling_periods, num_sampling_periods),
      base::Histogram::FactoryGet(
          kName, /*min_value=*/1, /*max_value=*/100000, /*bucket_count=*/50,
          base::HistogramBase::kUmaTargetedHistogramFlag));

  if (is_exclusive_measurement) {
    static const char kExclusiveName[] =
        "Power.ForegroundBatteryDrain.30SecondsAvg.Exclusive";
    STATIC_HISTOGRAM_POINTER_BLOCK(
        kExclusiveName,
        AddCount(capacity_consumed / num_sampling_periods,
                 num_sampling_periods),
        base::Histogram::FactoryGet(
            kExclusiveName, /*min_value=*/1, /*max_value=*/100000,
            /*bucket_count=*/50,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }

  // Also report the time it took for us to detect this drop to see what the
  // overall metric sensitivity is.
  UMA_HISTOGRAM_LONG_TIMES_100(
      "Power.ForegroundBatteryDrain.TimeBetweenEvents",
      base::TimeDelta::FromSeconds(30 * num_sampling_periods));
}

}  // namespace

// static
constexpr base::TimeDelta AndroidBatteryMetrics::kMetricsInterval;

AndroidBatteryMetrics::AndroidBatteryMetrics()
    : app_visible_(false),
      on_battery_power_(base::PowerMonitor::IsOnBatteryPower()) {
  base::PowerMonitor::AddObserver(this);
  UpdateMetricsEnabled();
}

AndroidBatteryMetrics::~AndroidBatteryMetrics() {
  base::PowerMonitor::RemoveObserver(this);
}

void AndroidBatteryMetrics::OnAppVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  app_visible_ = visible;
  UpdateMetricsEnabled();
}

void AndroidBatteryMetrics::OnPowerStateChange(bool on_battery_power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_battery_power_ = on_battery_power;
  UpdateMetricsEnabled();
}

void AndroidBatteryMetrics::UpdateMetricsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We want to attribute battery drain to chromium while the embedding app is
  // visible. Battery drain will only be reflected in remaining battery capacity
  // when the device is not on a charger.
  bool should_be_enabled = app_visible_ && on_battery_power_;

  if (should_be_enabled && !metrics_timer_.IsRunning()) {
    // Capture first capacity measurement and enable the repeating timer.
    last_remaining_capacity_uah_ =
        base::PowerMonitor::GetRemainingBatteryCapacity();
    if (!net::android::traffic_stats::GetTotalTxBytes(&last_tx_bytes_))
      last_tx_bytes_ = -1;
    if (!net::android::traffic_stats::GetTotalRxBytes(&last_rx_bytes_))
      last_rx_bytes_ = -1;
    skipped_timers_ = 0;
    observed_capacity_drops_ = 0;

    metrics_timer_.Start(FROM_HERE, kMetricsInterval, this,
                         &AndroidBatteryMetrics::CaptureAndReportMetrics);
  } else if (!should_be_enabled && metrics_timer_.IsRunning()) {
    // Capture one last measurement before disabling the timer.
    CaptureAndReportMetrics();
    metrics_timer_.Stop();
  }
}

void AndroidBatteryMetrics::UpdateAndReportRadio() {
  int64_t tx_bytes;
  int64_t rx_bytes;
  if (!net::android::traffic_stats::GetTotalTxBytes(&tx_bytes))
    tx_bytes = -1;
  if (!net::android::traffic_stats::GetTotalRxBytes(&rx_bytes))
    rx_bytes = -1;

  if (last_tx_bytes_ > 0 && tx_bytes > 0 && last_rx_bytes_ > 0 &&
      rx_bytes > 0) {
    Report30SecondRadioUsage(tx_bytes - last_tx_bytes_,
                             rx_bytes - last_rx_bytes_);
  }
  last_tx_bytes_ = tx_bytes;
  last_rx_bytes_ = rx_bytes;
}

void AndroidBatteryMetrics::CaptureAndReportMetrics() {
  int remaining_capacity_uah =
      base::PowerMonitor::GetRemainingBatteryCapacity();

  if (remaining_capacity_uah >= last_remaining_capacity_uah_) {
    // No change in battery capacity, or it increased. The latter could happen
    // if we detected the switch off battery power to a charger late, or if the
    // device reports bogus values. We don't change last_remaining_capacity_uah_
    // here to avoid overreporting in case of fluctuating values.
    skipped_timers_++;
    Report30SecondDrain(0, IsMeasuringDrainExclusively());
    UpdateAndReportRadio();

    return;
  }
  observed_capacity_drops_++;

  // Report the consumed capacity delta over the last 30 seconds.
  int capacity_consumed = last_remaining_capacity_uah_ - remaining_capacity_uah;
  Report30SecondDrain(capacity_consumed, IsMeasuringDrainExclusively());
  UpdateAndReportRadio();

  // Also record drain over 30 second intervals, but averaged since the last
  // time we recorded an increase (or started recording samples). Because the
  // underlying battery capacity counter is often low-resolution (usually
  // between .5 and 50 mAh), it may only increment after multiple sampling
  // points.
  ReportAveragedDrain(capacity_consumed, IsMeasuringDrainExclusively(),
                      skipped_timers_ + 1);

  // Also track the total capacity consumed in a single-bucket-histogram,
  // emitting one sample for every 100 uAh drained.
  static constexpr base::Histogram::Sample kSampleFactor = 100;
  UMA_HISTOGRAM_SCALED_EXACT_LINEAR("Power.ForegroundBatteryDrain",
                                    /*sample=*/1, capacity_consumed,
                                    /*sample_max=*/1, kSampleFactor);
  if (IsMeasuringDrainExclusively()) {
    UMA_HISTOGRAM_SCALED_EXACT_LINEAR("Power.ForegroundBatteryDrain.Exclusive",
                                      /*sample=*/1, capacity_consumed,
                                      /*sample_max=*/1, kSampleFactor);
  }

  last_remaining_capacity_uah_ = remaining_capacity_uah;
  skipped_timers_ = 0;
}

bool AndroidBatteryMetrics::IsMeasuringDrainExclusively() const {
  return observed_capacity_drops_ >= 2;
}

}  // namespace power_metrics
