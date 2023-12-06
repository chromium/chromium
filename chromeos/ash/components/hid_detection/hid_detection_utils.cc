// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/hid_detection_utils.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::hid_detection {
namespace {

using InputDeviceType = device::mojom::InputDeviceType;

std::optional<HidType> GetHidType(
    const device::mojom::InputDeviceInfo& device) {
  if (device.is_touchscreen || device.is_tablet)
    return HidType::kTouchscreen;

  if (IsDevicePointer(device)) {
    switch (device.type) {
      case InputDeviceType::TYPE_BLUETOOTH:
        return HidType::kBluetoothPointer;
      case InputDeviceType::TYPE_USB:
        return HidType::kUsbPointer;
      case InputDeviceType::TYPE_SERIO:
        return HidType::kSerialPointer;
      case InputDeviceType::TYPE_UNKNOWN:
        return HidType::kUnknownPointer;
    }
  }

  if (device.is_keyboard) {
    switch (device.type) {
      case InputDeviceType::TYPE_BLUETOOTH:
        return HidType::kBluetoothKeyboard;
      case InputDeviceType::TYPE_USB:
        return HidType::kUsbKeyboard;
      case InputDeviceType::TYPE_SERIO:
        return HidType::kSerialKeyboard;
      case InputDeviceType::TYPE_UNKNOWN:
        return HidType::kUnknownKeyboard;
    }
  }

  return std::nullopt;
}

}  // namespace

bool IsDevicePointer(const device::mojom::InputDeviceInfo& device) {
  return device.is_mouse || device.is_touchpad;
}

bool IsDeviceTouchscreen(const device::mojom::InputDeviceInfo& device) {
  return device.is_touchscreen || device.is_tablet;
}

void RecordHidConnected(const device::mojom::InputDeviceInfo& device) {
  std::optional<HidType> hid_type = GetHidType(device);

  // If |device| is not relevant (i.e. an accelerometer, joystick, etc), don't
  // emit metric.
  if (!hid_type.has_value()) {
    HID_LOG(DEBUG) << "HidConnected not logged for device " << device.id
                   << " because it doesn't have a relevant device type.";
    return;
  }

  base::UmaHistogramEnumeration("OOBE.HidDetectionScreen.HidConnected",
                                hid_type.value());
}

void RecordHidDisconnected(const device::mojom::InputDeviceInfo& device) {
  std::optional<HidType> hid_type = GetHidType(device);

  // If |device| is not relevant (i.e. an accelerometer, joystick, etc), don't
  // emit metric.
  if (!hid_type.has_value()) {
    HID_LOG(DEBUG) << "HidDisconnected not logged for device " << device.id
                   << " because it doesn't have a relevant device type.";
    return;
  }

  base::UmaHistogramEnumeration("OOBE.HidDetectionScreen.HidDisconnected",
                                hid_type.value());
}

void RecordBluetoothPairingAttempts(size_t attempts) {
  base::UmaHistogramCounts100(
      "OOBE.HidDetectionScreen.BluetoothPairingAttempts", attempts);
}

void RecordBluetoothPairingResult(bool success,
                                  base::TimeDelta pairing_duration) {
  base::UmaHistogramCustomTimes(
      base::StrCat({"OOBE.HidDetectionScreen.BluetoothPairing.Duration.",
                    success ? "Success" : "Failure"}),
      pairing_duration,
      /*min=*/base::Milliseconds(1),
      /*max=*/base::Seconds(30), /*buckets=*/50);

  // Also record the pairing result metric.
  base::UmaHistogramEnumeration(
      "OOBE.HidDetectionScreen.BluetoothPairing.Result",
      success ? HidDetectionBluetoothPairingResult::kPaired
              : HidDetectionBluetoothPairingResult::kNotPaired);
}

void RecordPairingTimeoutExceeded() {
  base::UmaHistogramBoolean(
      "OOBE.HidDetectionScreen.BluetoothPairing.TimeoutExceeded", true);
}

void RecordInitialHidsMissing(const HidsMissing& hids_missing) {
  base::UmaHistogramEnumeration("OOBE.HidDetectionScreen.InitialHidsMissing",
                                hids_missing);
}

}  // namespace ash::hid_detection
