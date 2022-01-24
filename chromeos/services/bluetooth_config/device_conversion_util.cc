// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_conversion_util.h"

#include <bitset>

#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

// Returns the DeviceType associated with |device|. Note that BluetoothDevice
// expresses more granularity than Bluetooth system UI, so some similar device
// types return the same DeviceType value (e.g., both PHONE and MODEM return
// the kPhone type).
mojom::DeviceType ComputeDeviceType(const device::BluetoothDevice* device) {
  switch (device->GetDeviceType()) {
    case device::BluetoothDeviceType::UNKNOWN:
      return mojom::DeviceType::kUnknown;

    case device::BluetoothDeviceType::COMPUTER:
      return mojom::DeviceType::kComputer;

    case device::BluetoothDeviceType::PHONE:
      FALLTHROUGH;
    case device::BluetoothDeviceType::MODEM:
      return mojom::DeviceType::kPhone;

    case device::BluetoothDeviceType::AUDIO:
      FALLTHROUGH;
    case device::BluetoothDeviceType::CAR_AUDIO:
      return mojom::DeviceType::kHeadset;

    case device::BluetoothDeviceType::VIDEO:
      return mojom::DeviceType::kVideoCamera;

    case device::BluetoothDeviceType::PERIPHERAL:
      FALLTHROUGH;
    case device::BluetoothDeviceType::JOYSTICK:
      FALLTHROUGH;
    case device::BluetoothDeviceType::GAMEPAD:
      return mojom::DeviceType::kGameController;

    case device::BluetoothDeviceType::KEYBOARD:
      FALLTHROUGH;
    case device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      return mojom::DeviceType::kKeyboard;

    case device::BluetoothDeviceType::MOUSE:
      return mojom::DeviceType::kMouse;

    case device::BluetoothDeviceType::TABLET:
      return mojom::DeviceType::kTablet;
  }
}

mojom::AudioOutputCapability ComputeAudioOutputCapability(
    const device::BluetoothDevice* device) {
  // For a device to provide audio output capabilities, both the "rendering" and
  // "audio" bits of the Bluetooth class must be set. See
  // https://crbug.com/1058451 for details.
  static constexpr int kRenderingBitPosition = 18;
  static constexpr int kAudioBitPosition = 21;

  const std::bitset<32> bluetooth_class_bitset(device->GetBluetoothClass());
  if (bluetooth_class_bitset.test(kRenderingBitPosition) &&
      bluetooth_class_bitset.test(kAudioBitPosition)) {
    return mojom::AudioOutputCapability::kCapableOfAudioOutput;
  }

  return mojom::AudioOutputCapability::kNotCapableOfAudioOutput;
}

mojom::DeviceBatteryInfoPtr ComputeBatteryInfo(
    const device::BluetoothDevice* device) {
  const absl::optional<device::BluetoothDevice::BatteryInfo> battery_info =
      device->GetBatteryInfo(device::BluetoothDevice::BatteryType::kDefault);

  if (!battery_info || !battery_info->percentage.has_value())
    return nullptr;

  return mojom::DeviceBatteryInfo::New(
      mojom::BatteryProperties::New(battery_info->percentage.value()));
}

mojom::DeviceConnectionState ComputeConnectionState(
    const device::BluetoothDevice* device) {
  if (device->IsConnected())
    return mojom::DeviceConnectionState::kConnected;

  if (device->IsConnecting())
    return mojom::DeviceConnectionState::kConnecting;

  return mojom::DeviceConnectionState::kNotConnected;
}

}  // namespace

mojom::BluetoothDevicePropertiesPtr GenerateBluetoothDeviceMojoProperties(
    const device::BluetoothDevice* device) {
  auto properties = mojom::BluetoothDeviceProperties::New();
  properties->id = device->GetIdentifier();
  properties->public_name = device->GetNameForDisplay();
  properties->device_type = ComputeDeviceType(device);
  properties->audio_capability = ComputeAudioOutputCapability(device);
  properties->battery_info = ComputeBatteryInfo(device);
  properties->connection_state = ComputeConnectionState(device);
  return properties;
}

}  // namespace bluetooth_config
}  // namespace chromeos
