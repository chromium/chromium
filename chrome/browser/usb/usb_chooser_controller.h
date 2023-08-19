// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_USB_USB_CHOOSER_CONTROLLER_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "components/permissions/chooser_controller.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

// UsbChooserController creates a chooser for WebUSB.
class UsbChooserController : public permissions::ChooserController,
                             public UsbChooserContext::DeviceObserver {
 public:
  UsbChooserController(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::WebUsbRequestDeviceOptionsPtr options,
      blink::mojom::WebUsbService::GetPermissionCallback callback);

  UsbChooserController(const UsbChooserController&) = delete;
  UsbChooserController& operator=(const UsbChooserController&) = delete;

  ~UsbChooserController() override;

  // permissions::ChooserController:
  std::u16string GetNoOptionsText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  size_t NumOptions() const override;
  std::u16string GetOption(size_t index) const override;
  bool IsPaired(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // UsbChooserContext::DeviceObserver implementation:
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::UsbDeviceInfo& device_info) override;
  void OnBrowserContextShutdown() override;

 private:
  void GotUsbDeviceList(std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  bool DisplayDevice(const device::mojom::UsbDeviceInfo& device) const;

  blink::mojom::WebUsbRequestDeviceOptionsPtr options_;
  blink::mojom::WebUsbService::GetPermissionCallback callback_;
  url::Origin origin_;

  const raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged>
      requesting_frame_;
  base::WeakPtr<UsbChooserContext> chooser_context_;
  base::ScopedObservation<UsbChooserContext, UsbChooserContext::DeviceObserver>
      observation_{this};

  // Each pair is a (device guid, device name).
  std::vector<std::pair<std::string, std::u16string>> devices_;
  // Maps from device name to number of devices.
  std::unordered_map<std::u16string, int> device_name_map_;
  base::WeakPtrFactory<UsbChooserController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_USB_USB_CHOOSER_CONTROLLER_H_
