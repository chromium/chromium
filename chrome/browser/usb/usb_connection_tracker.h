// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CONNECTION_TRACKER_H_
#define CHROME_BROWSER_USB_USB_CONNECTION_TRACKER_H_

#include "chrome/browser/device_notifications/device_connection_tracker.h"

// Manages the opened device connection count by the profile.
class UsbConnectionTracker : public DeviceConnectionTracker {
 public:
  explicit UsbConnectionTracker(Profile* profile);
  UsbConnectionTracker(UsbConnectionTracker&&) = delete;
  UsbConnectionTracker& operator=(UsbConnectionTracker&) = delete;
  ~UsbConnectionTracker() override;

  void ShowContentSettingsExceptions() override;

  // KeyedService:
  void Shutdown() override;

 private:
  DeviceSystemTrayIcon* GetSystemTrayIcon() override;
};

#endif  // CHROME_BROWSER_USB_USB_CONNECTION_TRACKER_H_
