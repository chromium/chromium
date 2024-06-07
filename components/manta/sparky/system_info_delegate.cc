// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/system_info_delegate.h"

#include <memory>

namespace manta {

StorageData::StorageData(const std::string& in_use_bytes,
                         const std::string& total_bytes)
    : in_use_bytes(in_use_bytes), total_bytes(total_bytes) {}

StorageData::~StorageData() = default;

BatteryData::BatteryData(int cycle_count,
                         int battery_wear_percentage,
                         const std::string& power_time,
                         int battery_percentage)
    : cycle_count(cycle_count),
      battery_wear_percentage(battery_wear_percentage),
      power_time(power_time),
      battery_percentage(battery_percentage) {}

BatteryData::~BatteryData() = default;

CpuData::CpuData(int cpu_usage_percentage_snapshot,
                 int average_cpu_temp_celsius,
                 double scaling_current_frequency_ghz)
    : cpu_usage_percentage_snapshot(cpu_usage_percentage_snapshot),
      average_cpu_temp_celsius(average_cpu_temp_celsius),
      scaling_current_frequency_ghz(scaling_current_frequency_ghz) {}

CpuData::~CpuData() = default;

MemoryData::MemoryData(double available_memory_gb, double total_memory_gb)
    : available_memory_gb(available_memory_gb),
      total_memory_gb(total_memory_gb) {}

MemoryData::~MemoryData() = default;

DiagnosticsData::DiagnosticsData(std::unique_ptr<BatteryData> battery_data,
                                 std::unique_ptr<CpuData> cpu_data,
                                 std::unique_ptr<MemoryData> memory_data)
    : battery_data(std::move(battery_data)),
      cpu_data(std::move(cpu_data)),
      memory_data(std::move(memory_data)) {}

DiagnosticsData::~DiagnosticsData() = default;

SystemInfoDelegate::SystemInfoDelegate() = default;
SystemInfoDelegate::~SystemInfoDelegate() = default;

}  // namespace manta
