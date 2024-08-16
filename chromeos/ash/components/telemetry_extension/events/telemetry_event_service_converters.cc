// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_converters.h"

#include <utility>
#include <vector>

#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "base/notreached.h"
#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_keyboard_event.mojom.h"

namespace ash::converters::events {

namespace unchecked {

crosapi::mojom::UInt32ValuePtr LegacyUncheckedConvertPtr(
    cros_healthd::mojom::NullableUint32Ptr input) {
  return crosapi::mojom::UInt32Value::New(input->value);
}

std::optional<uint32_t> UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint32Ptr input) {
  return input->value;
}

crosapi::mojom::TelemetryKeyboardInfoPtr UncheckedConvertPtr(
    diagnostics::mojom::KeyboardInfoPtr input) {
  auto result = crosapi::mojom::TelemetryKeyboardInfo::New();
  result->id = crosapi::mojom::UInt32Value::New(input->id);
  result->connection_type = Convert(input->connection_type);
  result->name = input->name;
  result->physical_layout = Convert(input->physical_layout);
  result->mechanical_layout = Convert(input->mechanical_layout);
  result->region_code = input->region_code;
  result->number_pad_present = Convert(input->number_pad_present);
  result->top_row_keys =
      ConvertVector<crosapi::mojom::TelemetryKeyboardTopRowKey>(
          input->top_row_keys);
  result->top_right_key = Convert(input->top_right_key);
  result->has_assistant_key =
      crosapi::mojom::BoolValue::New(input->has_assistant_key);
  return result;
}

crosapi::mojom::TelemetryKeyboardDiagnosticEventInfoPtr UncheckedConvertPtr(
    diagnostics::mojom::KeyboardDiagnosticEventInfoPtr input) {
  auto result = crosapi::mojom::TelemetryKeyboardDiagnosticEventInfo::New();
  result->keyboard_info = ConvertStructPtr(std::move(input->keyboard_info));
  result->tested_keys = std::move(input->tested_keys);
  result->tested_top_row_keys = std::move(input->tested_top_row_keys);

  return result;
}

crosapi::mojom::TelemetryAudioJackEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioJackEventInfoPtr input) {
  return crosapi::mojom::TelemetryAudioJackEventInfo::New(
      Convert(input->state), Convert(input->device_type));
}

crosapi::mojom::TelemetryLidEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LidEventInfoPtr input) {
  return crosapi::mojom::TelemetryLidEventInfo::New(Convert(input->state));
}

crosapi::mojom::TelemetryUsbEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::UsbEventInfoPtr input) {
  return crosapi::mojom::TelemetryUsbEventInfo::New(
      input->vendor, input->name, input->vid, input->pid, input->categories,
      Convert(input->state));
}

crosapi::mojom::TelemetryExternalDisplayEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::ExternalDisplayEventInfoPtr input) {
  return crosapi::mojom::TelemetryExternalDisplayEventInfo::New(
      Convert(input->state), ash::converters::telemetry::ConvertProbePtr(
                                 std::move(input->display_info)));
}

crosapi::mojom::TelemetrySdCardEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::SdCardEventInfoPtr input) {
  return crosapi::mojom::TelemetrySdCardEventInfo::New(Convert(input->state));
}

crosapi::mojom::TelemetryPowerEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PowerEventInfoPtr input) {
  return crosapi::mojom::TelemetryPowerEventInfo::New(Convert(input->state));
}

crosapi::mojom::TelemetryStylusGarageEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StylusGarageEventInfoPtr input) {
  return crosapi::mojom::TelemetryStylusGarageEventInfo::New(
      Convert(input->state));
}

crosapi::mojom::TelemetryTouchPointInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TouchPointInfoPtr input) {
  auto result = crosapi::mojom::TelemetryTouchPointInfo::New();
  result->tracking_id = input->tracking_id;
  result->x = input->x;
  result->y = input->y;
  result->pressure = LegacyConvertStructPtr(std::move(input->pressure));
  result->touch_major = LegacyConvertStructPtr(std::move(input->touch_major));
  result->touch_minor = LegacyConvertStructPtr(std::move(input->touch_minor));
  return result;
}

crosapi::mojom::TelemetryTouchpadButtonEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TouchpadButtonEventPtr input) {
  auto pressed =
      input->pressed
          ? crosapi::mojom::TelemetryTouchpadButtonEventInfo_State::kPressed
          : crosapi::mojom::TelemetryTouchpadButtonEventInfo_State::kReleased;
  return crosapi::mojom::TelemetryTouchpadButtonEventInfo::New(
      Convert(input->button), pressed);
}

