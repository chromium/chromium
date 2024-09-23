// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/bluetooth_chooser_context.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_context.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "url/origin.h"

using blink::WebBluetoothDeviceId;
using device::BluetoothUUID;

namespace permissions {

namespace {

// The Bluetooth device permission objects are dictionary type base::Values. The
// object contains keys for the device address, device name, services that can
// be accessed, and the generated web bluetooth device ID. Since base::Value
// does not have a set type, the services key contains another dictionary type
// base::Value object where each key is a UUID for a service and the value is a
// boolean that is never used. This allows for service permissions to be queried
// quickly and for new service permissions to added without duplicating existing
// service permissions. The following is an example of how a device permission
// is formatted using JSON notation:
// {
//   "device-address": "00:00:00:00:00:00",
//   "name": "Wireless Device",
//   "services": {
//     "0xabcd": "true",
//     "0x1234": "true",
//   },
//   "web-bluetooth-device-id": "4ik7W0WVaGFY6zXxJqdAKw==",
// }
constexpr char kDeviceAddressKey[] = "device-address";
constexpr char kDeviceNameKey[] = "name";
constexpr char kManufacturerDataKey[] = "manufacturer-data";
constexpr char kServicesKey[] = "services";
constexpr char kWebBluetoothDeviceIdKey[] = "web-bluetooth-device-id";

// The Web Bluetooth API spec states that when the user selects a device to
// pair with the origin, the origin is allowed to access any service listed in
// |options->filters| and |options->optional_services|.
// https://webbluetoothcg.github.io/web-bluetooth/#bluetooth
void AddUnionOfServicesTo(
    const blink::mojom::WebBluetoothRequestDeviceOptions* options,
    base::Value::Dict& permission_object) {
  if (!options)
    return;

  DCHECK(!!permission_object.FindDict(kServicesKey));
  auto& services_dict = *permission_object.FindDict(kServicesKey);
  if (options->filters) {
    for (const blink::mojom::WebBluetoothLeScanFilterPtr& filter :
         options->filters.value()) {
      if (!filter->services)
        continue;

      for (const BluetoothUUID& uuid : filter->services.value())
        services_dict.Set(uuid.canonical_value(), /*val=*/true);
    }
  }

  for (const BluetoothUUID& uuid : options->optional_services)
    services_dict.Set(uuid.canonical_value(), /*val=*/true);
}

void AddManufacturerDataTo(
    const blink::mojom::WebBluetoothRequestDeviceOptions* options,
    base::Value::Dict& permission_object) {
  if (!options || options->optional_manufacturer_data.empty())
    return;

  CHECK(permission_object.FindDict(kManufacturerDataKey));
  auto& manufacturer_data_dict =
      *permission_object.FindDict(kManufacturerDataKey);
  for (uint16_t manufacturer_code : options->optional_manufacturer_data) {
    manufacturer_data_dict.Set(base::NumberToString(manufacturer_code),
                               /*value=*/true);
  }
}

}  // namespace

BluetoothChooserContext::BluetoothChooserContext(
    content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::BLUETOOTH_GUARD,
          ContentSettingsType::BLUETOOTH_CHOOSER_DATA,
          PermissionsClient::Get()->GetSettingsMap(browser_context)) {}

BluetoothChooserContext::~BluetoothChooserContext() = default;

// static
base::Value::Dict BluetoothChooserContext::DeviceInfoToValue(
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options,
    const WebBluetoothDeviceId& device_id) {
  base::Value::Dict device_value;
  device_value.Set(kDeviceAddressKey, device->GetAddress());
  device_value.Set(kWebBluetoothDeviceIdKey, device_id.str());
  device_value.Set(kDeviceNameKey, device->GetNameForDisplay());

  device_value.Set(kServicesKey, base::Value::Dict());
  AddUnionOfServicesTo(options, device_value);

  device_value.Set(kManufacturerDataKey, base::Value::Dict());
  AddManufacturerDataTo(options, device_value);

  return device_value;
}

WebBluetoothDeviceId BluetoothChooserContext::GetWebBluetoothDeviceId(
    const url::Origin& origin,
    const std::string& device_address) {
  const std::vector<std::unique_ptr<Object>> object_list =
      GetGrantedObjects(origin);
  for (const auto& object : object_list) {
    const base::Value::Dict& device = object->value;
    DCHECK(IsValidObject(device));

    if (device_address == *device.FindString(kDeviceAddressKey)) {
      return WebBluetoothDeviceId(*device.FindString(kWebBluetoothDeviceIdKey));
    }
  }

  // Check if the device has been assigned an ID through an LE scan.
  auto scanned_devices_it = scanned_devices_.find(origin);
  if (scanned_devices_it == scanned_devices_.end())
    return {};

  auto address_to_id_it = scanned_devices_it->second.find(device_address);
  if (address_to_id_it != scanned_devices_it->second.end())
    return address_to_id_it->second;
  return {};
}

std::string BluetoothChooserContext::GetDeviceAddress(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id) {
  std::optional<base::Value::Dict> device = FindDeviceObject(origin, device_id);
  if (device)
    return *device->FindString(kDeviceAddressKey);

  // Check if the device ID corresponds to a device detected via an LE scan.
  auto scanned_devices_it = scanned_devices_.find(origin);
  if (scanned_devices_it == scanned_devices_.end())
    return std::string();

  for (const auto& entry : scanned_devices_it->second) {
    if (entry.second == device_id)
      return entry.first;
  }
  return std::string();
}

WebBluetoothDeviceId BluetoothChooserContext::AddScannedDevice(
    const url::Origin& origin,
    const std::string& device_address) {
  // Check if a WebBluetoothDeviceId already exists for the device with
  // |device_address| for the current origin.
  const auto granted_id = GetWebBluetoothDeviceId(origin, device_address);
  if (granted_id.IsValid())
    return granted_id;

  DeviceAddressToIdMap& address_to_id_map = scanned_devices_[origin];
  auto scanned_id = WebBluetoothDeviceId::Create();
  address_to_id_map.emplace(device_address, scanned_id);
  return scanned_id;
}

WebBluetoothDeviceId BluetoothChooserContext::GrantServiceAccessPermission(
    const url::Origin& origin,
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  // If |origin| already has permission to access the device with
  // |device_address|, update the allowed GATT services by performing a union of
  // |services|.
  const std::vector<std::unique_ptr<Object>> object_list =
      GetGrantedObjects(origin);
  const std::string& device_address = device->GetAddress();
  for (const auto& object : object_list) {
    const base::Value::Dict& device_dict = object->value;
    DCHECK(IsValidObject(device_dict));
    if (device_address == *device_dict.FindString(kDeviceAddressKey)) {
      auto new_device_dict = device_dict.Clone();
      WebBluetoothDeviceId device_id(
          *new_device_dict.FindString(kWebBluetoothDeviceIdKey));

      AddUnionOfServicesTo(options, new_device_dict);
      AddManufacturerDataTo(options, new_device_dict);
      UpdateObjectPermission(origin, device_dict, std::move(new_device_dict));
      return device_id;
    }
  }

  // If the device has been detected through the Web Bluetooth Scanning API,
  // grant permission using the WebBluetoothDeviceId generated through that API.
  // Remove the ID from the temporary |scanned_devices_| map to avoid
  // duplication, since the ID will now be stored in HostContentSettingsMap.
  WebBluetoothDeviceId device_id;
  auto scanned_devices_it = scanned_devices_.find(origin);
  if (scanned_devices_it != scanned_devices_.end()) {
    auto& address_to_id_map = scanned_devices_it->second;
    auto address_to_id_it = address_to_id_map.find(device_address);

    if (address_to_id_it != address_to_id_map.end()) {
      device_id = address_to_id_it->second;
      address_to_id_map.erase(address_to_id_it);

      if (scanned_devices_it->second.empty())
        scanned_devices_.erase(scanned_devices_it);
    }
  }

  if (!device_id.IsValid())
    device_id = WebBluetoothDeviceId::Create();

  base::Value::Dict permission_object =
      DeviceInfoToValue(device, options, device_id);
  GrantObjectPermission(origin, std::move(permission_object));
  return device_id;
}

bool BluetoothChooserContext::HasDevicePermission(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id) {
  std::optional<base::Value::Dict> device = FindDeviceObject(origin, device_id);
  return device.has_value();
}

void BluetoothChooserContext::RevokeDevicePermissionWebInitiated(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id) {
  std::optional<base::Value::Dict> device = FindDeviceObject(origin, device_id);
  if (device.has_value())
    RevokeObjectPermission(origin, std::move(*device));
}

bool BluetoothChooserContext::IsAllowedToAccessAtLeastOneService(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id) {
  std::optional<base::Value::Dict> device = FindDeviceObject(origin, device_id);
  if (!device.has_value())
    return false;
  return !device->FindDict(kServicesKey)->empty();
}

bool BluetoothChooserContext::IsAllowedToAccessService(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  std::optional<base::Value::Dict> device = FindDeviceObject(origin, device_id);
  if (!device.has_value())
    return false;

  const auto& services_dict = *device->FindDict(kServicesKey);
  return !!services_dict.contains(service.canonical_value());
}

bool BluetoothChooserContext::IsAllowedToAccessManufacturerData(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id,
    uint16_t manufacturer_code) {
  std::optional<base::Value::Dict> device = FindDeviceObject(origin, device_id);
  if (!device.has_value())
    return false;

  const auto& manufacturer_data_list = *device->FindDict(kManufacturerDataKey);
  return manufacturer_data_list.contains(
      base::NumberToString(manufacturer_code));
}

// static
WebBluetoothDeviceId BluetoothChooserContext::GetObjectDeviceId(
    const base::Value::Dict& object) {
  std::string device_id_str = *object.FindString(kWebBluetoothDeviceIdKey);
  return WebBluetoothDeviceId(device_id_str);
}

std::string BluetoothChooserContext::GetKeyForObject(
    const base::Value::Dict& object) {
  if (!IsValidObject(object))
    return std::string();
  return *(object.FindString(kWebBluetoothDeviceIdKey));
}

bool BluetoothChooserContext::IsValidObject(const base::Value::Dict& object) {
  return IsValidDict(object);
}

std::u16string BluetoothChooserContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  return base::UTF8ToUTF16(*object.FindString(kDeviceNameKey));
}

