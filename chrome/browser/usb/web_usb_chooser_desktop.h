// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_CHOOSER_DESKTOP_H_
#define CHROME_BROWSER_USB_WEB_USB_CHOOSER_DESKTOP_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "components/bubble/bubble_reference.h"

// Implementation of WebUsbChooser for desktop browsers that uses a bubble to
// display the permission prompt.
class WebUsbChooserDesktop : public WebUsbChooser {
 public:
  explicit WebUsbChooserDesktop(content::RenderFrameHost* render_frame_host);
  ~WebUsbChooserDesktop() override;

  // WebUsbChooser implementation
  void ShowChooser(std::unique_ptr<UsbChooserController> controller) override;

  base::WeakPtr<WebUsbChooser> GetWeakPtr() override;

 private:
  BubbleReference bubble_;

  base::WeakPtrFactory<WebUsbChooserDesktop> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebUsbChooserDesktop);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_CHOOSER_DESKTOP_H_
