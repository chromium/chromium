// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
#define CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

class UsbChooserContext;

// Implements a restricted device::mojom::UsbDeviceManager interface by wrapping
// another UsbDeviceManager instance and enforces the rules of the WebUSB
// permission model as well as permission granted by the user through a device
// chooser UI.
class WebUsbServiceImpl : public blink::mojom::WebUsbService,
                          public ChooserContextBase::PermissionObserver,
                          public UsbChooserContext::DeviceObserver,
                          public device::mojom::UsbDeviceClient {
 public:
  WebUsbServiceImpl(content::RenderFrameHost* render_frame_host,
                    base::WeakPtr<WebUsbChooser> usb_chooser);
  ~WebUsbServiceImpl() override;

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

 private:
  bool HasDevicePermission(
      const device::mojom::UsbDeviceInfo& device_info) const;

  // blink::mojom::WebUsbService implementation:
  void GetDevices(GetDevicesCallback callback) override;
  void GetDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) override;
  void GetPermission(
      std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
      GetPermissionCallback callback) override;
  void SetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client) override;

  void OnGetDevices(
      GetDevicesCallback callback,
      std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list);

  // ChooserContextBase::PermissionObserver implementation:
  void OnPermissionRevoked(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin) override;

  // UsbChooserContext::DeviceObserver implementation:
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceManagerConnectionError() override;

  // device::mojom::UsbDeviceClient implementation:
  void OnDeviceOpened() override;
  void OnDeviceClosed() override;

  void OnConnectionError();

  content::RenderFrameHost* const render_frame_host_;
  base::WeakPtr<WebUsbChooser> usb_chooser_;
  UsbChooserContext* chooser_context_;
  url::Origin requesting_origin_;
  url::Origin embedding_origin_;

  // Used to bind with Blink.
  mojo::ReceiverSet<blink::mojom::WebUsbService> receivers_;
  mojo::AssociatedRemoteSet<device::mojom::UsbDeviceManagerClient> clients_;

  // Tracks DeviceClient receivers for each device (by GUID).
  std::unordered_map<std::string,
                     mojo::ReceiverSet<device::mojom::UsbDeviceClient>>
      device_client_receivers_;

  ScopedObserver<UsbChooserContext, UsbChooserContext::DeviceObserver>
      device_observer_;
  ScopedObserver<ChooserContextBase, ChooserContextBase::PermissionObserver>
      permission_observer_;

  base::WeakPtrFactory<WebUsbServiceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebUsbServiceImpl);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
