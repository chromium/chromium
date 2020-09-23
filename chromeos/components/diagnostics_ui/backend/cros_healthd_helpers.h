// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_CROS_HEALTHD_HELPERS_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_CROS_HEALTHD_HELPERS_H_

namespace cros_healthd {
namespace mojom {
class BatteryInfo;
class CpuInfo;
class MemoryInfo;
class SystemInfo;
class TelemetryInfo;
}  // namespace mojom
}  // namespace cros_healthd

namespace chromeos {
namespace diagnostics {

// Extracts BatteryInfo from |info|. Logs and returns a nullptr if
// BatteryInfo in not present.
const cros_healthd::mojom::BatteryInfo* GetBatteryInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

// Extracts CpuInfo from |info|. Logs and returns a nullptr if CpuInfo
// in not present.
const cros_healthd::mojom::CpuInfo* GetCpuInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

// Extracts MemoryInfo from |info|. Logs and returns a nullptr if MemoryInfo
// in not present.
const cros_healthd::mojom::MemoryInfo* GetMemoryInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

// Extracts SystemInfo from |info|. Logs and returns a nullptr if SystemInfo
// in not present.
const cros_healthd::mojom::SystemInfo* GetSystemInfo(
    const cros_healthd::mojom::TelemetryInfo& info);

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_CROS_HEALTHD_HELPERS_H_
