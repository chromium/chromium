// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_SYSTEM_INFO_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_SYSTEM_INFO_TEST_UTIL_H_

#include "base/component_export.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace system_info {

// Initializes the fake Power Manager Client and the Fake CrosHealthd. This must
// be run during set up of the test to use these util functions.
void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO) SetUpTest();

// Shuts down the Power Manager Client and the Fake CrosHealthd.
void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO) TestDownTest();

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    SetProbeTelemetryInfoResponse(
        ash::cros_healthd::mojom::BatteryInfoPtr battery_info,
        ash::cros_healthd::mojom::CpuInfoPtr cpu_info,
        ash::cros_healthd::mojom::MemoryInfoPtr memory_info);

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    SetCrosHealthdCpuResponse(
        const std::vector<system_info::CpuUsageData>& usage_data,
        const std::vector<int32_t>& cpu_temps,
        const std::vector<uint32_t>& scaled_cpu_clock_speed);

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    SetCrosHealthdMemoryUsageResponse(uint32_t total_memory_kib,
                                      uint32_t free_memory_kib,
                                      uint32_t available_memory_kib);

// Constructs a BatteryInfoPtr.
ash::cros_healthd::mojom::BatteryInfoPtr COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    CreateCrosHealthdBatteryHealthResponse(double charge_full_now,
                                           double charge_full_design,
                                           int32_t cycle_count);

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    SetCrosHealthdBatteryHealthResponse(double charge_full_now,
                                        double charge_full_design,
                                        int32_t cycle_count);

bool COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    AreValidPowerTimes(int64_t time_to_full, int64_t time_to_empty);

power_manager::PowerSupplyProperties COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    ConstructPowerSupplyProperties(
        power_manager::PowerSupplyProperties::ExternalPower power_source,
        power_manager::PowerSupplyProperties::BatteryState battery_state,
        bool is_calculating_battery_time,
        int64_t time_to_full,
        int64_t time_to_empty,
        double battery_percent);

// Sets the PowerSupplyProperties on FakePowerManagerClient. Calling this
// method immediately notifies PowerManagerClient observers. One of
// `time_to_full` or `time_to_empty` must be either -1 or a positive number.
// The other must be 0. If `battery_state` is NOT_PRESENT, both `time_to_full`
// and `time_to_empty` will be left unset.
void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    SetPowerManagerProperties(
        power_manager::PowerSupplyProperties::ExternalPower power_source,
        power_manager::PowerSupplyProperties::BatteryState battery_state,
        bool is_calculating_battery_time,
        int64_t time_to_full,
        int64_t time_to_empty,
        double battery_percent);

ash::cros_healthd::mojom::ProbeErrorPtr COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    CreateProbeError(ash::cros_healthd::mojom::ErrorType error_type);

}  // namespace system_info

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_SYSTEM_INFO_TEST_UTIL_H_
