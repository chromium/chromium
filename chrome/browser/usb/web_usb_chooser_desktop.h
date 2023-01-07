// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_CHOOSER_DESKTOP_H_
#define CHROME_BROWSER_USB_WEB_USB_CHOOSER_DESKTOP_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/usb/web_usb_chooser.h"

// Implementation of WebUsbChooser for desktop browsers that uses a bubble to
// display the permission prompt.
class WebUsbChooserDesktop : public WebUsbChooser {
 public:
  WebUsbChooserDesktop();

  WebUsbChooserDesktop(const WebUsbChooserDesktop&) = delete;
  WebUsbChooserDesktop& operator=(const WebUsbChooserDesktop&) = delete;

  ~WebUsbChooserDesktop() override;

  // WebUsbChooser implementation
  void ShowChooser(content::RenderFrameHost* render_frame_host,
                   std::unique_ptr<UsbChooserController> controller) override;

 private:
  base::ScopedClosureRunner closure_runner_;
};

#endif  // CHROME_BROWSER_USB_WEB_USB_CHOOSER_DESKTOP_H_
