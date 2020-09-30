// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/diagnostics_service_converters.h"

#include "base/notreached.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"
#include "chromeos/components/telemetry_extension_ui/convert_ptr.h"
#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"

namespace chromeos {
namespace converters {

namespace unchecked {

namespace {

std::string GetStringFromMojoHandle(mojo::ScopedHandle handle) {
  base::ReadOnlySharedMemoryMapping shared_memory;
  return MojoUtils::GetStringPieceFromMojoHandle(std::move(handle),
                                                 &shared_memory)
      .as_string();
}

}  // namespace

health::mojom::RoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdatePtr input) {
  return health::mojom::RoutineUpdate::New(
      input->progress_percent,
      GetStringFromMojoHandle(std::move(input->output)),
      ConvertPtr(std::move(input->routine_update_union)));
}

health::mojom::RoutineUpdateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdateUnionPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::RoutineUpdateUnion::Tag::INTERACTIVE_UPDATE:
      return health::mojom::RoutineUpdateUnion::NewInteractiveUpdate(
          ConvertPtr(std::move(input->get_interactive_update())));
    case cros_healthd::mojom::RoutineUpdateUnion::Tag::NONINTERACTIVE_UPDATE:
      return health::mojom::RoutineUpdateUnion::NewNoninteractiveUpdate(
          ConvertPtr(std::move(input->get_noninteractive_update())));
  }
}

health::mojom::InteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::InteractiveRoutineUpdatePtr input) {
  return health::mojom::InteractiveRoutineUpdate::New(
      Convert(input->user_message));
}

health::mojom::NonInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::NonInteractiveRoutineUpdatePtr input) {
  return health::mojom::NonInteractiveRoutineUpdate::New(
      Convert(input->status), std::move(input->status_message));
}

health::mojom::RunRoutineResponsePtr UncheckedConvertPtr(
    cros_healthd::mojom::RunRoutineResponsePtr input) {
  return health::mojom::RunRoutineResponse::New(input->id,
                                                Convert(input->status));
}

}  // namespace unchecked

base::Optional<health::mojom::DiagnosticRoutineEnum> Convert(
    cros_healthd::mojom::DiagnosticRoutineEnum input) {
  switch (input) {
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity:
      return health::mojom::DiagnosticRoutineEnum::kBatteryCapacity;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth:
      return health::mojom::DiagnosticRoutineEnum::kBatteryHealth;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck:
      return health::mojom::DiagnosticRoutineEnum::kSmartctlCheck;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower:
      return health::mojom::DiagnosticRoutineEnum::kAcPower;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache:
      return health::mojom::DiagnosticRoutineEnum::kCpuCache;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress:
      return health::mojom::DiagnosticRoutineEnum::kCpuStress;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy:
      return health::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel:
      return health::mojom::DiagnosticRoutineEnum::kNvmeWearLevel;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest:
      return health::mojom::DiagnosticRoutineEnum::kNvmeSelfTest;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead:
      return health::mojom::DiagnosticRoutineEnum::kDiskRead;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch:
      return health::mojom::DiagnosticRoutineEnum::kPrimeSearch;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge:
      return health::mojom::DiagnosticRoutineEnum::kBatteryDischarge;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge:
      return health::mojom::DiagnosticRoutineEnum::kBatteryCharge;
    default:
      return base::nullopt;
  }
}

std::vector<health::mojom::DiagnosticRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input) {
  std::vector<health::mojom::DiagnosticRoutineEnum> output;
  for (const auto element : input) {
    base::Optional<health::mojom::DiagnosticRoutineEnum> converted =
        Convert(element);
    if (converted.has_value()) {
      output.push_back(converted.value());
    }
  }
  return output;
}

health::mojom::DiagnosticRoutineUserMessageEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineUserMessageEnum input) {
  switch (input) {
    case cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::kUnplugACPower:
      return health::mojom::DiagnosticRoutineUserMessageEnum::kUnplugACPower;
    case cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::kPlugInACPower:
      return health::mojom::DiagnosticRoutineUserMessageEnum::kPlugInACPower;
  }
  NOTREACHED();
  return static_cast<health::mojom::DiagnosticRoutineUserMessageEnum>(
      static_cast<int>(
          health::mojom::DiagnosticRoutineUserMessageEnum::kMaxValue) +
      1);
}

health::mojom::DiagnosticRoutineStatusEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineStatusEnum input) {
  switch (input) {
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady:
      return health::mojom::DiagnosticRoutineStatusEnum::kReady;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRunning:
      return health::mojom::DiagnosticRoutineStatusEnum::kRunning;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kWaiting:
      return health::mojom::DiagnosticRoutineStatusEnum::kWaiting;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kPassed:
      return health::mojom::DiagnosticRoutineStatusEnum::kPassed;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kFailed:
      return health::mojom::DiagnosticRoutineStatusEnum::kFailed;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kError:
      return health::mojom::DiagnosticRoutineStatusEnum::kError;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kCancelled:
      return health::mojom::DiagnosticRoutineStatusEnum::kCancelled;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kFailedToStart:
      return health::mojom::DiagnosticRoutineStatusEnum::kFailedToStart;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRemoved:
      return health::mojom::DiagnosticRoutineStatusEnum::kRemoved;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kCancelling:
      return health::mojom::DiagnosticRoutineStatusEnum::kCancelling;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kUnsupported:
      return health::mojom::DiagnosticRoutineStatusEnum::kUnsupported;
  }
  NOTREACHED();
  return static_cast<health::mojom::DiagnosticRoutineStatusEnum>(
      static_cast<int>(health::mojom::DiagnosticRoutineStatusEnum::kMaxValue) +
      1);
}

cros_healthd::mojom::DiagnosticRoutineCommandEnum Convert(
    health::mojom::DiagnosticRoutineCommandEnum input) {
  switch (input) {
    case health::mojom::DiagnosticRoutineCommandEnum::kContinue:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kContinue;
    case health::mojom::DiagnosticRoutineCommandEnum::kCancel:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kCancel;
    case health::mojom::DiagnosticRoutineCommandEnum::kGetStatus:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kGetStatus;
    case health::mojom::DiagnosticRoutineCommandEnum::kRemove:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kRemove;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::DiagnosticRoutineCommandEnum>(
      static_cast<int>(
          cros_healthd::mojom::DiagnosticRoutineCommandEnum::kMaxValue) +
      1);
}

cros_healthd::mojom::AcPowerStatusEnum Convert(
    health::mojom::AcPowerStatusEnum input) {
  switch (input) {
    case health::mojom::AcPowerStatusEnum::kConnected:
      return cros_healthd::mojom::AcPowerStatusEnum::kConnected;
    case health::mojom::AcPowerStatusEnum::kDisconnected:
      return cros_healthd::mojom::AcPowerStatusEnum::kDisconnected;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::AcPowerStatusEnum>(
      static_cast<int>(cros_healthd::mojom::AcPowerStatusEnum::kMaxValue) + 1);
}

cros_healthd::mojom::NvmeSelfTestTypeEnum Convert(
    health::mojom::NvmeSelfTestTypeEnum input) {
  switch (input) {
    case health::mojom::NvmeSelfTestTypeEnum::kShortSelfTest:
      return cros_healthd::mojom::NvmeSelfTestTypeEnum::kShortSelfTest;
    case health::mojom::NvmeSelfTestTypeEnum::kLongSelfTest:
      return cros_healthd::mojom::NvmeSelfTestTypeEnum::kLongSelfTest;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::NvmeSelfTestTypeEnum>(
      static_cast<int>(cros_healthd::mojom::NvmeSelfTestTypeEnum::kMaxValue) +
      1);
}

cros_healthd::mojom::DiskReadRoutineTypeEnum Convert(
    health::mojom::DiskReadRoutineTypeEnum input) {
  switch (input) {
    case health::mojom::DiskReadRoutineTypeEnum::kLinearRead:
      return cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead;
    case health::mojom::DiskReadRoutineTypeEnum::kRandomRead:
      return cros_healthd::mojom::DiskReadRoutineTypeEnum::kRandomRead;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::DiskReadRoutineTypeEnum>(
      static_cast<int>(
          cros_healthd::mojom::DiskReadRoutineTypeEnum::kMaxValue) +
      1);
}

}  // namespace converters
}  // namespace chromeos
