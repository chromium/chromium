// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
#define CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/usb/chrome_usb_delegate.h"
#include "content/public/browser/usb_chooser.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

// Implements a restricted device::mojom::UsbDeviceManager interface by wrapping
// another UsbDeviceManager instance and enforces the rules of the WebUSB
// permission model as well as permission granted by the user through a device
// chooser UI.
class WebUsbServiceImpl : public blink::mojom::WebUsbService,
                          public ChromeUsbDelegate::Observer {
 public:
  explicit WebUsbServiceImpl(content::RenderFrameHost* render_frame_host);

  WebUsbServiceImpl(const WebUsbServiceImpl&) = delete;
  WebUsbServiceImpl& operator=(const WebUsbServiceImpl&) = delete;

  ~WebUsbServiceImpl() override;

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

  // Allow tests to set the ChromeUsbDelegate.
  void SetUsbDelegateForTesting(std::unique_ptr<ChromeUsbDelegate> delegate);

 private:
  class UsbDeviceClient;

  ChromeUsbDelegate* delegate() const { return delegate_.get(); }

  std::vector<uint8_t> GetProtectedInterfaceClasses() const;

  // blink::mojom::WebUsbService implementation:
  void GetDevices(GetDevicesCallback callback) override;
  void GetDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) override;
  void GetPermission(
      std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
      GetPermissionCallback callback) override;
  void ForgetDevice(const std::string& guid,
                    ForgetDeviceCallback callback) override;
  void SetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client) override;

  void OnGetDevices(
      GetDevicesCallback callback,
      std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list);

  // UsbDelegate::Observer implementation:
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceManagerConnectionError() override;
  void OnPermissionRevoked(const url::Origin& origin) override;

  void IncrementConnectionCount();
  void DecrementConnectionCount();
  void RemoveDeviceClient(const UsbDeviceClient* client);

  void OnConnectionError();

  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  std::unique_ptr<content::UsbChooser> usb_chooser_;
  url::Origin origin_;

  // Used to bind with Blink.
  mojo::ReceiverSet<blink::mojom::WebUsbService> receivers_;
  mojo::AssociatedRemoteSet<device::mojom::UsbDeviceManagerClient> clients_;

  // A UsbDeviceClient tracks a UsbDevice pipe that has been passed to Blink.
  std::vector<std::unique_ptr<UsbDeviceClient>> device_clients_;

  std::unique_ptr<ChromeUsbDelegate> delegate_;

  base::WeakPtrFactory<WebUsbServiceImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
