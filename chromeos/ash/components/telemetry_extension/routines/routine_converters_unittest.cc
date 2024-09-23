// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/routines/routine_converters.h"

#include <iterator>
#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

// Tests that `ConvertRoutinePtr` function returns nullptr if input is
// nullptr. `ConvertRoutinePtr` is a template, so we can test this function
// with any valid type.
TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertRoutinePtrTakesNullPtr) {
  EXPECT_TRUE(
      ConvertRoutinePtr(crosapi::TelemetryDiagnosticRoutineArgumentPtr())
          .is_null());
}

TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertRoutineArgumentPtr) {
  auto input =
      crosapi::TelemetryDiagnosticRoutineArgument::NewUnrecognizedArgument(
          true);

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_unrecognizedArgument());
  EXPECT_TRUE(result->get_unrecognizedArgument());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertMemoryRoutineArgumentPtr) {
  constexpr uint32_t kMaxTestingMemKib = 42;

  auto input =
      crosapi::TelemetryDiagnosticMemoryRoutineArgument::New(kMaxTestingMemKib);

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->max_testing_mem_kib);
  EXPECT_EQ(*result->max_testing_mem_kib, kMaxTestingMemKib);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertVolumeButtonRoutineArgumentPtr) {
  constexpr auto kTimeout = base::Seconds(10);

  auto input = crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::New();
  input->type = crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::
      ButtonType::kVolumeUp;
  input->timeout = kTimeout;

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->type,
            healthd::VolumeButtonRoutineArgument::ButtonType::kVolumeUp);
  EXPECT_EQ(result->timeout, kTimeout);
}

TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertFanRoutineArgumentPtr) {
  auto input = crosapi::TelemetryDiagnosticFanRoutineArgument::New();

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertLedLitUpRoutineArgumentPtr) {
  auto input = crosapi::TelemetryDiagnosticLedLitUpRoutineArgument::New();
  input->name = crosapi::TelemetryDiagnosticLedName::kBattery;
  input->color = crosapi::TelemetryDiagnosticLedColor::kRed;

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->name, healthd::LedName::kBattery);
  EXPECT_EQ(result->color, healthd::LedColor::kRed);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertNetworkBandwidthRoutineArgumentPtr) {
  auto input =
      crosapi::TelemetryDiagnosticNetworkBandwidthRoutineArgument::New();

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertCameraFrameAnalysisRoutineArgumentPtr) {
  auto input =
      crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineArgument::New();

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertKeyboardBacklightRoutineArgumentPtr) {
  auto input =
      crosapi::TelemetryDiagnosticKeyboardBacklightRoutineArgument::New();

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertUnrecognizedRoutineInquiryReplyPtr) {
  auto input =
      crosapi::TelemetryDiagnosticRoutineInquiryReply::NewUnrecognizedReply(
          true);

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_unrecognizedReply());
  EXPECT_TRUE(result->get_unrecognizedReply());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertRoutineCheckLedLitUpStateInquiryReplyPtr) {
  auto input =
      crosapi::TelemetryDiagnosticRoutineInquiryReply::NewCheckLedLitUpState(
          crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::New());

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_check_led_lit_up_state());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertRoutineCheckKeyboardBacklightInquiryReplyPtr) {
  auto input = crosapi::TelemetryDiagnosticRoutineInquiryReply::
      NewCheckKeyboardBacklightState(
          crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::New());

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_check_keyboard_backlight_state());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateInitializedPtr) {
  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateInitialized::New()),
            crosapi::TelemetryDiagnosticRoutineStateInitialized::New());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateRunningPtr) {
  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateRunning::New()),
            crosapi::TelemetryDiagnosticRoutineStateRunning::New());
}

