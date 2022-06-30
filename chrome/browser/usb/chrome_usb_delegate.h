// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_CHROME_USB_DELEGATE_H_
#define CHROME_BROWSER_USB_CHROME_USB_DELEGATE_H_

#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/usb_chooser.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

class ChromeUsbDelegate
    : public permissions::ObjectPermissionContextBase::PermissionObserver,
      public UsbChooserContext::DeviceObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device) = 0;
    virtual void OnDeviceRemoved(
        const device::mojom::UsbDeviceInfo& device) = 0;
    virtual void OnDeviceManagerConnectionError() = 0;
    virtual void OnPermissionRevoked(const url::Origin& origin) = 0;
  };

  ChromeUsbDelegate();
  ChromeUsbDelegate(ChromeUsbDelegate&&) = delete;
  ChromeUsbDelegate& operator=(ChromeUsbDelegate&) = delete;
  ~ChromeUsbDelegate() override;

  void AdjustProtectedInterfaceClasses(content::RenderFrameHost& frame,
                                       std::vector<uint8_t>& classes);
  virtual std::unique_ptr<content::UsbChooser> RunChooser(
      content::RenderFrameHost& frame,
      std::vector<device::mojom::UsbDeviceFilterPtr> filters,
      blink::mojom::WebUsbService::GetPermissionCallback callback);
  bool CanRequestDevicePermission(content::RenderFrameHost& frame);
  void RevokeDevicePermissionWebInitiated(
      content::RenderFrameHost& frame,
      const device::mojom::UsbDeviceInfo& device);
  const device::mojom::UsbDeviceInfo* GetDeviceInfo(
      content::RenderFrameHost& frame,
      const std::string& guid);
  bool HasDevicePermission(content::RenderFrameHost& frame,
                           const device::mojom::UsbDeviceInfo& device);
  void GetDevices(
      content::RenderFrameHost& frame,
      base::OnceCallback<void(std::vector<device::mojom::UsbDeviceInfoPtr>)>
          callback);
  void GetDevice(
      content::RenderFrameHost& frame,
      const std::string& guid,
      base::span<const uint8_t> blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client);
  void AddObserver(content::RenderFrameHost& frame, Observer* observer);
  void RemoveObserver(Observer* observer);

  // permissions::ObjectPermissionContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& origin) override;

  // UsbChooserContext::DeviceObserver:
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo&) override;
  void OnDeviceRemoved(const device::mojom::UsbDeviceInfo&) override;
  void OnDeviceManagerConnectionError() override;

 private:
  base::ScopedObservation<UsbChooserContext,
                          UsbChooserContext::DeviceObserver,
                          &UsbChooserContext::AddObserver,
                          &UsbChooserContext::RemoveObserver>
      device_observation_{this};
  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      permission_observation_{this};
  base::ObserverList<Observer> observer_list_;
};

#endif  // CHROME_BROWSER_USB_CHROME_USB_DELEGATE_H_
