// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/routines/routine_converters.h"

#include <utility>

#include "base/notreached.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace ash::converters {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

namespace unchecked {

crosapi::TelemetryDiagnosticMemtesterResultPtr UncheckedConvertPtr(
    healthd::MemtesterResultPtr input) {
  return crosapi::TelemetryDiagnosticMemtesterResult::New(
      ConvertVector(input->passed_items), ConvertVector(input->failed_items));
}

crosapi::TelemetryDiagnosticMemoryRoutineDetailPtr UncheckedConvertPtr(
    healthd::MemoryRoutineDetailPtr input) {
  return crosapi::TelemetryDiagnosticMemoryRoutineDetail::New(
      input->bytes_tested, ConvertRoutinePtr(std::move(input->result)));
}

crosapi::TelemetryDiagnosticFanRoutineDetailPtr UncheckedConvertPtr(
    healthd::FanRoutineDetailPtr input) {
  return crosapi::TelemetryDiagnosticFanRoutineDetail::New(
      input->passed_fan_ids, input->failed_fan_ids,
      Convert(input->fan_count_status));
}

crosapi::TelemetryDiagnosticRoutineStateInitializedPtr UncheckedConvertPtr(
    healthd::RoutineStateInitializedPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateInitialized::New();
}

crosapi::TelemetryDiagnosticNetworkBandwidthRoutineDetailPtr
UncheckedConvertPtr(healthd::NetworkBandwidthRoutineDetailPtr input) {
  auto detail =
      crosapi::TelemetryDiagnosticNetworkBandwidthRoutineDetail::New();
  detail->download_speed_kbps = input->download_speed_kbps;
  detail->upload_speed_kbps = input->upload_speed_kbps;
  return detail;
}

crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetailPtr
UncheckedConvertPtr(healthd::CameraFrameAnalysisRoutineDetailPtr input) {
  auto detail =
      crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::New();
  detail->issue = Convert(input->issue);
  detail->privacy_shutter_open_test = Convert(input->privacy_shutter_open_test);
  detail->lens_not_dirty_test = Convert(input->lens_not_dirty_test);
  return detail;
}

crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfoPtr
UncheckedConvertPtr(healthd::NetworkBandwidthRoutineRunningInfoPtr input) {
  return crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::New(
      Convert(input->type), input->speed_kbps);
}

crosapi::TelemetryDiagnosticRoutineRunningInfoPtr UncheckedConvertPtr(
    healthd::RoutineRunningInfoPtr input) {
  switch (input->which()) {
    case healthd::RoutineRunningInfo::Tag::kUnrecognizedArgument:
      return crosapi::TelemetryDiagnosticRoutineRunningInfo::
          NewUnrecognizedArgument(input->get_unrecognizedArgument());
    case healthd::RoutineRunningInfo::Tag::kNetworkBandwidth:
      return crosapi::TelemetryDiagnosticRoutineRunningInfo::
          NewNetworkBandwidth(
              ConvertRoutinePtr(std::move(input->get_network_bandwidth())));
  }
}

crosapi::TelemetryDiagnosticRoutineStateRunningPtr UncheckedConvertPtr(
    healthd::RoutineStateRunningPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateRunning::New(
      ConvertRoutinePtr(std::move(input->info)));
}

crosapi::TelemetryDiagnosticCheckLedLitUpStateInquiryPtr UncheckedConvertPtr(
    healthd::CheckLedLitUpStateInquiryPtr input) {
  return crosapi::TelemetryDiagnosticCheckLedLitUpStateInquiry::New();
}

crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateInquiryPtr
UncheckedConvertPtr(healthd::CheckKeyboardBacklightStateInquiryPtr input) {
  return crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateInquiry::New();
}

crosapi::TelemetryDiagnosticRoutineInquiryPtr UncheckedConvertPtr(
    healthd::RoutineInquiryPtr input) {
  switch (input->which()) {
    case healthd::RoutineInquiry::Tag::kUnrecognizedInquiry:
      return crosapi::TelemetryDiagnosticRoutineInquiry::NewUnrecognizedInquiry(
          input->get_unrecognizedInquiry());
    case healthd::RoutineInquiry::Tag::kCheckLedLitUpState:
      return crosapi::TelemetryDiagnosticRoutineInquiry::NewCheckLedLitUpState(
          ConvertRoutinePtr(std::move(input->get_check_led_lit_up_state())));
    case healthd::RoutineInquiry::Tag::kCheckKeyboardBacklightState:
      return crosapi::TelemetryDiagnosticRoutineInquiry::
          NewCheckKeyboardBacklightState(ConvertRoutinePtr(
              std::move(input->get_check_keyboard_backlight_state())));
    // The following routines have not been added to crosapi yet.
    case healthd::RoutineInquiry::Tag::kUnplugAcAdapterInquiry:
      return crosapi::TelemetryDiagnosticRoutineInquiry::NewUnrecognizedInquiry(
          /*unrecognizedArgument=*/false);
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticRoutineInteractionPtr UncheckedConvertPtr(
    healthd::RoutineInteractionPtr input) {
  switch (input->which()) {
    case healthd::RoutineInteraction::Tag::kUnrecognizedInteraction:
      return crosapi::TelemetryDiagnosticRoutineInteraction::
          NewUnrecognizedInteraction(input->get_unrecognizedInteraction());
    case healthd::RoutineInteraction::Tag::kInquiry:
      return crosapi::TelemetryDiagnosticRoutineInteraction::NewInquiry(
          ConvertRoutinePtr(std::move(input->get_inquiry())));
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticRoutineStateWaitingPtr UncheckedConvertPtr(
    healthd::RoutineStateWaitingPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateWaiting::New(
      Convert(input->reason), input->message,
      ConvertRoutinePtr(std::move(input->interaction)));
}

crosapi::TelemetryDiagnosticRoutineDetailPtr UncheckedConvertPtr(
    healthd::RoutineDetailPtr input) {
  switch (input->which()) {
    case healthd::RoutineDetail::Tag::kUnrecognizedArgument:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
          input->get_unrecognizedArgument());
    case healthd::RoutineDetail::Tag::kMemory:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
          ConvertRoutinePtr(std::move(input->get_memory())));
    case healthd::RoutineDetail::Tag::kFan:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewFan(
          ConvertRoutinePtr(std::move(input->get_fan())));
    case healthd::RoutineDetail::Tag::kNetworkBandwidth:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewNetworkBandwidth(
          ConvertRoutinePtr(std::move(input->get_network_bandwidth())));
    case healthd::RoutineDetail::Tag::kCameraFrameAnalysis:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewCameraFrameAnalysis(
          ConvertRoutinePtr(std::move(input->get_camera_frame_analysis())));
    // The following routines have not been added to crosapi yet.
    case healthd::RoutineDetail::Tag::kAudioDriver:
    case healthd::RoutineDetail::Tag::kUfsLifetime:
    case healthd::RoutineDetail::Tag::kBluetoothPower:
    case healthd::RoutineDetail::Tag::kBluetoothDiscovery:
    case healthd::RoutineDetail::Tag::kBluetoothScanning:
    case healthd::RoutineDetail::Tag::kBluetoothPairing:
    case healthd::RoutineDetail::Tag::kCameraAvailability:
    case healthd::RoutineDetail::Tag::kSensitiveSensor:
    case healthd::RoutineDetail::Tag::kBatteryDischarge:
      // The actual value of unrecognizedArgument should not be used. Assign an
      // arbitrary value to it.
      return crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
          /*unrecognizedArgument=*/false);
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticRoutineStateFinishedPtr UncheckedConvertPtr(
    healthd::RoutineStateFinishedPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateFinished::New(
      input->has_passed, ConvertRoutinePtr(std::move(input->detail)));
}

crosapi::TelemetryDiagnosticRoutineStateUnionPtr UncheckedConvertPtr(
    healthd::RoutineStateUnionPtr input) {
  switch (input->which()) {
    case healthd::RoutineStateUnion::Tag::kUnrecognizedArgument:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::
          NewUnrecognizedArgument(input->get_unrecognizedArgument());
    case healthd::RoutineStateUnion::Tag::kInitialized:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
          ConvertRoutinePtr(std::move(input->get_initialized())));
    case healthd::RoutineStateUnion::Tag::kRunning:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
          ConvertRoutinePtr(std::move(input->get_running())));
    case healthd::RoutineStateUnion::Tag::kWaiting:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewWaiting(
          ConvertRoutinePtr(std::move(input->get_waiting())));
    case healthd::RoutineStateUnion::Tag::kFinished:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
          ConvertRoutinePtr(std::move(input->get_finished())));
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticRoutineStatePtr UncheckedConvertPtr(
    healthd::RoutineStatePtr input) {
  return crosapi::TelemetryDiagnosticRoutineState::New(
      input->percentage, ConvertRoutinePtr(std::move(input->state_union)));
}

healthd::RoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr input) {
  switch (input->which()) {
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::
        kUnrecognizedArgument:
      return healthd::RoutineArgument::NewUnrecognizedArgument(
          std::move(input->get_unrecognizedArgument()));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory:
      return healthd::RoutineArgument::NewMemory(
          ConvertRoutinePtr(std::move(input->get_memory())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kVolumeButton:
      return healthd::RoutineArgument::NewVolumeButton(
          ConvertRoutinePtr(std::move(input->get_volume_button())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kFan:
      return healthd::RoutineArgument::NewFan(
          ConvertRoutinePtr(std::move(input->get_fan())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kLedLitUp:
      return healthd::RoutineArgument::NewLedLitUp(
          ConvertRoutinePtr(std::move(input->get_led_lit_up())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kNetworkBandwidth:
      return healthd::RoutineArgument::NewNetworkBandwidth(
          ConvertRoutinePtr(std::move(input->get_network_bandwidth())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kCameraFrameAnalysis:
      return healthd::RoutineArgument::NewCameraFrameAnalysis(
          ConvertRoutinePtr(std::move(input->get_camera_frame_analysis())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kKeyboardBacklight:
      return healthd::RoutineArgument::NewKeyboardBacklight(
          ConvertRoutinePtr(std::move(input->get_keyboard_backlight())));
  }
}

healthd::MemoryRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemoryRoutineArgumentPtr input) {
  return healthd::MemoryRoutineArgument::New(
      std::move(input->max_testing_mem_kib));
}

healthd::VolumeButtonRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticVolumeButtonRoutineArgumentPtr input) {
  auto arg = healthd::VolumeButtonRoutineArgument::New();
  switch (input->type) {
    case crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType::
        kUnmappedEnumField:
      arg->type =
          healthd::VolumeButtonRoutineArgument::ButtonType::kUnmappedEnumField;
      break;
    case crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType::
        kVolumeUp:
      arg->type = healthd::VolumeButtonRoutineArgument::ButtonType::kVolumeUp;
      break;
    case crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType::
        kVolumeDown:
      arg->type = healthd::VolumeButtonRoutineArgument::ButtonType::kVolumeDown;
      break;
  }
  arg->timeout = input->timeout;
  return arg;
}

healthd::FanRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticFanRoutineArgumentPtr input) {
  return healthd::FanRoutineArgument::New();
}

healthd::LedLitUpRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticLedLitUpRoutineArgumentPtr input) {
  auto arg = healthd::LedLitUpRoutineArgument::New();
  arg->name = Convert(input->name);
  arg->color = Convert(input->color);
  return arg;
}

healthd::CheckLedLitUpStateReplyPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticCheckLedLitUpStateReplyPtr input) {
  auto arg = healthd::CheckLedLitUpStateReply::New();
  arg->state = Convert(input->state);
  return arg;
}

healthd::CheckKeyboardBacklightStateReplyPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReplyPtr input) {
  auto arg = healthd::CheckKeyboardBacklightStateReply::New();
  arg->state = Convert(input->state);
  return arg;
}

healthd::NetworkBandwidthRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticNetworkBandwidthRoutineArgumentPtr input) {
  return healthd::NetworkBandwidthRoutineArgument::New();
}

healthd::CameraFrameAnalysisRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineArgumentPtr input) {
  return healthd::CameraFrameAnalysisRoutineArgument::New();
}

healthd::KeyboardBacklightRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticKeyboardBacklightRoutineArgumentPtr input) {
  return healthd::KeyboardBacklightRoutineArgument::New();
}

healthd::RoutineInquiryReplyPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineInquiryReplyPtr input) {
  switch (input->which()) {
    case crosapi::TelemetryDiagnosticRoutineInquiryReply::Tag::
        kUnrecognizedReply:
      return healthd::RoutineInquiryReply::NewUnrecognizedReply(
          input->get_unrecognizedReply());
    case crosapi::TelemetryDiagnosticRoutineInquiryReply::Tag::
        kCheckLedLitUpState:
      return healthd::RoutineInquiryReply::NewCheckLedLitUpState(
          ConvertRoutinePtr(std::move(input->get_check_led_lit_up_state())));
    case crosapi::TelemetryDiagnosticRoutineInquiryReply::Tag::
        kCheckKeyboardBacklightState:
      return healthd::RoutineInquiryReply::NewCheckKeyboardBacklightState(
          ConvertRoutinePtr(
              std::move(input->get_check_keyboard_backlight_state())));
  }
  NOTREACHED();
}

}  // namespace unchecked

healthd::LedName Convert(crosapi::TelemetryDiagnosticLedName input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticLedName::kUnmappedEnumField:
      return healthd::LedName::kUnmappedEnumField;
    case crosapi::TelemetryDiagnosticLedName::kBattery:
      return healthd::LedName::kBattery;
    case crosapi::TelemetryDiagnosticLedName::kPower:
      return healthd::LedName::kPower;
    case crosapi::TelemetryDiagnosticLedName::kAdapter:
      return healthd::LedName::kAdapter;
    case crosapi::TelemetryDiagnosticLedName::kLeft:
      return healthd::LedName::kLeft;
    case crosapi::TelemetryDiagnosticLedName::kRight:
      return healthd::LedName::kRight;
  }
  NOTREACHED();
}

healthd::LedColor Convert(crosapi::TelemetryDiagnosticLedColor input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticLedColor::kUnmappedEnumField:
      return healthd::LedColor::kUnmappedEnumField;
    case crosapi::TelemetryDiagnosticLedColor::kRed:
      return healthd::LedColor::kRed;
    case crosapi::TelemetryDiagnosticLedColor::kGreen:
      return healthd::LedColor::kGreen;
    case crosapi::TelemetryDiagnosticLedColor::kBlue:
      return healthd::LedColor::kBlue;
    case crosapi::TelemetryDiagnosticLedColor::kYellow:
      return healthd::LedColor::kYellow;
    case crosapi::TelemetryDiagnosticLedColor::kWhite:
      return healthd::LedColor::kWhite;
    case crosapi::TelemetryDiagnosticLedColor::kAmber:
      return healthd::LedColor::kAmber;
  }
  NOTREACHED();
}

healthd::CheckLedLitUpStateReply::State Convert(
    crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
        kUnmappedEnumField:
      return healthd::CheckLedLitUpStateReply::State::kUnmappedEnumField;
    case crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
        kCorrectColor:
      return healthd::CheckLedLitUpStateReply::State::kCorrectColor;
    case crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::kNotLitUp:
      return healthd::CheckLedLitUpStateReply::State::kNotLitUp;
  }
  NOTREACHED();
}

