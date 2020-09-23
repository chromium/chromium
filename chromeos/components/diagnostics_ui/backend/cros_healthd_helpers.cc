// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/cros_healthd_helpers.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace chromeos {
namespace diagnostics {
namespace {

using cros_healthd::mojom::BatteryInfo;
using cros_healthd::mojom::BatteryResult;
using cros_healthd::mojom::BatteryResultPtr;
using cros_healthd::mojom::CpuInfo;
using cros_healthd::mojom::CpuResult;
using cros_healthd::mojom::CpuResultPtr;
using cros_healthd::mojom::MemoryInfo;
using cros_healthd::mojom::MemoryResult;
using cros_healthd::mojom::MemoryResultPtr;
using cros_healthd::mojom::SystemInfo;
using cros_healthd::mojom::SystemResult;
using cros_healthd::mojom::SystemResultPtr;
using cros_healthd::mojom::TelemetryInfo;

template <typename TResult, typename TTag>
bool CheckResponse(const TResult& result,
                   TTag expected_tag,
                   base::StringPiece type_name) {
  if (result.is_null()) {
    DVLOG(1) << type_name << " not found in croshealthd response.";
    return false;
  }

  auto tag = result->which();
  if (tag == TTag::ERROR) {
    DVLOG(1) << "Error retrieving " << type_name
             << "from croshealthd: " << result->get_error()->msg;
    return false;
  }

  DCHECK_EQ(tag, expected_tag);

  return true;
}

}  // namespace

const BatteryInfo* GetBatteryInfo(const TelemetryInfo& info) {
  const BatteryResultPtr& battery_result = info.battery_result;
  if (!CheckResponse(battery_result, BatteryResult::Tag::BATTERY_INFO,
                     "battery info")) {
    return nullptr;
  }

  return battery_result->get_battery_info().get();
}

const CpuInfo* GetCpuInfo(const TelemetryInfo& info) {
  const CpuResultPtr& cpu_result = info.cpu_result;
  if (!CheckResponse(cpu_result, CpuResult::Tag::CPU_INFO, "cpu info")) {
    return nullptr;
  }

  return cpu_result->get_cpu_info().get();
}

const MemoryInfo* GetMemoryInfo(const TelemetryInfo& info) {
  const MemoryResultPtr& memory_result = info.memory_result;
  if (!CheckResponse(memory_result, MemoryResult::Tag::MEMORY_INFO,
                     "memory info")) {
    return nullptr;
  }

  return memory_result->get_memory_info().get();
}

const SystemInfo* GetSystemInfo(const TelemetryInfo& info) {
  const SystemResultPtr& system_result = info.system_result;
  if (!CheckResponse(system_result, SystemResult::Tag::SYSTEM_INFO,
                     "system info")) {
    return nullptr;
  }

  return system_result->get_system_info().get();
}

}  // namespace diagnostics
}  // namespace chromeos
