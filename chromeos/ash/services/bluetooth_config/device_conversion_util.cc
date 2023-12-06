// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_conversion_util.h"

#include <bitset>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/string_util_icu.h"
#include "url/gurl.h"

namespace ash::bluetooth_config {

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
      [[fallthrough]];
    case device::BluetoothDeviceType::MODEM:
      return mojom::DeviceType::kPhone;

    case device::BluetoothDeviceType::AUDIO:
      [[fallthrough]];
    case device::BluetoothDeviceType::CAR_AUDIO:
      return mojom::DeviceType::kHeadset;

    case device::BluetoothDeviceType::VIDEO:
      return mojom::DeviceType::kVideoCamera;

    case device::BluetoothDeviceType::PERIPHERAL:
      [[fallthrough]];
    case device::BluetoothDeviceType::JOYSTICK:
      [[fallthrough]];
    case device::BluetoothDeviceType::GAMEPAD:
      return mojom::DeviceType::kGameController;

    case device::BluetoothDeviceType::KEYBOARD:
      return mojom::DeviceType::kKeyboard;

    case device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      return mojom::DeviceType::kKeyboardMouseCombo;

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

  // Alternatively, audio output capable devices may have any of the following
  // UUIDs in its UUID list.
  static const std::array<device::BluetoothUUID, 3> kAudioServiceUuids{
      device::BluetoothUUID("00001108-0000-1000-8000-00805f9b34fb"),  // Headset
      device::BluetoothUUID(
          "0000110b-0000-1000-8000-00805f9b34fb"),  // Audio Sink
      device::BluetoothUUID(
          "0000111e-0000-1000-8000-00805f9b34fb"),  // Handsfree
  };

  const std::bitset<32> bluetooth_class_bitset(device->GetBluetoothClass());
  if (bluetooth_class_bitset.test(kRenderingBitPosition) &&
      bluetooth_class_bitset.test(kAudioBitPosition)) {
    return mojom::AudioOutputCapability::kCapableOfAudioOutput;
  }

  // Some audio devices won't have both the rendering and audio bit set
  // (b/209666215). Check if any of the device's UUIDs correspond with an audio
  // service.
  base::flat_set<device::BluetoothUUID> uuids = device->GetUUIDs();
  for (device::BluetoothUUID audio_service_uuid : kAudioServiceUuids) {
    if (uuids.contains(audio_service_uuid)) {
      return mojom::AudioOutputCapability::kCapableOfAudioOutput;
    }
  }

  return mojom::AudioOutputCapability::kNotCapableOfAudioOutput;
}

mojom::BatteryPropertiesPtr ComputeBatteryInfoForBatteryType(
    const device::BluetoothDevice* device,
    device::BluetoothDevice::BatteryType battery_type) {
  const std::optional<device::BluetoothDevice::BatteryInfo> battery_info =
      device->GetBatteryInfo(battery_type);

  if (!battery_info || !battery_info->percentage.has_value())
    return nullptr;

  return mojom::BatteryProperties::New(battery_info->percentage.value());
}

mojom::DeviceBatteryInfoPtr ComputeBatteryInfo(
    const device::BluetoothDevice* device) {
  // Only provide battery information for devices that are connected.
  if (!device->IsConnected())
    return nullptr;

  mojom::BatteryPropertiesPtr default_battery =
      ComputeBatteryInfoForBatteryType(
          device, device::BluetoothDevice::BatteryType::kDefault);
  mojom::BatteryPropertiesPtr left_bud_battery =
      ComputeBatteryInfoForBatteryType(
          device, device::BluetoothDevice::BatteryType::kLeftBudTrueWireless);
  mojom::BatteryPropertiesPtr right_bud_battery =
      ComputeBatteryInfoForBatteryType(
          device, device::BluetoothDevice::BatteryType::kRightBudTrueWireless);
  mojom::BatteryPropertiesPtr case_battery = ComputeBatteryInfoForBatteryType(
      device, device::BluetoothDevice::BatteryType::kCaseTrueWireless);

  if (!default_battery && !left_bud_battery && !right_bud_battery &&
      !case_battery) {
    return nullptr;
  }

  mojom::DeviceBatteryInfoPtr device_battery_info =
      mojom::DeviceBatteryInfo::New();

  if (default_battery)
    device_battery_info->default_properties = std::move(default_battery);

  if (left_bud_battery)
    device_battery_info->left_bud_info = std::move(left_bud_battery);

  if (right_bud_battery)
    device_battery_info->right_bud_info = std::move(right_bud_battery);

  if (case_battery)
    device_battery_info->case_info = std::move(case_battery);

  return device_battery_info;
}

