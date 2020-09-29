// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/diagnostics_service_converters.h"

#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace converters {

TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticRoutineStatusEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace health = ::chromeos::health::mojom;

  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kReady),
            health::DiagnosticRoutineStatusEnum::kReady);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kRunning),
            health::DiagnosticRoutineStatusEnum::kRunning);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kWaiting),
            health::DiagnosticRoutineStatusEnum::kWaiting);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kPassed),
            health::DiagnosticRoutineStatusEnum::kPassed);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kFailed),
            health::DiagnosticRoutineStatusEnum::kFailed);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kError),
            health::DiagnosticRoutineStatusEnum::kError);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kCancelled),
            health::DiagnosticRoutineStatusEnum::kCancelled);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kFailedToStart),
            health::DiagnosticRoutineStatusEnum::kFailedToStart);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kRemoved),
            health::DiagnosticRoutineStatusEnum::kRemoved);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kCancelling),
            health::DiagnosticRoutineStatusEnum::kCancelling);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kUnsupported),
            health::DiagnosticRoutineStatusEnum::kUnsupported);
}

TEST(DiagnosticsServiceConvertersTest,
     ConvertDiagnosticRoutineUserMessageEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace health = ::chromeos::health::mojom;

  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineUserMessageEnum::kUnplugACPower),
      health::DiagnosticRoutineUserMessageEnum::kUnplugACPower);
  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineUserMessageEnum::kPlugInACPower),
      health::DiagnosticRoutineUserMessageEnum::kPlugInACPower);
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticRoutineCommandEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace health = ::chromeos::health::mojom;

  EXPECT_EQ(Convert(health::DiagnosticRoutineCommandEnum::kContinue),
            cros_healthd::DiagnosticRoutineCommandEnum::kContinue);
  EXPECT_EQ(Convert(health::DiagnosticRoutineCommandEnum::kCancel),
            cros_healthd::DiagnosticRoutineCommandEnum::kCancel);
  EXPECT_EQ(Convert(health::DiagnosticRoutineCommandEnum::kGetStatus),
            cros_healthd::DiagnosticRoutineCommandEnum::kGetStatus);
  EXPECT_EQ(Convert(health::DiagnosticRoutineCommandEnum::kRemove),
            cros_healthd::DiagnosticRoutineCommandEnum::kRemove);
}

TEST(DiagnosticsServiceConvertersTest, ConvertAcPowerStatusEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace health = ::chromeos::health::mojom;

  EXPECT_EQ(Convert(health::AcPowerStatusEnum::kConnected),
            cros_healthd::AcPowerStatusEnum::kConnected);
  EXPECT_EQ(Convert(health::AcPowerStatusEnum::kDisconnected),
            cros_healthd::AcPowerStatusEnum::kDisconnected);
}

TEST(DiagnosticsServiceConvertersTest, ConvertNvmeSelfTestTypeEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace health = ::chromeos::health::mojom;

  EXPECT_EQ(Convert(health::NvmeSelfTestTypeEnum::kShortSelfTest),
            cros_healthd::NvmeSelfTestTypeEnum::kShortSelfTest);
  EXPECT_EQ(Convert(health::NvmeSelfTestTypeEnum::kLongSelfTest),
            cros_healthd::NvmeSelfTestTypeEnum::kLongSelfTest);
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiskReadRoutineTypeEnum) {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;
  namespace health = ::chromeos::health::mojom;

  EXPECT_EQ(Convert(health::DiskReadRoutineTypeEnum::kLinearRead),
            cros_healthd::DiskReadRoutineTypeEnum::kLinearRead);
  EXPECT_EQ(Convert(health::DiskReadRoutineTypeEnum::kRandomRead),
            cros_healthd::DiskReadRoutineTypeEnum::kRandomRead);
}

}  // namespace converters
}  // namespace chromeos
