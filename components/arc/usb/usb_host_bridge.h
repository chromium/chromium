// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_USB_USB_HOST_BRIDGE_H_
#define COMPONENTS_ARC_USB_USB_HOST_BRIDGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/arc/mojom/usb_host.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

class BrowserContextKeyedServiceFactory;

namespace arc {

class ArcBridgeService;
class ArcUsbHostUiDelegate;

// Private implementation of UsbHostHost.
class ArcUsbHostBridge : public KeyedService,
                         public ConnectionObserver<mojom::UsbHostInstance>,
                         public device::mojom::UsbDeviceManagerClient,
                         public mojom::UsbHostHost {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Attaches the unclaimed USB devices to the ARCVM instance if ARCVM is
    // enabled. Called by ArcUsbHostBridge once it successfully established the
    // Mojo connection to the ARC instance. This may be called multiple times
    // within the lifetime of a single ArcUsbHostBridge instance.
    virtual void AttachDevicesToArcVm() = 0;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcUsbHostBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // The constructor will register an Observer with ArcBridgeService.
  ArcUsbHostBridge(content::BrowserContext* context,
                   ArcBridgeService* bridge_service);
  ~ArcUsbHostBridge() override;

  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // mojom::UsbHostHost overrides:
  void RequestPermission(const std::string& guid,
                         const std::string& package,
                         bool interactive,
                         RequestPermissionCallback callback) override;
  void OpenDeviceDeprecated(const std::string& guid,
                            const base::Optional<std::string>& package,
                            OpenDeviceCallback callback) override;
  void OpenDevice(const std::string& guid,
                  const base::Optional<std::string>& package,
                  OpenDeviceCallback callback) override;
  void GetDeviceInfo(const std::string& guid,
                     GetDeviceInfoCallback callback) override;

  // ConnectionObserver<mojom::UsbHostInstance> overrides:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // KeyedService overrides:
  void Shutdown() override;

  void SetUiDelegate(ArcUsbHostUiDelegate* ui_delegate);

  // Sets the Delegate instance.
  void SetDelegate(std::unique_ptr<Delegate> delegate);

 private:
  // Init |devices_| once the device list has been returned, so that we
  // can get UsbDeviceInfo from |guid| for other methods.
  void InitDeviceList(std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  std::vector<std::string> GetEventReceiverPackages(
      const device::mojom::UsbDeviceInfo& device_info);
  void OnDeviceChecked(const std::string& guid, bool allowed);
  bool HasPermissionForDevice(const device::mojom::UsbDeviceInfo& device_info,
                              const std::string& package);
  void HandleScanDeviceListRequest(const std::string& package,
                                   RequestPermissionCallback callback);
  void Disconnect();

  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override;

  SEQUENCE_CHECKER(sequence_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  mojom::UsbHostHostPtr usb_host_ptr_;

  // Connection to the DeviceService for usb manager.
  mojo::Remote<device::mojom::UsbDeviceManager> usb_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};

  // A mapping from GUID -> UsbDeviceInfoPtr for each attached USB device.
  std::map<std::string, device::mojom::UsbDeviceInfoPtr> devices_;

  ArcUsbHostUiDelegate* ui_delegate_ = nullptr;
  std::unique_ptr<Delegate> delegate_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcUsbHostBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcUsbHostBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_USB_USB_HOST_BRIDGE_H_