void BluetoothChooserContext::Shutdown() {
  FlushScheduledSaveSettingsCalls();
  ObjectPermissionContextBase::Shutdown();
}

bool BluetoothChooserContext::IsValidDict(const base::Value::Dict& dict) {
  return dict.FindString(kDeviceAddressKey) &&
         dict.FindString(kDeviceNameKey) &&
         dict.FindString(kWebBluetoothDeviceIdKey) &&
         WebBluetoothDeviceId::IsValid(
             *dict.FindString(kWebBluetoothDeviceIdKey)) &&
         dict.FindDict(kServicesKey) && dict.FindDict(kManufacturerDataKey);
}

std::optional<base::Value::Dict> BluetoothChooserContext::FindDeviceObject(
    const url::Origin& origin,
    const blink::WebBluetoothDeviceId& device_id) {
  const std::vector<std::unique_ptr<Object>> object_list =
      GetGrantedObjects(origin);
  for (const auto& object : object_list) {
    base::Value::Dict device = std::move(object->value);
    DCHECK(IsValidDict(device));

    const WebBluetoothDeviceId web_bluetooth_device_id(
        *device.FindString(kWebBluetoothDeviceIdKey));
    if (device_id == web_bluetooth_device_id)
      return device;
  }
  return std::nullopt;
}

}  // namespace permissions
