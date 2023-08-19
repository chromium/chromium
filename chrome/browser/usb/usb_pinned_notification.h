// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_PINNED_NOTIFICATION_H_
#define CHROME_BROWSER_USB_USB_PINNED_NOTIFICATION_H_

#include "chrome/browser/usb/usb_system_tray_icon.h"

class UsbPinnedNotification : public UsbSystemTrayIcon {
 public:
  UsbPinnedNotification();
  UsbPinnedNotification(const UsbPinnedNotification&) = delete;
  UsbPinnedNotification& operator=(const UsbPinnedNotification&) = delete;
  ~UsbPinnedNotification() override;
};

#endif  // CHROME_BROWSER_USB_USB_PINNED_NOTIFICATION_H_
