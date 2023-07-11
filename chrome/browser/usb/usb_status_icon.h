// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_STATUS_ICON_H_
#define CHROME_BROWSER_USB_USB_STATUS_ICON_H_

#include "chrome/browser/usb/usb_system_tray_icon.h"

class UsbStatusIcon : public UsbSystemTrayIcon {
 public:
  UsbStatusIcon();
  UsbStatusIcon(const UsbStatusIcon&) = delete;
  UsbStatusIcon& operator=(const UsbStatusIcon&) = delete;
  ~UsbStatusIcon() override;
};

#endif  // CHROME_BROWSER_USB_USB_STATUS_ICON_H_
