// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_SYSTEM_POWER_MONITOR_H_
#define COMPONENTS_POWER_METRICS_SYSTEM_POWER_MONITOR_H_

#include <memory>
#include <vector>

#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_log.h"
#include "components/power_metrics/energy_metrics_provider.h"

namespace power_metrics {

// A delegate to isolate System Power Monitor functionality mainly for
// testing.
class SystemPowerMonitorDelegate {
 public:
  SystemPowerMonitorDelegate();

  SystemPowerMonitorDelegate(const SystemPowerMonitorDelegate&) = delete;
  SystemPowerMonitorDelegate& operator=(const SystemPowerMonitorDelegate&) =
      delete;

  virtual ~SystemPowerMonitorDelegate();

  // Emits trace counter. The metric string stands for trace counter name,
  // timestamp is the counter timestamp, power is the corresponding counter
  // value of system power consumption in units of milliwatts.
  virtual void RecordSystemPower(const char* metric,
                                 base::TimeTicks timestamp,
                                 int64_t power);

  // Returns whether the tracing category is enabled to determine if we should
  // record.
  virtual bool IsTraceCategoryEnabled() const;
};

// Manages a timer to regularly sample and emit trace events, whose start and
// stop are controlled by System Power Monitor.
class SystemPowerMonitorHelper {
 public:
  // Default sampling interval, which should be set to larger or equal to 50 ms.
  static constexpr base::TimeDelta kDefaultSampleInterval =
      base::Milliseconds(50);

  SystemPowerMonitorHelper(
      std::unique_ptr<EnergyMetricsProvider> provider,
      std::unique_ptr<SystemPowerMonitorDelegate> delegate);

  SystemPowerMonitorHelper(const SystemPowerMonitorHelper&) = delete;
  SystemPowerMonitorHelper& operator=(const SystemPowerMonitorHelper&) = delete;

  ~SystemPowerMonitorHelper();

  void Start();
  void Stop();
  void Sample();

  bool IsTimerRunningForTesting();

 private:
  std::vector<const char*> valid_metrics_;
  EnergyMetricsProvider::EnergyMetrics last_sample_;
  base::TimeTicks last_timestamp_;
  base::RepeatingTimer timer_;

  // Used to derive instant system energy metrics.
  std::unique_ptr<EnergyMetricsProvider> provider_;
  std::unique_ptr<SystemPowerMonitorDelegate> delegate_;
};

// Monitors system-wide power consumption. Gets data from EnergyMetricsProvider.
class SystemPowerMonitor
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  SystemPowerMonitor();

  SystemPowerMonitor(const SystemPowerMonitor&) = delete;
  SystemPowerMonitor& operator=(const SystemPowerMonitor&) = delete;

  ~SystemPowerMonitor() override;

  static SystemPowerMonitor* GetInstance();

  // TraceLog::EnabledStateObserver.
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

 private:
  friend class SystemPowerMonitorTest;

  SystemPowerMonitor(std::unique_ptr<EnergyMetricsProvider> provider,
                     std::unique_ptr<SystemPowerMonitorDelegate> delegate);

  base::SequenceBound<SystemPowerMonitorHelper>* GetHelperForTesting();

  base::SequenceBound<SystemPowerMonitorHelper> helper_;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_SYSTEM_POWER_MONITOR_H_
