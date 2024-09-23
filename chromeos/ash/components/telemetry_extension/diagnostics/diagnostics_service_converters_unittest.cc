// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_converters.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters::diagnostics {

// Tests that |ConvertDiagnosticsPtr| function returns nullptr if input is
// nullptr. ConvertDiagnosticsPtr is a template, so we can test this function
// with any valid type.
TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticsPtrTakesNullPtr) {
  EXPECT_TRUE(
      ConvertDiagnosticsPtr(cros_healthd::mojom::InteractiveRoutineUpdatePtr())
          .is_null());
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticRoutineEnum) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kAcPower),
            crosapi::DiagnosticsRoutineEnum::kAcPower);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBatteryCapacity),
            crosapi::DiagnosticsRoutineEnum::kBatteryCapacity);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBatteryHealth),
            crosapi::DiagnosticsRoutineEnum::kBatteryHealth);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBatteryDischarge),
            crosapi::DiagnosticsRoutineEnum::kBatteryDischarge);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBatteryCharge),
            crosapi::DiagnosticsRoutineEnum::kBatteryCharge);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kCpuCache),
            crosapi::DiagnosticsRoutineEnum::kCpuCache);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kCpuStress),
            crosapi::DiagnosticsRoutineEnum::kCpuStress);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kDiskRead),
            crosapi::DiagnosticsRoutineEnum::kDiskRead);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kDnsResolution),
            crosapi::DiagnosticsRoutineEnum::kDnsResolution);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kDnsResolverPresent),
            crosapi::DiagnosticsRoutineEnum::kDnsResolverPresent);
  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineEnum::kFloatingPointAccuracy),
      crosapi::DiagnosticsRoutineEnum::kFloatingPointAccuracy);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kGatewayCanBePinged),
            crosapi::DiagnosticsRoutineEnum::kGatewayCanBePinged);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kLanConnectivity),
            crosapi::DiagnosticsRoutineEnum::kLanConnectivity);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kMemory),
            crosapi::DiagnosticsRoutineEnum::kMemory);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kNvmeSelfTest),
            crosapi::DiagnosticsRoutineEnum::kNvmeSelfTest);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kPrimeSearch),
            crosapi::DiagnosticsRoutineEnum::kPrimeSearch);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kSignalStrength),
            crosapi::DiagnosticsRoutineEnum::kSignalStrength);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kSensitiveSensor),
            crosapi::DiagnosticsRoutineEnum::kSensitiveSensor);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kFingerprintAlive),
            crosapi::DiagnosticsRoutineEnum::kFingerprintAlive);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::
                        kSmartctlCheckWithPercentageUsed),
            crosapi::DiagnosticsRoutineEnum::kSmartctlCheckWithPercentageUsed);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kEmmcLifetime),
            crosapi::DiagnosticsRoutineEnum::kEmmcLifetime);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBluetoothPower),
            crosapi::DiagnosticsRoutineEnum::kBluetoothPower);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kUfsLifetime),
            crosapi::DiagnosticsRoutineEnum::kUfsLifetime);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kPowerButton),
            crosapi::DiagnosticsRoutineEnum::kPowerButton);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kAudioDriver),
            crosapi::DiagnosticsRoutineEnum::kAudioDriver);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBluetoothDiscovery),
            crosapi::DiagnosticsRoutineEnum::kBluetoothDiscovery);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBluetoothScanning),
            crosapi::DiagnosticsRoutineEnum::kBluetoothScanning);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kBluetoothPairing),
            crosapi::DiagnosticsRoutineEnum::kBluetoothPairing);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kFan),
            crosapi::DiagnosticsRoutineEnum::kFan);

  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineEnum::DEPRECATED_kNvmeWearLevel),
      crosapi::DiagnosticsRoutineEnum::DEPRECATED_kNvmeWearLevel);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineEnum::kArcHttp),
            std::nullopt);
}

