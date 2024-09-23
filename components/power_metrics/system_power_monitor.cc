// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/system_power_monitor.h"

#include <array>
#include <cstring>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"

namespace power_metrics {

namespace {

constexpr const char kTraceCategory[] =
    TRACE_DISABLED_BY_DEFAULT("system_power");

constexpr const char kPackagePowerTraceCounterName[] = "Package Power (mW)";
constexpr const char kCpuPowerTraceCounterName[] = "CPU Power (mW)";
constexpr const char kIntegratedGpuPowerTraceCounterName[] = "iGPU Power (mW)";
constexpr const char kDramPowerTraceCounterName[] = "DRAM Power (mW)";
constexpr const char kPsysPowerTraceCounterName[] = "Psys Power (mW)";
constexpr const char kVddcrVddTraceCounterName[] = "VDDCR VDD (mW)";
constexpr const char kVddcrSocTraceCounterName[] = "VDDCR SOC (mW)";
constexpr const char kCurrentSocketTraceCounterName[] = "Current Socket (mW)";
constexpr const char kApuPowerTraceCounterName[] = "APU Power (mW)";

// Here we determine if the specified metric is valid according to whether its
// corresponding value in the provided sample is greater than 0, since the
// absolute energy must be greater than 0.
bool GenerateValidMetrics(const EnergyMetricsProvider::EnergyMetrics& sample,
                          std::vector<const char*>& valid_metrics) {
  if (sample.package_nanojoules > 0) {
    valid_metrics.push_back(kPackagePowerTraceCounterName);
  }
  if (sample.cpu_nanojoules > 0) {
    valid_metrics.push_back(kCpuPowerTraceCounterName);
  }
  if (sample.gpu_nanojoules > 0) {
    valid_metrics.push_back(kIntegratedGpuPowerTraceCounterName);
  }
  if (sample.dram_nanojoules > 0) {
    valid_metrics.push_back(kDramPowerTraceCounterName);
  }
  if (sample.psys_nanojoules > 0) {
    valid_metrics.push_back(kPsysPowerTraceCounterName);
  }
  if (sample.vdd_nanojoules > 0) {
    valid_metrics.push_back(kVddcrVddTraceCounterName);
  }
  if (sample.soc_nanojoules > 0) {
    valid_metrics.push_back(kVddcrSocTraceCounterName);
  }
  if (sample.socket_nanojoules > 0) {
    valid_metrics.push_back(kCurrentSocketTraceCounterName);
  }
  if (sample.apu_nanojoules > 0) {
    valid_metrics.push_back(kApuPowerTraceCounterName);
  }
  return !valid_metrics.empty();
}

int64_t CalculateNanojoulesDeltaFromSamples(
    const EnergyMetricsProvider::EnergyMetrics& new_sample,
    const EnergyMetricsProvider::EnergyMetrics& old_sample,
    const char* metric) {
  if (std::strcmp(metric, kPackagePowerTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.package_nanojoules -
                                old_sample.package_nanojoules);
  } else if (std::strcmp(metric, kCpuPowerTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.cpu_nanojoules -
                                old_sample.cpu_nanojoules);
  } else if (std::strcmp(metric, kIntegratedGpuPowerTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.gpu_nanojoules -
                                old_sample.gpu_nanojoules);
  } else if (std::strcmp(metric, kDramPowerTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.dram_nanojoules -
                                old_sample.dram_nanojoules);
  } else if (std::strcmp(metric, kPsysPowerTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.psys_nanojoules -
                                old_sample.psys_nanojoules);
  } else if (std::strcmp(metric, kVddcrVddTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.vdd_nanojoules -
                                old_sample.vdd_nanojoules);
  } else if (std::strcmp(metric, kVddcrSocTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.soc_nanojoules -
                                old_sample.soc_nanojoules);
  } else if (std::strcmp(metric, kCurrentSocketTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.socket_nanojoules -
                                old_sample.socket_nanojoules);
  } else if (std::strcmp(metric, kApuPowerTraceCounterName) == 0) {
    return static_cast<int64_t>(new_sample.apu_nanojoules -
                                old_sample.apu_nanojoules);
  }
  NOTREACHED() << "Unexpected metric: " << metric;
}

}  // namespace

SystemPowerMonitorDelegate::SystemPowerMonitorDelegate() = default;
SystemPowerMonitorDelegate::~SystemPowerMonitorDelegate() = default;

void SystemPowerMonitorDelegate::RecordSystemPower(const char* metric,
                                                   base::TimeTicks timestamp,
                                                   int64_t power) {
  TRACE_COUNTER_WITH_TIMESTAMP1(TRACE_DISABLED_BY_DEFAULT("system_power"),
                                metric, timestamp, power);
}

bool SystemPowerMonitorDelegate::IsTraceCategoryEnabled() const {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTraceCategory, &enabled);
  return enabled;
}

