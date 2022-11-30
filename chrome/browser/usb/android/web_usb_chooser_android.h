// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_ANDROID_WEB_USB_CHOOSER_ANDROID_H_
#define CHROME_BROWSER_USB_ANDROID_WEB_USB_CHOOSER_ANDROID_H_

#include <memory>

#include "chrome/browser/usb/web_usb_chooser.h"

class UsbChooserController;
class UsbChooserDialogAndroid;

// Android implementation of the WebUsbChooser interface.
// This interface can be used by a webpage to request permission from user
// to access a certain device.
class WebUsbChooserAndroid : public WebUsbChooser {
 public:
  WebUsbChooserAndroid();

  WebUsbChooserAndroid(const WebUsbChooserAndroid&) = delete;
  WebUsbChooserAndroid& operator=(const WebUsbChooserAndroid&) = delete;

  ~WebUsbChooserAndroid() override;

  // WebUsbChooser implementation
  void ShowChooser(content::RenderFrameHost* render_frame_host,
                   std::unique_ptr<UsbChooserController> controller) override;

 private:
  void OnDialogClosed();

  std::unique_ptr<UsbChooserDialogAndroid> dialog_;
};

#endif  // CHROME_BROWSER_USB_ANDROID_WEB_USB_CHOOSER_ANDROID_H_
