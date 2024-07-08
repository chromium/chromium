// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/usb/usb_policy_allowed_devices.h"
#include "components/permissions/object_permission_context_base.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "url/origin.h"

class Profile;

class UsbChooserContext : public permissions::ObjectPermissionContextBase,
                          public device::mojom::UsbDeviceManagerClient {
 public:
  explicit UsbChooserContext(Profile* profile);

  UsbChooserContext(const UsbChooserContext&) = delete;
  UsbChooserContext& operator=(const UsbChooserContext&) = delete;

  ~UsbChooserContext() override;

  // This observer can be used to be notified of changes to USB devices that are
  // connected.
  class DeviceObserver : public base::CheckedObserver {
   public:
    virtual void OnDeviceAdded(const device::mojom::UsbDeviceInfo&);
    virtual void OnDeviceRemoved(const device::mojom::UsbDeviceInfo&);
    virtual void OnDeviceManagerConnectionError();

    // Called when the BrowserContext is shutting down. Observers must remove
    // themselves before returning.
    virtual void OnBrowserContextShutdown() = 0;
  };

  static base::Value::Dict DeviceInfoToValue(
      const device::mojom::UsbDeviceInfo& device_info);

  // ObjectPermissionContextBase:
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const url::Origin& origin,
                              const base::Value::Dict& object) override;
  std::string GetKeyForObject(const base::Value::Dict& object) override;
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  // Grants |origin| access to the USB device.
  void GrantDevicePermission(const url::Origin& origin,
                             const device::mojom::UsbDeviceInfo& device_info);

  // Checks if |origin| has access to a device with |device_info|.
  bool HasDevicePermission(const url::Origin& origin,
                           const device::mojom::UsbDeviceInfo& device_info);

  // Revokes |origin| access to the USB device ordered by website.
  void RevokeDevicePermissionWebInitiated(
      const url::Origin& origin,
      const device::mojom::UsbDeviceInfo& device);

  void AddObserver(DeviceObserver* observer);
  void RemoveObserver(DeviceObserver* observer);

  // Forward UsbDeviceManager methods.
  void GetDevices(device::mojom::UsbDeviceManager::GetDevicesCallback callback);
  void GetDevice(
      const std::string& guid,
      base::span<const uint8_t> blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client);
#if BUILDFLAG(IS_ANDROID)
  void RefreshDeviceInfo(
      const std::string& guid,
      device::mojom::UsbDeviceManager::RefreshDeviceInfoCallback callback);
#endif

  // This method should only be called when you are sure that |devices_| has
  // been initialized. It will return nullptr if the guid cannot be found.
  const device::mojom::UsbDeviceInfo* GetDeviceInfo(const std::string& guid);

  base::WeakPtr<UsbChooserContext> AsWeakPtr();

  void SetDeviceManagerForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_device_manager);

  void InitDeviceList(std::vector<::device::mojom::UsbDeviceInfoPtr> devices);

  const UsbPolicyAllowedDevices& usb_policy_allowed_devices() {
    return *usb_policy_allowed_devices_;
  }

  // KeyedService:
  void Shutdown() override;

 private:
  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override;

  void RevokeObjectPermissionInternal(const url::Origin& origin,
                                      const base::Value::Dict& object,
                                      bool revoked_by_website);

  void OnDeviceManagerConnectionError();
  void EnsureConnectionWithDeviceManager();
  void SetUpDeviceManagerConnection();
#if BUILDFLAG(IS_ANDROID)
  void OnDeviceInfoRefreshed(
      device::mojom::UsbDeviceManager::RefreshDeviceInfoCallback callback,
      device::mojom::UsbDeviceInfoPtr device_info);
#endif

  bool is_incognito_;
  bool is_initialized_ = false;
  base::queue<device::mojom::UsbDeviceManager::GetDevicesCallback>
      pending_get_devices_requests_;

  std::map<url::Origin, std::set<std::string>> ephemeral_devices_;
  std::map<std::string, device::mojom::UsbDeviceInfoPtr> devices_;

  std::unique_ptr<UsbPolicyAllowedDevices> usb_policy_allowed_devices_;

  // Connection to |device_manager_instance_|.
  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};
  base::ObserverList<DeviceObserver> device_observer_list_;

  base::WeakPtrFactory<UsbChooserContext> weak_factory_{this};
};

#endif  // CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_
