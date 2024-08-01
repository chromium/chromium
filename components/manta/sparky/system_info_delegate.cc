// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/system_info_delegate.h"

#include <memory>
#include <optional>

namespace manta {

StorageData::StorageData(const std::string& free_bytes,
                         const std::string& total_bytes)
    : free_bytes(free_bytes), total_bytes(total_bytes) {}

StorageData::~StorageData() = default;
StorageData::StorageData(const StorageData&) = default;
StorageData& StorageData::operator=(const StorageData&) = default;

BatteryData::BatteryData(int cycle_count,
                         int battery_wear_percentage,
                         const std::string& power_time,
                         int battery_percentage)
    : cycle_count(cycle_count),
      battery_wear_percentage(battery_wear_percentage),
      power_time(power_time),
      battery_percentage(battery_percentage) {}

BatteryData::~BatteryData() = default;

BatteryData::BatteryData(const BatteryData&) = default;
BatteryData& BatteryData::operator=(const BatteryData&) = default;

CpuData::CpuData(int cpu_usage_percentage_snapshot,
                 int average_cpu_temp_celsius,
                 double scaling_current_frequency_ghz)
    : cpu_usage_percentage_snapshot(cpu_usage_percentage_snapshot),
      average_cpu_temp_celsius(average_cpu_temp_celsius),
      scaling_current_frequency_ghz(scaling_current_frequency_ghz) {}

CpuData::~CpuData() = default;

CpuData::CpuData(const CpuData&) = default;
CpuData& CpuData::operator=(const CpuData&) = default;

MemoryData::MemoryData(double available_memory_gb, double total_memory_gb)
    : available_memory_gb(available_memory_gb),
      total_memory_gb(total_memory_gb) {}

MemoryData::~MemoryData() = default;

MemoryData::MemoryData(const MemoryData&) = default;
MemoryData& MemoryData::operator=(const MemoryData&) = default;

DiagnosticsData::DiagnosticsData(std::optional<BatteryData> battery_data,
                                 std::optional<CpuData> cpu_data,
                                 std::optional<MemoryData> memory_data,
                                 std::optional<StorageData> storage_data)
    : battery_data(std::move(battery_data)),
      cpu_data(std::move(cpu_data)),
      memory_data(std::move(memory_data)),
      storage_data(std::move(storage_data)) {}

DiagnosticsData::~DiagnosticsData() = default;

DiagnosticsData::DiagnosticsData(const DiagnosticsData&) = default;
DiagnosticsData& DiagnosticsData::operator=(const DiagnosticsData&) = default;

SystemInfoDelegate::SystemInfoDelegate() = default;
SystemInfoDelegate::~SystemInfoDelegate() = default;

}  // namespace manta