SystemPowerMonitorHelper::SystemPowerMonitorHelper(
    std::unique_ptr<EnergyMetricsProvider> provider,
    std::unique_ptr<SystemPowerMonitorDelegate> delegate)
    : provider_(std::move(provider)), delegate_(std::move(delegate)) {}

SystemPowerMonitorHelper::~SystemPowerMonitorHelper() = default;

void SystemPowerMonitorHelper::Start() {
  CHECK(provider_);
  CHECK(!timer_.IsRunning());
  if (!delegate_->IsTraceCategoryEnabled()) {
    return;
  }

  // If the provider fails to capture valid sample at the first time, we
  // determine that it is unable to provide valid data and give up starting the
  // timer.
  auto sample = provider_->CaptureMetrics();
  if (!sample.has_value()) {
    return;
  }

  // To avoid redundant loops on invalid metrics, we select the valid metrics
  // before start.
  CHECK(valid_metrics_.empty());
  if (!GenerateValidMetrics(sample.value(), valid_metrics_)) {
    return;
  }

  last_sample_ = sample.value();
  last_timestamp_ = base::TimeTicks::Now();

  timer_.Start(FROM_HERE, kDefaultSampleInterval,
               base::BindRepeating(&SystemPowerMonitorHelper::Sample,
                                   base::Unretained(this)));
}

void SystemPowerMonitorHelper::Stop() {
  timer_.Stop();
  valid_metrics_.clear();
}

void SystemPowerMonitorHelper::Sample() {
  // If the provider fails to capture valid metrics after the timer started,
  // we leave the timer running.
  auto sample = provider_->CaptureMetrics();
  if (!sample.has_value()) {
    return;
  }

  base::TimeTicks timestamp = base::TimeTicks::Now();
  base::TimeDelta interval = timestamp - last_timestamp_;
  CHECK(interval.is_positive());

  for (auto const* metric : valid_metrics_) {
    int64_t nanojoules = CalculateNanojoulesDeltaFromSamples(
        sample.value(), last_sample_, metric);
    CHECK_GE(nanojoules, 0ll);

    int64_t milliwatts = nanojoules / interval.InMicroseconds();
    delegate_->RecordSystemPower(metric, last_timestamp_, milliwatts);
  }

  last_sample_ = sample.value();
  last_timestamp_ = timestamp;
}

bool SystemPowerMonitorHelper::IsTimerRunningForTesting() {
  return timer_.IsRunning();
}

SystemPowerMonitor::SystemPowerMonitor()
    : SystemPowerMonitor(EnergyMetricsProvider::Create(),
                         std::make_unique<SystemPowerMonitorDelegate>()) {}

SystemPowerMonitor::SystemPowerMonitor(
    std::unique_ptr<EnergyMetricsProvider> provider,
    std::unique_ptr<SystemPowerMonitorDelegate> delegate) {
  helper_ = base::SequenceBound<SystemPowerMonitorHelper>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::BEST_EFFORT}),
      std::move(provider), std::move(delegate));
}

SystemPowerMonitor::~SystemPowerMonitor() = default;

// static
SystemPowerMonitor* SystemPowerMonitor::GetInstance() {
  static base::NoDestructor<SystemPowerMonitor> instance;
  return instance.get();
}

void SystemPowerMonitor::OnTraceLogEnabled() {
  helper_.AsyncCall(&SystemPowerMonitorHelper::Start);
}

void SystemPowerMonitor::OnTraceLogDisabled() {
  helper_.AsyncCall(&SystemPowerMonitorHelper::Stop);
}

base::SequenceBound<SystemPowerMonitorHelper>*
SystemPowerMonitor::GetHelperForTesting() {
  return helper_ ? &helper_ : nullptr;
}

}  // namespace power_metrics
