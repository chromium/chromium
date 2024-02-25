// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_CHROME_USB_DELEGATE_H_
#define CHROME_BROWSER_USB_CHROME_USB_DELEGATE_H_

#include "base/containers/span.h"
#include "content/public/browser/usb_chooser.h"
#include "content/public/browser/usb_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

class ChromeUsbDelegate : public content::UsbDelegate {
 public:
  ChromeUsbDelegate();
  ChromeUsbDelegate(ChromeUsbDelegate&&) = delete;
  ChromeUsbDelegate& operator=(ChromeUsbDelegate&) = delete;
  ~ChromeUsbDelegate() override;

  // content::UsbDelegate:
  void AdjustProtectedInterfaceClasses(content::BrowserContext* browser_context,
                                       const url::Origin& origin,
                                       content::RenderFrameHost* frame,
                                       std::vector<uint8_t>& classes) override;
  std::unique_ptr<content::UsbChooser> RunChooser(
      content::RenderFrameHost& frame,
      blink::mojom::WebUsbRequestDeviceOptionsPtr options,
      blink::mojom::WebUsbService::GetPermissionCallback callback) override;
  bool PageMayUseUsb(content::Page& page) override;
  bool CanRequestDevicePermission(content::BrowserContext* browser_context,
                                  const url::Origin& origin) override;
  void RevokeDevicePermissionWebInitiated(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      const device::mojom::UsbDeviceInfo& device) override;
  const device::mojom::UsbDeviceInfo* GetDeviceInfo(
      content::BrowserContext* browser_context,
      const std::string& guid) override;
  bool HasDevicePermission(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      const url::Origin& origin,
      const device::mojom::UsbDeviceInfo& device_info) override;
  void GetDevices(
      content::BrowserContext* browser_context,
      blink::mojom::WebUsbService::GetDevicesCallback callback) override;
  void GetDevice(
      content::BrowserContext* browser_context,
      const std::string& guid,
      base::span<const uint8_t> blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override;
  void AddObserver(content::BrowserContext* browser_context,
                   Observer* observer) override;
  void RemoveObserver(content::BrowserContext* browser_context,
                      Observer* observer) override;
  bool IsServiceWorkerAllowedForOrigin(const url::Origin& origin) override;
  void IncrementConnectionCount(content::BrowserContext* browser_context,
                                const url::Origin& origin) override;
  void DecrementConnectionCount(content::BrowserContext* browser_context,
                                const url::Origin& origin) override;

 private:
  class ContextObservation;

  ContextObservation* GetContextObserver(
      content::BrowserContext* browser_context);

  base::flat_map<content::BrowserContext*, std::unique_ptr<ContextObservation>>
      observations_;
};

#endif  // CHROME_BROWSER_USB_CHROME_USB_DELEGATE_H_
