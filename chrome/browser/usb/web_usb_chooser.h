// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_CHOOSER_H_
#define CHROME_BROWSER_USB_WEB_USB_CHOOSER_H_

#include <memory>

#include "content/public/browser/usb_chooser.h"

namespace content {
class RenderFrameHost;
}

class UsbChooserController;

// This interface can be used by a webpage to request permission from user
// to access a certain device.
class WebUsbChooser : public content::UsbChooser {
 public:
  static std::unique_ptr<WebUsbChooser> Create(
      content::RenderFrameHost* frame,
      std::unique_ptr<UsbChooserController> controller);

  WebUsbChooser(const WebUsbChooser&) = delete;
  WebUsbChooser& operator=(const WebUsbChooser&) = delete;

  ~WebUsbChooser() override;

 protected:
  WebUsbChooser();

  virtual void ShowChooser(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<UsbChooserController> controller) = 0;
};

#endif  // CHROME_BROWSER_USB_WEB_USB_CHOOSER_H_