TEST(TelemetryDiagnosticRoutineConvertersTest, ConvertRoutineInquiryPtr) {
  auto input = healthd::RoutineInquiry::NewUnrecognizedInquiry(true);

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_unrecognizedInquiry());
  EXPECT_TRUE(result->get_unrecognizedInquiry());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertRoutineInquiryCheckLedLitUpStatePtr) {
  auto input = healthd::RoutineInquiry::NewCheckLedLitUpState(
      healthd::CheckLedLitUpStateInquiry::New());

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_check_led_lit_up_state());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertRoutineInquiryUnplugAcAdapterInquiryPtr) {
  auto input = healthd::RoutineInquiry::NewUnplugAcAdapterInquiry(
      healthd::UnplugAcAdapterInquiry::New());

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_unrecognizedInquiry());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertRoutineInquiryCheckKeyboardBacklightStatePtr) {
  auto input = healthd::RoutineInquiry::NewCheckKeyboardBacklightState(
      healthd::CheckKeyboardBacklightStateInquiry::New());

  const auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_check_keyboard_backlight_state());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateWaitingPtr) {
  constexpr char kMessage[] = "TEST";

  auto input = healthd::RoutineStateWaiting::New(
      healthd::RoutineStateWaiting::Reason::kWaitingToBeScheduled, kMessage,
      healthd::RoutineInteraction::NewUnrecognizedInteraction(true));

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->reason, crosapi::TelemetryDiagnosticRoutineStateWaiting::
                                Reason::kWaitingToBeScheduled);
  EXPECT_EQ(result->message, kMessage);
  ASSERT_TRUE(result->interaction);
  EXPECT_TRUE(result->interaction->is_unrecognizedInteraction());
  EXPECT_TRUE(result->interaction->get_unrecognizedInteraction());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticLedName) {
  EXPECT_EQ(healthd::LedName::kUnmappedEnumField,
            Convert(crosapi::TelemetryDiagnosticLedName::kUnmappedEnumField));
  EXPECT_EQ(healthd::LedName::kBattery,
            Convert(crosapi::TelemetryDiagnosticLedName::kBattery));
  EXPECT_EQ(healthd::LedName::kPower,
            Convert(crosapi::TelemetryDiagnosticLedName::kPower));
  EXPECT_EQ(healthd::LedName::kAdapter,
            Convert(crosapi::TelemetryDiagnosticLedName::kAdapter));
  EXPECT_EQ(healthd::LedName::kLeft,
            Convert(crosapi::TelemetryDiagnosticLedName::kLeft));
  EXPECT_EQ(healthd::LedName::kRight,
            Convert(crosapi::TelemetryDiagnosticLedName::kRight));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticLedColor) {
  EXPECT_EQ(healthd::LedColor::kUnmappedEnumField,
            Convert(crosapi::TelemetryDiagnosticLedColor::kUnmappedEnumField));
  EXPECT_EQ(healthd::LedColor::kRed,
            Convert(crosapi::TelemetryDiagnosticLedColor::kRed));
  EXPECT_EQ(healthd::LedColor::kGreen,
            Convert(crosapi::TelemetryDiagnosticLedColor::kGreen));
  EXPECT_EQ(healthd::LedColor::kBlue,
            Convert(crosapi::TelemetryDiagnosticLedColor::kBlue));
  EXPECT_EQ(healthd::LedColor::kYellow,
            Convert(crosapi::TelemetryDiagnosticLedColor::kYellow));
  EXPECT_EQ(healthd::LedColor::kWhite,
            Convert(crosapi::TelemetryDiagnosticLedColor::kWhite));
  EXPECT_EQ(healthd::LedColor::kAmber,
            Convert(crosapi::TelemetryDiagnosticLedColor::kAmber));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticCheckLedLitUpStateReplyState) {
  EXPECT_EQ(healthd::CheckLedLitUpStateReply::State::kUnmappedEnumField,
            Convert(crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
                        kUnmappedEnumField));
  EXPECT_EQ(healthd::CheckLedLitUpStateReply::State::kCorrectColor,
            Convert(crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
                        kCorrectColor));
  EXPECT_EQ(healthd::CheckLedLitUpStateReply::State::kNotLitUp,
            Convert(crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
                        kNotLitUp));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticCheckKeyboardBacklightStateReplyState) {
  EXPECT_EQ(
      healthd::CheckKeyboardBacklightStateReply::State::kUnmappedEnumField,
      Convert(crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::
                  State::kUnmappedEnumField));
  EXPECT_EQ(
      healthd::CheckKeyboardBacklightStateReply::State::kOk,
      Convert(crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::
                  State::kOk));
  EXPECT_EQ(
      healthd::CheckKeyboardBacklightStateReply::State::kAnyNotLitUp,
      Convert(crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::
                  State::kAnyNotLitUp));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticMemtesterTestItemEnum) {
  constexpr struct MemtesterEnums {
    healthd::MemtesterTestItemEnum healthd;
    crosapi::TelemetryDiagnosticMemtesterTestItemEnum crosapi;
  } enums[] = {
      {healthd::MemtesterTestItemEnum::kUnmappedEnumField,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnmappedEnumField},
      {healthd::MemtesterTestItemEnum::kUnknown,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnknown},
      {healthd::MemtesterTestItemEnum::kStuckAddress,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kStuckAddress},
      {healthd::MemtesterTestItemEnum::kCompareAND,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND},
      {healthd::MemtesterTestItemEnum::kCompareDIV,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV},
      {healthd::MemtesterTestItemEnum::kCompareMUL,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL},
      {healthd::MemtesterTestItemEnum::kCompareOR,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareOR},
      {healthd::MemtesterTestItemEnum::kCompareSUB,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB},
      {healthd::MemtesterTestItemEnum::kCompareXOR,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareXOR},
      {healthd::MemtesterTestItemEnum::kSequentialIncrement,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSequentialIncrement},
      {healthd::MemtesterTestItemEnum::kBitFlip,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip},
      {healthd::MemtesterTestItemEnum::kBitSpread,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread},
      {healthd::MemtesterTestItemEnum::kBlockSequential,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBlockSequential},
      {healthd::MemtesterTestItemEnum::kCheckerboard,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCheckerboard},
      {healthd::MemtesterTestItemEnum::kRandomValue,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kRandomValue},
      {healthd::MemtesterTestItemEnum::kSolidBits,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSolidBits},
      {healthd::MemtesterTestItemEnum::kWalkingOnes,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingOnes},
      {healthd::MemtesterTestItemEnum::kWalkingZeroes,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingZeroes},
      {healthd::MemtesterTestItemEnum::k8BitWrites,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites},
      {healthd::MemtesterTestItemEnum::k16BitWrites,
       crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSixteenBitWrites},
  };

  EXPECT_EQ(
      static_cast<uint32_t>(healthd::MemtesterTestItemEnum::kMaxValue) + 1,
      std::size(enums));

  for (const auto& enum_pair : enums) {
    EXPECT_EQ(Convert(enum_pair.healthd), enum_pair.crosapi);
  }
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticMemtesterResultPtr) {
  auto input = healthd::MemtesterResult::New();
  input->passed_items = {healthd::MemtesterTestItemEnum::k8BitWrites,
                         healthd::MemtesterTestItemEnum::k16BitWrites};
  input->failed_items = {healthd::MemtesterTestItemEnum::kBitFlip,
                         healthd::MemtesterTestItemEnum::kBitSpread};

  auto result = ConvertRoutinePtr(std::move(input));

  EXPECT_THAT(
      result->passed_items,
      testing::ElementsAre(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites,
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
              kSixteenBitWrites));

  EXPECT_THAT(
      result->failed_items,
      testing::ElementsAre(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip,
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticMemoryRoutineDetailPtr) {
  constexpr uint64_t kBytesTested = 42;

  auto mem_result = healthd::MemtesterResult::New();
  mem_result->passed_items = {healthd::MemtesterTestItemEnum::k8BitWrites,
                              healthd::MemtesterTestItemEnum::k16BitWrites};
  mem_result->failed_items = {healthd::MemtesterTestItemEnum::kBitFlip,
                              healthd::MemtesterTestItemEnum::kBitSpread};

  auto input = healthd::MemoryRoutineDetail::New();
  input->bytes_tested = kBytesTested;
  input->result = std::move(mem_result);

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->bytes_tested, kBytesTested);
  ASSERT_TRUE(result->result);
  EXPECT_THAT(
      result->result->passed_items,
      testing::ElementsAre(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites,
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
              kSixteenBitWrites));

  EXPECT_THAT(
      result->result->failed_items,
      testing::ElementsAre(
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip,
          crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticFanRoutineDetailPtr) {
  auto input = healthd::FanRoutineDetail::New();
  input->passed_fan_ids = {0};
  input->failed_fan_ids = {1};
  input->fan_count_status = healthd::HardwarePresenceStatus::kMatched;

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_THAT(result->passed_fan_ids, testing::ElementsAre(0));
  EXPECT_THAT(result->failed_fan_ids, testing::ElementsAre(1));
  EXPECT_EQ(result->fan_count_status,
            crosapi::TelemetryDiagnosticHardwarePresenceStatus::kMatched);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticNetworkBandwidthRoutineDetailPtr) {
  auto input = healthd::NetworkBandwidthRoutineDetail::New();
  input->download_speed_kbps = 123.0;
  input->upload_speed_kbps = 456.0;

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->download_speed_kbps, 123.0);
  EXPECT_EQ(result->upload_speed_kbps, 456.0);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticCameraFrameAnalysisRoutineDetailPtr) {
  auto input = healthd::CameraFrameAnalysisRoutineDetail::New();
  input->issue = healthd::CameraFrameAnalysisRoutineDetail::Issue::kNone;
  input->privacy_shutter_open_test = healthd::CameraSubtestResult::kPassed;
  input->lens_not_dirty_test = healthd::CameraSubtestResult::kFailed;

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->issue,
            crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
                Issue::kNone);
  EXPECT_EQ(result->privacy_shutter_open_test,
            crosapi::TelemetryDiagnosticCameraSubtestResult::kPassed);
  EXPECT_EQ(result->lens_not_dirty_test,
            crosapi::TelemetryDiagnosticCameraSubtestResult::kFailed);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineDetailPtr) {
  EXPECT_EQ(
      ConvertRoutinePtr(healthd::RoutineDetail::NewUnrecognizedArgument(true)),
      crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(true));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewMemory(
                healthd::MemoryRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
                crosapi::TelemetryDiagnosticMemoryRoutineDetail::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewFan(
                healthd::FanRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewFan(
                crosapi::TelemetryDiagnosticFanRoutineDetail::New()));

  EXPECT_EQ(
      ConvertRoutinePtr(healthd::RoutineDetail::NewNetworkBandwidth(
          healthd::NetworkBandwidthRoutineDetail::New())),
      crosapi::TelemetryDiagnosticRoutineDetail::NewNetworkBandwidth(
          crosapi::TelemetryDiagnosticNetworkBandwidthRoutineDetail::New()));

  EXPECT_EQ(
      ConvertRoutinePtr(healthd::RoutineDetail::NewCameraFrameAnalysis(
          healthd::CameraFrameAnalysisRoutineDetail::New())),
      crosapi::TelemetryDiagnosticRoutineDetail::NewCameraFrameAnalysis(
          crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewAudioDriver(
                healthd::AudioDriverRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewUfsLifetime(
                healthd::UfsLifetimeRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewBluetoothPower(
                healthd::BluetoothPowerRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewBluetoothDiscovery(
                healthd::BluetoothDiscoveryRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewBluetoothScanning(
                healthd::BluetoothScanningRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewBluetoothPairing(
                healthd::BluetoothPairingRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewCameraAvailability(
                healthd::CameraAvailabilityRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewSensitiveSensor(
                healthd::SensitiveSensorRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineDetail::NewBatteryDischarge(
                healthd::BatteryDischargeRoutineDetail::New())),
            crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
                false));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateFinishedPtr) {
  auto routine_detail = healthd::RoutineDetail::NewUnrecognizedArgument(true);

  auto input = healthd::RoutineStateFinished::New();
  input->has_passed = false;
  input->detail = std::move(routine_detail);

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->has_passed);

  auto detail_result = std::move(result->detail);

  ASSERT_TRUE(detail_result);
  ASSERT_TRUE(detail_result->is_unrecognizedArgument());

  EXPECT_TRUE(detail_result->get_unrecognizedArgument());
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStateUnionPtr) {
  EXPECT_EQ(
      ConvertRoutinePtr(
          healthd::RoutineStateUnion::NewUnrecognizedArgument(true)),
      crosapi::TelemetryDiagnosticRoutineStateUnion::NewUnrecognizedArgument(
          true));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewInitialized(
                healthd::RoutineStateInitialized::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
                crosapi::TelemetryDiagnosticRoutineStateInitialized::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewRunning(
                healthd::RoutineStateRunning::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
                crosapi::TelemetryDiagnosticRoutineStateRunning::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewWaiting(
                healthd::RoutineStateWaiting::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewWaiting(
                crosapi::TelemetryDiagnosticRoutineStateWaiting::New()));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineStateUnion::NewFinished(
                healthd::RoutineStateFinished::New())),
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New()));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineStatePtr) {
  constexpr uint8_t kPercentage = 50;

  auto input = healthd::RoutineState::New();
  input->percentage = kPercentage;
  input->state_union = healthd::RoutineStateUnion::NewRunning(
      healthd::RoutineStateRunning::New());

  auto result = ConvertRoutinePtr(std::move(input));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->percentage, kPercentage);
  EXPECT_EQ(result->state_union,
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
                crosapi::TelemetryDiagnosticRoutineStateRunning::New()));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticRoutineRunningInfoPtr) {
  EXPECT_EQ(
      ConvertRoutinePtr(
          healthd::RoutineRunningInfo::NewUnrecognizedArgument(true)),
      crosapi::TelemetryDiagnosticRoutineRunningInfo::NewUnrecognizedArgument(
          true));

  EXPECT_EQ(ConvertRoutinePtr(healthd::RoutineRunningInfo::NewNetworkBandwidth(
                healthd::NetworkBandwidthRoutineRunningInfo::New())),
            crosapi::TelemetryDiagnosticRoutineRunningInfo::NewNetworkBandwidth(
                crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::
                    New()));
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticNetworkBandwidthRoutineRunningInfoPtr) {
  auto input = healthd::NetworkBandwidthRoutineRunningInfo::New();
  input->type = healthd::NetworkBandwidthRoutineRunningInfo::Type::kDownload;
  input->speed_kbps = 100.0;

  auto result = ConvertRoutinePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->type,
            crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::
                Type::kDownload);
  EXPECT_EQ(result->speed_kbps, 100.0);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticCameraFrameAnalysisRoutineDetailIssue) {
  EXPECT_EQ(
      Convert(
          healthd::CameraFrameAnalysisRoutineDetail::Issue::kUnmappedEnumField),
      crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue::
          kUnmappedEnumField);
  EXPECT_EQ(Convert(healthd::CameraFrameAnalysisRoutineDetail::Issue::kNone),
            crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
                Issue::kNone);
  EXPECT_EQ(Convert(healthd::CameraFrameAnalysisRoutineDetail::Issue::
                        kCameraServiceNotAvailable),
            crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
                Issue::kCameraServiceNotAvailable);
  EXPECT_EQ(Convert(healthd::CameraFrameAnalysisRoutineDetail::Issue::
                        kBlockedByPrivacyShutter),
            crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
                Issue::kBlockedByPrivacyShutter);
  EXPECT_EQ(
      Convert(healthd::CameraFrameAnalysisRoutineDetail::Issue::kLensAreDirty),
      crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue::
          kLensAreDirty);
}

TEST(TelemetryDiagnosticRoutineConvertersTest,
     ConvertTelemetryDiagnosticCameraSubtestResult) {
  EXPECT_EQ(
      Convert(healthd::CameraSubtestResult::kUnmappedEnumField),
      crosapi::TelemetryDiagnosticCameraSubtestResult::kUnmappedEnumField);
  EXPECT_EQ(Convert(healthd::CameraSubtestResult::kNotRun),
            crosapi::TelemetryDiagnosticCameraSubtestResult::kNotRun);
  EXPECT_EQ(Convert(healthd::CameraSubtestResult::kPassed),
            crosapi::TelemetryDiagnosticCameraSubtestResult::kPassed);
  EXPECT_EQ(Convert(healthd::CameraSubtestResult::kFailed),
            crosapi::TelemetryDiagnosticCameraSubtestResult::kFailed);
}

}  // namespace ash::converters
