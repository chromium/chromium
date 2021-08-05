// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_CHOOSER_H_
#define CHROME_BROWSER_USB_WEB_USB_CHOOSER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"

namespace content {
class RenderFrameHost;
}

class UsbChooserController;

// This interface can be used by a webpage to request permission from user
// to access a certain device.
class WebUsbChooser {
 public:
  explicit WebUsbChooser(content::RenderFrameHost* render_frame_host);

  virtual ~WebUsbChooser();

  void GetPermission(
      std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
      blink::mojom::WebUsbService::GetPermissionCallback callback);

  virtual void ShowChooser(
      std::unique_ptr<UsbChooserController> controller) = 0;

  virtual base::WeakPtr<WebUsbChooser> GetWeakPtr() = 0;

  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

 private:
  content::RenderFrameHost* const render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbChooser);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_CHOOSER_H_