crosapi::mojom::TelemetryTouchpadTouchEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TouchpadTouchEventPtr input) {
  std::vector<crosapi::mojom::TelemetryTouchPointInfoPtr>
      converted_touch_points;
  for (auto& touch_point : input->touch_points) {
    converted_touch_points.push_back(ConvertStructPtr(std::move(touch_point)));
  }
  return crosapi::mojom::TelemetryTouchpadTouchEventInfo::New(
      std::move(converted_touch_points));
}

crosapi::mojom::TelemetryTouchpadConnectedEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TouchpadConnectedEventPtr input) {
  std::vector<crosapi::mojom::TelemetryInputTouchButton>
      converted_touch_buttons;
  for (const auto& touch_button : input->buttons) {
    converted_touch_buttons.push_back(Convert(touch_button));
  }
  return crosapi::mojom::TelemetryTouchpadConnectedEventInfo::New(
      input->max_x, input->max_y, input->max_pressure,
      std::move(converted_touch_buttons));
}

crosapi::mojom::TelemetryTouchscreenTouchEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TouchscreenTouchEventPtr input) {
  std::vector<crosapi::mojom::TelemetryTouchPointInfoPtr>
      converted_touch_points;
  for (auto& touch_point : input->touch_points) {
    converted_touch_points.push_back(ConvertStructPtr(std::move(touch_point)));
  }
  return crosapi::mojom::TelemetryTouchscreenTouchEventInfo::New(
      std::move(converted_touch_points));
}

crosapi::mojom::TelemetryTouchscreenConnectedEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TouchscreenConnectedEventPtr input) {
  return crosapi::mojom::TelemetryTouchscreenConnectedEventInfo::New(
      input->max_x, input->max_y, input->max_pressure);
}

crosapi::mojom::TelemetryStylusTouchEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StylusTouchEventPtr input) {
  return crosapi::mojom::TelemetryStylusTouchEventInfo::New(
      ConvertStructPtr(std::move(input->touch_point)));
}

crosapi::mojom::TelemetryStylusConnectedEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StylusConnectedEventPtr input) {
  return crosapi::mojom::TelemetryStylusConnectedEventInfo::New(
      input->max_x, input->max_y, input->max_pressure);
}

crosapi::mojom::TelemetryStylusTouchPointInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StylusTouchPointInfoPtr input) {
  return crosapi::mojom::TelemetryStylusTouchPointInfo::New(
      input->x, input->y, ConvertStructPtr(std::move(input->pressure)));
}