healthd::CheckKeyboardBacklightStateReply::State Convert(
    crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State::
        kUnmappedEnumField:
      return healthd::CheckKeyboardBacklightStateReply::State::
          kUnmappedEnumField;
    case crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State::
        kOk:
      return healthd::CheckKeyboardBacklightStateReply::State::kOk;
    case crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State::
        kAnyNotLitUp:
      return healthd::CheckKeyboardBacklightStateReply::State::kAnyNotLitUp;
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticMemtesterTestItemEnum Convert(
    healthd::MemtesterTestItemEnum input) {
  switch (input) {
    case healthd::MemtesterTestItemEnum::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kUnmappedEnumField;
    case healthd::MemtesterTestItemEnum::kUnknown:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnknown;
    case healthd::MemtesterTestItemEnum::kStuckAddress:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kStuckAddress;
    case healthd::MemtesterTestItemEnum::kCompareAND:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND;
    case healthd::MemtesterTestItemEnum::kCompareDIV:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV;
    case healthd::MemtesterTestItemEnum::kCompareMUL:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL;
    case healthd::MemtesterTestItemEnum::kCompareOR:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareOR;
    case healthd::MemtesterTestItemEnum::kCompareSUB:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB;
    case healthd::MemtesterTestItemEnum::kCompareXOR:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareXOR;
    case healthd::MemtesterTestItemEnum::kSequentialIncrement:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kSequentialIncrement;
    case healthd::MemtesterTestItemEnum::kBitFlip:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip;
    case healthd::MemtesterTestItemEnum::kBitSpread:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread;
    case healthd::MemtesterTestItemEnum::kBlockSequential:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kBlockSequential;
    case healthd::MemtesterTestItemEnum::kCheckerboard:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCheckerboard;
    case healthd::MemtesterTestItemEnum::kRandomValue:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kRandomValue;
    case healthd::MemtesterTestItemEnum::kSolidBits:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSolidBits;
    case healthd::MemtesterTestItemEnum::kWalkingOnes:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingOnes;
    case healthd::MemtesterTestItemEnum::kWalkingZeroes:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingZeroes;
    case healthd::MemtesterTestItemEnum::k8BitWrites:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites;
    case healthd::MemtesterTestItemEnum::k16BitWrites:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kSixteenBitWrites;
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticHardwarePresenceStatus Convert(
    healthd::HardwarePresenceStatus input) {
  switch (input) {
    case healthd::HardwarePresenceStatus::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::
          kUnmappedEnumField;
    case healthd::HardwarePresenceStatus::kMatched:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::kMatched;
    case healthd::HardwarePresenceStatus::kNotMatched:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::kNotMatched;
    case healthd::HardwarePresenceStatus::kNotConfigured:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::kNotConfigured;
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason Convert(
    healthd::RoutineStateWaiting::Reason input) {
  switch (input) {
    case healthd::RoutineStateWaiting_Reason::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
          kUnmappedEnumField;
    case healthd::RoutineStateWaiting_Reason::kWaitingToBeScheduled:
      return crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
          kWaitingToBeScheduled;
    case healthd::RoutineStateWaiting_Reason::kWaitingInteraction:
      return crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
          kWaitingForInteraction;
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::Type Convert(
    healthd::NetworkBandwidthRoutineRunningInfo::Type input) {
  switch (input) {
    case healthd::NetworkBandwidthRoutineRunningInfo::Type::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::
          Type::kUnmappedEnumField;
    case healthd::NetworkBandwidthRoutineRunningInfo::Type::kDownload:
      return crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::
          Type::kDownload;
    case healthd::NetworkBandwidthRoutineRunningInfo::Type::kUpload:
      return crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::
          Type::kUpload;
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue Convert(
    healthd::CameraFrameAnalysisRoutineDetail::Issue input) {
  switch (input) {
    case healthd::CameraFrameAnalysisRoutineDetail::Issue::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
          Issue::kUnmappedEnumField;
    case healthd::CameraFrameAnalysisRoutineDetail::Issue::kNone:
      return crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
          Issue::kNone;
    case healthd::CameraFrameAnalysisRoutineDetail::Issue::
        kCameraServiceNotAvailable:
      return crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
          Issue::kCameraServiceNotAvailable;
    case healthd::CameraFrameAnalysisRoutineDetail::Issue::
        kBlockedByPrivacyShutter:
      return crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
          Issue::kBlockedByPrivacyShutter;
    case healthd::CameraFrameAnalysisRoutineDetail::Issue::kLensAreDirty:
      return crosapi::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::
          Issue::kLensAreDirty;
  }
  NOTREACHED();
}

crosapi::TelemetryDiagnosticCameraSubtestResult Convert(
    healthd::CameraSubtestResult input) {
  switch (input) {
    case healthd::CameraSubtestResult::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticCameraSubtestResult::
          kUnmappedEnumField;
    case healthd::CameraSubtestResult::kNotRun:
      return crosapi::TelemetryDiagnosticCameraSubtestResult::kNotRun;
    case healthd::CameraSubtestResult::kPassed:
      return crosapi::TelemetryDiagnosticCameraSubtestResult::kPassed;
    case healthd::CameraSubtestResult::kFailed:
      return crosapi::TelemetryDiagnosticCameraSubtestResult::kFailed;
  }
  NOTREACHED();
}

}  // namespace ash::converters