mojom::DeviceConnectionState ComputeConnectionState(
    const device::BluetoothDevice* device) {
  if (device->IsConnected())
    return mojom::DeviceConnectionState::kConnected;

  if (device->IsConnecting())
    return mojom::DeviceConnectionState::kConnecting;

  return mojom::DeviceConnectionState::kNotConnected;
}

std::u16string ComputeDeviceName(const device::BluetoothDevice* device) {
  std::optional<std::string> name = device->GetName();
  if (name && device::HasGraphicCharacter(name.value()))
    return device->GetNameForDisplay();

  return base::UTF8ToUTF16(device->GetAddress());
}

mojom::DeviceImageInfoPtr ComputeImageInfo(
    const device::BluetoothDevice* device,
    FastPairDelegate* fast_pair_delegate) {
  std::optional<DeviceImageInfo> images =
      fast_pair_delegate->GetDeviceImageInfo(device->GetAddress());
  if (!images) {
    return nullptr;
  }

  mojom::DeviceImageInfoPtr device_image_info = mojom::DeviceImageInfo::New();
  if (!images->default_image().empty()) {
    GURL default_image_url = GURL(images->default_image());
    DCHECK(default_image_url.is_valid() &&
           default_image_url.SchemeIs(url::kDataScheme));
    device_image_info->default_image_url = default_image_url;
  }

  // Only add true_wireless_images if all fields are non-null.
  if (images->left_bud_image().empty() || images->right_bud_image().empty() ||
      images->case_image().empty()) {
    return device_image_info;
  }

  mojom::TrueWirelessImageInfoPtr true_wireless_images =
      mojom::TrueWirelessImageInfo::New();
  GURL left_bud_image_url = GURL(images->left_bud_image());
  DCHECK(left_bud_image_url.is_valid() &&
         left_bud_image_url.SchemeIs(url::kDataScheme));
  true_wireless_images->left_bud_image_url = GURL(images->left_bud_image());

  GURL right_bud_image_url = GURL(images->right_bud_image());
  DCHECK(right_bud_image_url.is_valid() &&
         right_bud_image_url.SchemeIs(url::kDataScheme));
  true_wireless_images->right_bud_image_url = GURL(images->right_bud_image());

  GURL case_image_url = GURL(images->case_image());
  DCHECK(case_image_url.is_valid() &&
         case_image_url.SchemeIs(url::kDataScheme));
  true_wireless_images->case_image_url = GURL(images->case_image());

  device_image_info->true_wireless_images = std::move(true_wireless_images);
  return device_image_info;
}

}  // namespace

mojom::BluetoothDevicePropertiesPtr GenerateBluetoothDeviceMojoProperties(
    const device::BluetoothDevice* device,
    FastPairDelegate* fast_pair_delegate) {
  auto properties = mojom::BluetoothDeviceProperties::New();
  properties->id = device->GetIdentifier();
  properties->address = device->GetAddress();
  properties->public_name = ComputeDeviceName(device);
  properties->device_type = ComputeDeviceType(device);
  properties->audio_capability = ComputeAudioOutputCapability(device);
  properties->battery_info = ComputeBatteryInfo(device);
  properties->connection_state = ComputeConnectionState(device);
  properties->is_blocked_by_policy = device->IsBlockedByPolicy();
  if (fast_pair_delegate)
    properties->image_info = ComputeImageInfo(device, fast_pair_delegate);
  return properties;
}

}  // namespace ash::bluetooth_config