crosapi::mojom::TelemetryEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::EventInfoPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::EventInfo::Tag::kAudioJackEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
          ConvertStructPtr(std::move(input->get_audio_jack_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kLidEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewLidEventInfo(
          ConvertStructPtr(std::move(input->get_lid_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kUsbEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewUsbEventInfo(
          ConvertStructPtr(std::move(input->get_usb_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kExternalDisplayEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewExternalDisplayEventInfo(
          ConvertStructPtr(
              std::move(input->get_external_display_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kSdCardEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewSdCardEventInfo(
          ConvertStructPtr(std::move(input->get_sd_card_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kPowerEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewPowerEventInfo(
          ConvertStructPtr(std::move(input->get_power_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kKeyboardDiagnosticEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewKeyboardDiagnosticEventInfo(
          ConvertStructPtr(
              std::move(input->get_keyboard_diagnostic_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kStylusGarageEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewStylusGarageEventInfo(
          ConvertStructPtr(std::move(input->get_stylus_garage_event_info())));
    case cros_healthd::mojom::EventInfo::Tag::kTouchpadEventInfo: {
      auto info = std::move(input->get_touchpad_event_info());
      switch (info->which()) {
        case cros_healthd::mojom::TouchpadEventInfo::Tag::kButtonEvent:
          return crosapi::mojom::TelemetryEventInfo::NewTouchpadButtonEventInfo(
              ConvertStructPtr(std::move(info->get_button_event())));
        case cros_healthd::mojom::TouchpadEventInfo::Tag::kTouchEvent:
          return crosapi::mojom::TelemetryEventInfo::NewTouchpadTouchEventInfo(
              ConvertStructPtr(std::move(info->get_touch_event())));
        case cros_healthd::mojom::TouchpadEventInfo::Tag::kConnectedEvent:
          return crosapi::mojom::TelemetryEventInfo::
              NewTouchpadConnectedEventInfo(
                  ConvertStructPtr(std::move(info->get_connected_event())));
        case cros_healthd::mojom::TouchpadEventInfo::Tag::kDefaultType:
          LOG(WARNING) << "Got unsupported touchpad event";
          return nullptr;
      }
    }
    case cros_healthd::mojom::EventInfo::Tag::kTouchscreenEventInfo: {
      auto info = std::move(input->get_touchscreen_event_info());
      switch (info->which()) {
        case cros_healthd::mojom::TouchscreenEventInfo::Tag::kTouchEvent:
          return crosapi::mojom::TelemetryEventInfo::
              NewTouchscreenTouchEventInfo(
                  ConvertStructPtr(std::move(info->get_touch_event())));
        case cros_healthd::mojom::TouchscreenEventInfo::Tag::kConnectedEvent:
          return crosapi::mojom::TelemetryEventInfo::
              NewTouchscreenConnectedEventInfo(
                  ConvertStructPtr(std::move(info->get_connected_event())));
        case cros_healthd::mojom::TouchscreenEventInfo::Tag::kDefaultType:
          LOG(WARNING) << "Got unsupported touchscreen event";
          return nullptr;
      }
    }
    case cros_healthd::mojom::EventInfo::Tag::kStylusEventInfo: {
      auto info = std::move(input->get_stylus_event_info());
      switch (info->which()) {
        case cros_healthd::mojom::StylusEventInfo::Tag::kTouchEvent:
          return crosapi::mojom::TelemetryEventInfo::NewStylusTouchEventInfo(
              ConvertStructPtr(std::move(info->get_touch_event())));
        case cros_healthd::mojom::StylusEventInfo::Tag::kConnectedEvent:
          return crosapi::mojom::TelemetryEventInfo::
              NewStylusConnectedEventInfo(
                  ConvertStructPtr(std::move(info->get_connected_event())));
        case cros_healthd::mojom::StylusEventInfo::Tag::kDefaultType:
          LOG(WARNING) << "Got unsupported touchpad event";
          return nullptr;
      }
    }
    default:
      LOG(WARNING) << "Got event for unsupported category";
      return nullptr;
  }
}

}  // namespace unchecked

crosapi::mojom::TelemetryKeyboardConnectionType Convert(
    diagnostics::mojom::ConnectionType input) {
  switch (input) {
    case diagnostics::mojom::ConnectionType::kUnmappedEnumField:
      return crosapi::mojom::TelemetryKeyboardConnectionType::
          kUnmappedEnumField;
    case diagnostics::mojom::ConnectionType::kInternal:
      return crosapi::mojom::TelemetryKeyboardConnectionType::kInternal;
    case diagnostics::mojom::ConnectionType::kUsb:
      return crosapi::mojom::TelemetryKeyboardConnectionType::kUsb;
    case diagnostics::mojom::ConnectionType::kBluetooth:
      return crosapi::mojom::TelemetryKeyboardConnectionType::kBluetooth;
    case diagnostics::mojom::ConnectionType::kUnknown:
      return crosapi::mojom::TelemetryKeyboardConnectionType::kUnknown;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryKeyboardPhysicalLayout Convert(
    diagnostics::mojom::PhysicalLayout input) {
  switch (input) {
    case diagnostics::mojom::PhysicalLayout::kUnmappedEnumField:
      return crosapi::mojom::TelemetryKeyboardPhysicalLayout::
          kUnmappedEnumField;
    case diagnostics::mojom::PhysicalLayout::kUnknown:
      return crosapi::mojom::TelemetryKeyboardPhysicalLayout::kUnknown;
    case diagnostics::mojom::PhysicalLayout::kChromeOS:
      return crosapi::mojom::TelemetryKeyboardPhysicalLayout::kChromeOS;
    case diagnostics::mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco:
      return crosapi::mojom::TelemetryKeyboardPhysicalLayout::kUnknown;
    case diagnostics::mojom::PhysicalLayout::kChromeOSDellEnterpriseDrallion:
      return crosapi::mojom::TelemetryKeyboardPhysicalLayout::kUnknown;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryKeyboardMechanicalLayout Convert(
    diagnostics::mojom::MechanicalLayout input) {
  switch (input) {
    case diagnostics::mojom::MechanicalLayout::kUnmappedEnumField:
      return crosapi::mojom::TelemetryKeyboardMechanicalLayout::
          kUnmappedEnumField;
    case diagnostics::mojom::MechanicalLayout::kUnknown:
      return crosapi::mojom::TelemetryKeyboardMechanicalLayout::kUnknown;
    case diagnostics::mojom::MechanicalLayout::kAnsi:
      return crosapi::mojom::TelemetryKeyboardMechanicalLayout::kAnsi;
    case diagnostics::mojom::MechanicalLayout::kIso:
      return crosapi::mojom::TelemetryKeyboardMechanicalLayout::kIso;
    case diagnostics::mojom::MechanicalLayout::kJis:
      return crosapi::mojom::TelemetryKeyboardMechanicalLayout::kJis;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryKeyboardNumberPadPresence Convert(
    diagnostics::mojom::NumberPadPresence input) {
  switch (input) {
    case diagnostics::mojom::NumberPadPresence::kUnmappedEnumField:
      return crosapi::mojom::TelemetryKeyboardNumberPadPresence::
          kUnmappedEnumField;
    case diagnostics::mojom::NumberPadPresence::kUnknown:
      return crosapi::mojom::TelemetryKeyboardNumberPadPresence::kUnknown;
    case diagnostics::mojom::NumberPadPresence::kPresent:
      return crosapi::mojom::TelemetryKeyboardNumberPadPresence::kPresent;
    case diagnostics::mojom::NumberPadPresence::kNotPresent:
      return crosapi::mojom::TelemetryKeyboardNumberPadPresence::kNotPresent;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryKeyboardTopRowKey Convert(
    diagnostics::mojom::TopRowKey input) {
  switch (input) {
    case diagnostics::mojom::TopRowKey::kUnmappedEnumField:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kUnmappedEnumField;
    case diagnostics::mojom::TopRowKey::kNone:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kNone;
    case diagnostics::mojom::TopRowKey::kUnknown:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kUnknown;
    case diagnostics::mojom::TopRowKey::kBack:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kBack;
    case diagnostics::mojom::TopRowKey::kForward:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kForward;
    case diagnostics::mojom::TopRowKey::kRefresh:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kRefresh;
    case diagnostics::mojom::TopRowKey::kFullscreen:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kFullscreen;
    case diagnostics::mojom::TopRowKey::kOverview:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kOverview;
    case diagnostics::mojom::TopRowKey::kScreenshot:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenshot;
    case diagnostics::mojom::TopRowKey::kScreenBrightnessDown:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenBrightnessDown;
    case diagnostics::mojom::TopRowKey::kScreenBrightnessUp:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenBrightnessUp;
    case diagnostics::mojom::TopRowKey::kPrivacyScreenToggle:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kPrivacyScreenToggle;
    case diagnostics::mojom::TopRowKey::kMicrophoneMute:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kMicrophoneMute;
    case diagnostics::mojom::TopRowKey::kVolumeMute:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kVolumeMute;
    case diagnostics::mojom::TopRowKey::kVolumeDown:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kVolumeDown;
    case diagnostics::mojom::TopRowKey::kVolumeUp:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kVolumeUp;
    case diagnostics::mojom::TopRowKey::kKeyboardBacklightToggle:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::
          kKeyboardBacklightToggle;
    case diagnostics::mojom::TopRowKey::kKeyboardBacklightDown:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kKeyboardBacklightDown;
    case diagnostics::mojom::TopRowKey::kKeyboardBacklightUp:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kKeyboardBacklightUp;
    case diagnostics::mojom::TopRowKey::kNextTrack:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kNextTrack;
    case diagnostics::mojom::TopRowKey::kPreviousTrack:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kPreviousTrack;
    case diagnostics::mojom::TopRowKey::kPlayPause:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kPlayPause;
    case diagnostics::mojom::TopRowKey::kScreenMirror:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kScreenMirror;
    case diagnostics::mojom::TopRowKey::kDelete:
      return crosapi::mojom::TelemetryKeyboardTopRowKey::kDelete;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryKeyboardTopRightKey Convert(
    diagnostics::mojom::TopRightKey input) {
  switch (input) {
    case diagnostics::mojom::TopRightKey::kUnmappedEnumField:
      return crosapi::mojom::TelemetryKeyboardTopRightKey::kUnmappedEnumField;
    case diagnostics::mojom::TopRightKey::kUnknown:
      return crosapi::mojom::TelemetryKeyboardTopRightKey::kUnknown;
    case diagnostics::mojom::TopRightKey::kPower:
      return crosapi::mojom::TelemetryKeyboardTopRightKey::kPower;
    case diagnostics::mojom::TopRightKey::kLock:
      return crosapi::mojom::TelemetryKeyboardTopRightKey::kLock;
    case diagnostics::mojom::TopRightKey::kControlPanel:
      return crosapi::mojom::TelemetryKeyboardTopRightKey::kControlPanel;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryAudioJackEventInfo::State Convert(
    cros_healthd::mojom::AudioJackEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::AudioJackEventInfo_State::kUnmappedEnumField:
      return crosapi::mojom::TelemetryAudioJackEventInfo::State::
          kUnmappedEnumField;
    case cros_healthd::mojom::AudioJackEventInfo_State::kAdd:
      return crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;
    case cros_healthd::mojom::AudioJackEventInfo_State::kRemove:
      return crosapi::mojom::TelemetryAudioJackEventInfo::State::kRemove;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType Convert(
    cros_healthd::mojom::AudioJackEventInfo::DeviceType input) {
  switch (input) {
    case cros_healthd::mojom::AudioJackEventInfo_DeviceType::kUnmappedEnumField:
      return crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::
          kUnmappedEnumField;
    case cros_healthd::mojom::AudioJackEventInfo_DeviceType::kHeadphone:
      return crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::
          kHeadphone;
    case cros_healthd::mojom::AudioJackEventInfo_DeviceType::kMicrophone:
      return crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::
          kMicrophone;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryLidEventInfo::State Convert(
    cros_healthd::mojom::LidEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::LidEventInfo_State::kUnmappedEnumField:
      return crosapi::mojom::TelemetryLidEventInfo::State::kUnmappedEnumField;
    case cros_healthd::mojom::LidEventInfo_State::kClosed:
      return crosapi::mojom::TelemetryLidEventInfo::State::kClosed;
    case cros_healthd::mojom::LidEventInfo_State::kOpened:
      return crosapi::mojom::TelemetryLidEventInfo::State::kOpened;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryUsbEventInfo::State Convert(
    cros_healthd::mojom::UsbEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::UsbEventInfo_State::kUnmappedEnumField:
      return crosapi::mojom::TelemetryUsbEventInfo::State::kUnmappedEnumField;
    case cros_healthd::mojom::UsbEventInfo_State::kAdd:
      return crosapi::mojom::TelemetryUsbEventInfo::State::kAdd;
    case cros_healthd::mojom::UsbEventInfo_State::kRemove:
      return crosapi::mojom::TelemetryUsbEventInfo::State::kRemove;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryExternalDisplayEventInfo::State Convert(
    cros_healthd::mojom::ExternalDisplayEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::ExternalDisplayEventInfo_State::
        kUnmappedEnumField:
      return crosapi::mojom::TelemetryExternalDisplayEventInfo::State::
          kUnmappedEnumField;
    case cros_healthd::mojom::ExternalDisplayEventInfo_State::kAdd:
      return crosapi::mojom::TelemetryExternalDisplayEventInfo::State::kAdd;
    case cros_healthd::mojom::ExternalDisplayEventInfo_State::kRemove:
      return crosapi::mojom::TelemetryExternalDisplayEventInfo::State::kRemove;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetrySdCardEventInfo::State Convert(
    cros_healthd::mojom::SdCardEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::SdCardEventInfo_State::kUnmappedEnumField:
      return crosapi::mojom::TelemetrySdCardEventInfo::State::
          kUnmappedEnumField;
    case cros_healthd::mojom::SdCardEventInfo_State::kAdd:
      return crosapi::mojom::TelemetrySdCardEventInfo::State::kAdd;
    case cros_healthd::mojom::SdCardEventInfo_State::kRemove:
      return crosapi::mojom::TelemetrySdCardEventInfo::State::kRemove;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryPowerEventInfo::State Convert(
    cros_healthd::mojom::PowerEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::PowerEventInfo_State::kUnmappedEnumField:
      return crosapi::mojom::TelemetryPowerEventInfo::State::kUnmappedEnumField;
    case cros_healthd::mojom::PowerEventInfo_State::kAcInserted:
      return crosapi::mojom::TelemetryPowerEventInfo::State::kAcInserted;
    case cros_healthd::mojom::PowerEventInfo_State::kAcRemoved:
      return crosapi::mojom::TelemetryPowerEventInfo::State::kAcRemoved;
    case cros_healthd::mojom::PowerEventInfo_State::kOsSuspend:
      return crosapi::mojom::TelemetryPowerEventInfo::State::kOsSuspend;
    case cros_healthd::mojom::PowerEventInfo_State::kOsResume:
      return crosapi::mojom::TelemetryPowerEventInfo::State::kOsResume;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryStylusGarageEventInfo::State Convert(
    cros_healthd::mojom::StylusGarageEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::StylusGarageEventInfo_State::kUnmappedEnumField:
      return crosapi::mojom::TelemetryStylusGarageEventInfo::State::
          kUnmappedEnumField;
    case cros_healthd::mojom::StylusGarageEventInfo_State::kInserted:
      return crosapi::mojom::TelemetryStylusGarageEventInfo::State::kInserted;
    case cros_healthd::mojom::StylusGarageEventInfo_State::kRemoved:
      return crosapi::mojom::TelemetryStylusGarageEventInfo::State::kRemoved;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryInputTouchButton Convert(
    cros_healthd::mojom::InputTouchButton input) {
  switch (input) {
    case cros_healthd::mojom::InputTouchButton::kUnmappedEnumField:
      return crosapi::mojom::TelemetryInputTouchButton::kUnmappedEnumField;
    case cros_healthd::mojom::InputTouchButton::kLeft:
      return crosapi::mojom::TelemetryInputTouchButton::kLeft;
    case cros_healthd::mojom::InputTouchButton::kMiddle:
      return crosapi::mojom::TelemetryInputTouchButton::kMiddle;
    case cros_healthd::mojom::InputTouchButton::kRight:
      return crosapi::mojom::TelemetryInputTouchButton::kRight;
  }
  NOTREACHED();
}

cros_healthd::mojom::EventCategoryEnum Convert(
    crosapi::mojom::TelemetryEventCategoryEnum input) {
  switch (input) {
    case crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField:
      return cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField;
    case crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack:
      return cros_healthd::mojom::EventCategoryEnum::kAudioJack;
    case crosapi::mojom::TelemetryEventCategoryEnum::kLid:
      return cros_healthd::mojom::EventCategoryEnum::kLid;
    case crosapi::mojom::TelemetryEventCategoryEnum::kUsb:
      return cros_healthd::mojom::EventCategoryEnum::kUsb;
    case crosapi::mojom::TelemetryEventCategoryEnum::kSdCard:
      return cros_healthd::mojom::EventCategoryEnum::kSdCard;
    case crosapi::mojom::TelemetryEventCategoryEnum::kPower:
      return cros_healthd::mojom::EventCategoryEnum::kPower;
    case crosapi::mojom::TelemetryEventCategoryEnum::kKeyboardDiagnostic:
      return cros_healthd::mojom::EventCategoryEnum::kKeyboardDiagnostic;
    case crosapi::mojom::TelemetryEventCategoryEnum::kStylusGarage:
      return cros_healthd::mojom::EventCategoryEnum::kStylusGarage;
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadButton:
      return cros_healthd::mojom::EventCategoryEnum::kTouchpad;
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadTouch:
      return cros_healthd::mojom::EventCategoryEnum::kTouchpad;
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadConnected:
      return cros_healthd::mojom::EventCategoryEnum::kTouchpad;
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchscreenTouch:
      return cros_healthd::mojom::EventCategoryEnum::kTouchscreen;
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchscreenConnected:
      return cros_healthd::mojom::EventCategoryEnum::kTouchscreen;
    case crosapi::mojom::TelemetryEventCategoryEnum::kExternalDisplay:
      return cros_healthd::mojom::EventCategoryEnum::kExternalDisplay;
    case crosapi::mojom::TelemetryEventCategoryEnum::kStylusTouch:
      return cros_healthd::mojom::EventCategoryEnum::kStylus;
    case crosapi::mojom::TelemetryEventCategoryEnum::kStylusConnected:
      return cros_healthd::mojom::EventCategoryEnum::kStylus;
  }
  NOTREACHED();
}

}  // namespace ash::converters::events
