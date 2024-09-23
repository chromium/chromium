// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_BLUETOOTH_CHOOSER_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_BLUETOOTH_CHOOSER_CONTEXT_H_

#include <map>
#include <optional>
#include <string>
#include <utility>

#include "components/permissions/object_permission_context_base.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"

namespace base {
class Value;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace device {
class BluetoothDevice;
class BluetoothUUID;
}  // namespace device

namespace url {
class Origin;
}  // namespace url

namespace permissions {

// Manages the permissions for Web Bluetooth device objects. A Web Bluetooth
// permission object consists of its WebBluetoothDeviceId and set of Bluetooth
// service UUIDs. The WebBluetoothDeviceId is generated randomly by this class
// and is unique for a given Bluetooth device address and origin pair, so this
// class stores this mapping and provides utility methods to convert between
// the WebBluetoothDeviceId and Bluetooth device address.
class BluetoothChooserContext : public ObjectPermissionContextBase {
 public:
  explicit BluetoothChooserContext(content::BrowserContext* browser_context);
  ~BluetoothChooserContext() override;

  // Set class as move-only.
  BluetoothChooserContext(const BluetoothChooserContext&) = delete;
  BluetoothChooserContext& operator=(const BluetoothChooserContext&) = delete;

  static base::Value::Dict DeviceInfoToValue(
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options,
      const blink::WebBluetoothDeviceId& device_id);

  // Helper methods for converting between a WebBluetoothDeviceId and a
  // Bluetooth device address string for a given origin pair.
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      const url::Origin& origin,
      const std::string& device_address);
  std::string GetDeviceAddress(const url::Origin& origin,
                               const blink::WebBluetoothDeviceId& device_id);

  // Bluetooth scanning specific interface for generating WebBluetoothDeviceIds
  // for scanned devices.
  blink::WebBluetoothDeviceId AddScannedDevice(
      const url::Origin& origin,
      const std::string& device_address);

  // Bluetooth-specific interface for granting and checking permissions.
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      const url::Origin& origin,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options);
  bool HasDevicePermission(const url::Origin& origin,
                           const blink::WebBluetoothDeviceId& device_id);
  void RevokeDevicePermissionWebInitiated(
      const url::Origin& origin,
      const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessAtLeastOneService(
      const url::Origin& origin,
      const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessService(const url::Origin& origin,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service);
  bool IsAllowedToAccessManufacturerData(
      const url::Origin& origin,
      const blink::WebBluetoothDeviceId& device_id,
      uint16_t manufacturer_code);

  static blink::WebBluetoothDeviceId GetObjectDeviceId(
      const base::Value::Dict& object);

  // ObjectPermissionContextBase;
  std::string GetKeyForObject(const base::Value::Dict& object) override;
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  // KeyedService:
  void Shutdown() override;

 private:
  static bool IsValidDict(const base::Value::Dict& dict);

  std::optional<base::Value::Dict> FindDeviceObject(
      const url::Origin& origin,
      const blink::WebBluetoothDeviceId& device_id);

  // This map records the generated Web Bluetooth IDs for devices discovered via
  // the Scanning API. Each requesting/embedding origin pair has its own version
  // of this map so that IDs cannot be correlated between cross-origin sites.
  using DeviceAddressToIdMap =
      std::map<std::string, blink::WebBluetoothDeviceId>;
  std::map<url::Origin, DeviceAddressToIdMap> scanned_devices_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_BLUETOOTH_CHOOSER_CONTEXT_H_
