// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_forwarder.h"

#include <utility>

#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

CrosHealthdEventForwarder::CrosHealthdEventForwarder(
    mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> crosapi_remote,
    crosapi::mojom::TelemetryEventCategoryEnum category)
    : category_(category), remote_(std::move(crosapi_remote)) {}

CrosHealthdEventForwarder::~CrosHealthdEventForwarder() = default;

void CrosHealthdEventForwarder::OnEvent(
    cros_healthd::mojom::EventInfoPtr info) {
  auto event = converters::events::ConvertStructPtr(std::move(info));
  switch (category_) {
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadButton: {
      if (event->is_touchpad_button_event_info()) {
        remote_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadTouch: {
      if (event->is_touchpad_touch_event_info()) {
        remote_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadConnected: {
      if (event->is_touchpad_connected_event_info()) {
        remote_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchscreenTouch: {
      if (event->is_touchscreen_touch_event_info()) {
        remote_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchscreenConnected: {
      if (event->is_touchscreen_connected_event_info()) {
        remote_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kStylusTouch: {
      if (event->is_stylus_touch_event_info()) {
        remote_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kStylusConnected: {
      if (event->is_stylus_connected_event_info()) {
        remote_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack:
    case crosapi::mojom::TelemetryEventCategoryEnum::kLid:
    case crosapi::mojom::TelemetryEventCategoryEnum::kUsb:
    case crosapi::mojom::TelemetryEventCategoryEnum::kSdCard:
    case crosapi::mojom::TelemetryEventCategoryEnum::kPower:
    case crosapi::mojom::TelemetryEventCategoryEnum::kKeyboardDiagnostic:
    case crosapi::mojom::TelemetryEventCategoryEnum::kStylusGarage:
    case crosapi::mojom::TelemetryEventCategoryEnum::kExternalDisplay: {
      remote_->OnEvent(std::move(event));
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField: {
      LOG(ERROR) << "Unrecognized event category";
      return;
    }
  }
}

mojo::Remote<crosapi::mojom::TelemetryEventObserver>&
CrosHealthdEventForwarder::GetRemote() {
  return remote_;
}

}  // namespace ash