// This test checks, that successful conversion of all
// `cros_health::DiagnosticRoutineEnum` variants match to all
// `crosapi::DiagnosticsRoutineEnum` variants. If this test fails, the
// corresponding crosapi variant was added, but no conversion from a
// cros_healthd variant.
TEST(DiagnosticsServiceConvertersTest, CheckAllVariantsCovered) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  // Create a vector of all conversions by converting all possible
  // cros_healthd variants.
  std::vector<crosapi::DiagnosticsRoutineEnum> found_conversions;
  for (auto iter = cros_healthd::DiagnosticRoutineEnum::kMinValue;
       iter <= cros_healthd::DiagnosticRoutineEnum::kMaxValue;
       iter = static_cast<cros_healthd::DiagnosticRoutineEnum>(
           static_cast<int>(iter) + 1)) {
    auto converted = Convert(iter);
    if (converted) {
      found_conversions.push_back(converted.value());
    }
  }

  // Assure that each crosapi variant is part of the conversions.
  for (auto iter = crosapi::DiagnosticsRoutineEnum::kMinValue;
       iter <= crosapi::DiagnosticsRoutineEnum::kMaxValue;
       iter = static_cast<crosapi::DiagnosticsRoutineEnum>(
           static_cast<int>(iter) + 1)) {
    EXPECT_THAT(found_conversions, testing::Contains(iter));
  }
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticRoutineStatusEnum) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kReady),
            crosapi::DiagnosticsRoutineStatusEnum::kReady);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kRunning),
            crosapi::DiagnosticsRoutineStatusEnum::kRunning);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kWaiting),
            crosapi::DiagnosticsRoutineStatusEnum::kWaiting);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kPassed),
            crosapi::DiagnosticsRoutineStatusEnum::kPassed);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kFailed),
            crosapi::DiagnosticsRoutineStatusEnum::kFailed);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kError),
            crosapi::DiagnosticsRoutineStatusEnum::kError);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kCancelled),
            crosapi::DiagnosticsRoutineStatusEnum::kCancelled);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kFailedToStart),
            crosapi::DiagnosticsRoutineStatusEnum::kFailedToStart);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kRemoved),
            crosapi::DiagnosticsRoutineStatusEnum::kRemoved);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kCancelling),
            crosapi::DiagnosticsRoutineStatusEnum::kCancelling);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kUnsupported),
            crosapi::DiagnosticsRoutineStatusEnum::kUnsupported);
  EXPECT_EQ(Convert(cros_healthd::DiagnosticRoutineStatusEnum::kNotRun),
            crosapi::DiagnosticsRoutineStatusEnum::kNotRun);
}

TEST(DiagnosticsServiceConvertersTest,
     ConvertDiagnosticRoutineUserMessageEnum) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineUserMessageEnum::kUnplugACPower),
      crosapi::DiagnosticsRoutineUserMessageEnum::kUnplugACPower);
  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineUserMessageEnum::kPlugInACPower),
      crosapi::DiagnosticsRoutineUserMessageEnum::kPlugInACPower);
  // LED routine is not yet supported in telemetry extension.
  EXPECT_EQ(
      Convert(cros_healthd::DiagnosticRoutineUserMessageEnum::kCheckLedColor),
      crosapi::DiagnosticsRoutineUserMessageEnum::kUnknown);
  EXPECT_EQ(
      Convert(
          cros_healthd::DiagnosticRoutineUserMessageEnum::kPressPowerButton),
      crosapi::DiagnosticsRoutineUserMessageEnum::kPressPowerButton);
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiagnosticRoutineCommandEnum) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kUnknown),
            cros_healthd::DiagnosticRoutineCommandEnum::kUnknown);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kContinue),
            cros_healthd::DiagnosticRoutineCommandEnum::kContinue);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kCancel),
            cros_healthd::DiagnosticRoutineCommandEnum::kCancel);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kGetStatus),
            cros_healthd::DiagnosticRoutineCommandEnum::kGetStatus);
  EXPECT_EQ(Convert(crosapi::DiagnosticsRoutineCommandEnum::kRemove),
            cros_healthd::DiagnosticRoutineCommandEnum::kRemove);
}

TEST(DiagnosticsServiceConvertersTest, ConvertAcPowerStatusEnum) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsAcPowerStatusEnum::kUnknown),
            cros_healthd::AcPowerStatusEnum::kUnknown);
  EXPECT_EQ(Convert(crosapi::DiagnosticsAcPowerStatusEnum::kConnected),
            cros_healthd::AcPowerStatusEnum::kConnected);
  EXPECT_EQ(Convert(crosapi::DiagnosticsAcPowerStatusEnum::kDisconnected),
            cros_healthd::AcPowerStatusEnum::kDisconnected);
}

TEST(DiagnosticsServiceConvertersTest, ConvertNvmeSelfTestTypeEnum) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsNvmeSelfTestTypeEnum::kUnknown),
            cros_healthd::NvmeSelfTestTypeEnum::kUnknown);
  EXPECT_EQ(Convert(crosapi::DiagnosticsNvmeSelfTestTypeEnum::kShortSelfTest),
            cros_healthd::NvmeSelfTestTypeEnum::kShortSelfTest);
  EXPECT_EQ(Convert(crosapi::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest),
            cros_healthd::NvmeSelfTestTypeEnum::kLongSelfTest);
}

TEST(DiagnosticsServiceConvertersTest, ConvertDiskReadRoutineTypeEnum) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(Convert(crosapi::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead),
            cros_healthd::DiskReadRoutineTypeEnum::kLinearRead);
  EXPECT_EQ(Convert(crosapi::DiagnosticsDiskReadRoutineTypeEnum::kRandomRead),
            cros_healthd::DiskReadRoutineTypeEnum::kRandomRead);
}

TEST(DiagnosticsServiceConvertersTest, ConvertUInt32ValuePtr) {
  namespace cros_healthd = cros_healthd::mojom;
  namespace crosapi = ::crosapi::mojom;

  EXPECT_EQ(ConvertDiagnosticsPtr(crosapi::UInt32ValuePtr()),
            cros_healthd::NullableUint32Ptr());
  EXPECT_EQ(ConvertDiagnosticsPtr(crosapi::UInt32Value::New(42)),
            cros_healthd::NullableUint32::New(42));
}

}  // namespace ash::converters::diagnostics
