// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/power_metrics/energy_metrics_provider_linux.h"

#include <linux/perf_event.h>
#include <sys/syscall.h>

#include <array>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace power_metrics {

namespace {

constexpr const char* kPowerEventPath = "/sys/bus/event_source/devices/power";

// Existing metrics that can be read via perf event.
constexpr std::array<const char*, 5> kMetrics{
    "energy-pkg", "energy-cores", "energy-gpu", "energy-ram", "energy-psys"};

bool ReadUint64FromFile(base::FilePath path, uint64_t* output) {
  std::string buf;
  if (!base::ReadFileToString(path, &buf)) {
    return false;
  }
  return base::StringToUint64(base::TrimString(buf, "\n", base::TRIM_TRAILING),
                              output);
}

bool ReadHexFromFile(base::FilePath path, uint64_t* output) {
  std::string buf;
  if (!base::ReadFileToString(path, &buf)) {
    return false;
  }
  base::ReplaceFirstSubstringAfterOffset(&buf, 0, "event=", "");
  return base::HexStringToUInt64(
      base::TrimString(buf, "\n", base::TRIM_TRAILING), output);
}

bool ReadDoubleFromFile(base::FilePath path, double* output) {
  std::string buf;
  if (!base::ReadFileToString(path, &buf)) {
    return false;
  }
  return base::StringToDouble(base::TrimString(buf, "\n", base::TRIM_TRAILING),
                              output);
}

// When pid == -1 and cpu >= 0, perf event measures all processes/threads on the
// specified CPU. This requires admin or a /proc/sys/kernel/perf_event_paranoid
// value of less than 1. Here, we only consider cpu0. See details in
// https://man7.org/linux/man-pages/man2/perf_event_open.2.html.
base::ScopedFD OpenPerfEvent(perf_event_attr* perf_attr) {
  base::ScopedFD perf_fd(syscall(__NR_perf_event_open, perf_attr, /*pid=*/-1,
                                 /*cpu=*/0, /*group_fd=*/-1,
                                 static_cast<int>(PERF_FLAG_FD_CLOEXEC)));
  return perf_fd;
}

void SetEnergyMetric(const std::string& metric_type,
                     EnergyMetricsProvider::EnergyMetrics& energy_metrics,
                     uint64_t absolute_energy) {
  if (metric_type == "energy-pkg") {
    energy_metrics.package_nanojoules = absolute_energy;
  } else if (metric_type == "energy-cores") {
    energy_metrics.cpu_nanojoules = absolute_energy;
  } else if (metric_type == "energy-gpu") {
    energy_metrics.gpu_nanojoules = absolute_energy;
  } else if (metric_type == "energy-ram") {
    energy_metrics.dram_nanojoules = absolute_energy;
  } else if (metric_type == "energy-psys") {
    energy_metrics.psys_nanojoules = absolute_energy;
  }
}

}  // namespace

EnergyMetricsProviderLinux::PowerEvent::PowerEvent(std::string metric_type,
                                                   double scale,
                                                   base::ScopedFD fd)
    : metric_type(metric_type), scale(scale), fd(std::move(fd)) {}

EnergyMetricsProviderLinux::PowerEvent::~PowerEvent() = default;

EnergyMetricsProviderLinux::PowerEvent::PowerEvent(PowerEvent&& other) =
    default;
EnergyMetricsProviderLinux::PowerEvent&
EnergyMetricsProviderLinux::PowerEvent::operator=(PowerEvent&& other) = default;

EnergyMetricsProviderLinux::EnergyMetricsProviderLinux() = default;
EnergyMetricsProviderLinux::~EnergyMetricsProviderLinux() = default;

// static
std::unique_ptr<EnergyMetricsProviderLinux>
EnergyMetricsProviderLinux::Create() {
  return base::WrapUnique(new EnergyMetricsProviderLinux());
}

std::optional<EnergyMetricsProvider::EnergyMetrics>
EnergyMetricsProviderLinux::CaptureMetrics() {
  if (!Initialize()) {
    return std::nullopt;
  }

  EnergyMetrics energy_metrics = {0};
  for (const auto& event : events_) {
    uint64_t absolute_energy;
    if (!base::ReadFromFD(
            event.fd.get(),
            base::as_writable_chars(base::span_from_ref(absolute_energy)))) {
      LOG(ERROR) << "Failed to read absolute energy of " << event.metric_type;
      continue;
    }
    SetEnergyMetric(event.metric_type, energy_metrics,
                    static_cast<uint64_t>(event.scale * absolute_energy));
  }
  return energy_metrics;
}

bool EnergyMetricsProviderLinux::Initialize() {
  if (is_initialized_) {
    if (events_.empty()) {
      return false;
    }
    return true;
  }

  is_initialized_ = true;

  // Check if there are available power-related events on local platform.
  if (!base::PathExists(base::FilePath(kPowerEventPath))) {
    LOG(WARNING) << "No available power event";
    return false;
  }

  // Check if perf_event_paranoid is set to 0 as required.
  uint64_t perf_event_paranoid;
  if (!ReadUint64FromFile(
          base::FilePath("/proc/sys/kernel/perf_event_paranoid"),
          &perf_event_paranoid)) {
    LOG(WARNING) << "Failed to get perf_event_paranoid";
    return false;
  }
  if (perf_event_paranoid) {
    LOG(WARNING) << "Permission denied for acquiring energy metrics";
    return false;
  }

  // Since the power Processor Monitor Unit (PMU) is dynamic, we have to get the
  // type for perf_event_attr from /sys/bus/event_source/devices/power/type.
  uint64_t attr_type;
  if (!ReadUint64FromFile(
          base::FilePath(base::StrCat({kPowerEventPath, "/type"})),
          &attr_type)) {
    LOG(WARNING) << "Failed to get perf event type";
    return false;
  }

  // For each metric, get their file descriptors.
  for (auto* const metric : kMetrics) {
    base::FilePath config_path =
        base::FilePath(base::StrCat({kPowerEventPath, "/events/", metric}));
    base::FilePath scale_path = base::FilePath(
        base::StrCat({kPowerEventPath, "/events/", metric, ".scale"}));
    // Some energy metrics may be unavailable on different platforms, so the
    // corresponding file path does not exist, which is normal.
    if (!base::PathExists(config_path) || !base::PathExists(scale_path)) {
      continue;
    }

    // Get the specified config for this event.
    uint64_t attr_config;
    if (!ReadHexFromFile(config_path, &attr_config)) {
      LOG(ERROR) << "Failed to get config " << config_path.value();
      continue;
    }

    // Each event has its own scale to convert ticks to joules, which is usually
    // set to 2.3283064365386962890625e-10.
    double scale;
    if (!ReadDoubleFromFile(scale_path, &scale)) {
      LOG(ERROR) << "Failed to get scale of " << metric;
      continue;
    }
    // Convert the unit from joules/tick to nanojoules/tick.
    scale = scale * 1e9;

    perf_event_attr perf_attr = {0};
    perf_attr.size = static_cast<uint32_t>(sizeof(perf_attr));
    perf_attr.type = static_cast<uint32_t>(attr_type);
    perf_attr.config = attr_config;
    base::ScopedFD fd = OpenPerfEvent(&perf_attr);
    if (!fd.is_valid()) {
      LOG(ERROR) << "Failed to get fd of " << metric;
      continue;
    }
    events_.push_back({metric, scale, std::move(fd)});
  }

  if (events_.empty()) {
    LOG(WARNING) << "No available energy metric";
    return false;
  }
  return true;
}

}  // namespace power_metrics
