// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_TEST_UTILS_H_
#define CHROME_BROWSER_USB_USB_TEST_UTILS_H_

#include "chrome/browser/device_notifications/device_test_utils.h"
#include "chrome/browser/usb/usb_connection_tracker.h"
#include "chrome/browser/usb/usb_system_tray_icon.h"

// This is a fake UsbConnectionTracker for testing.
// Test code can use `mock_device_connection_tracker()` to test method calls to
// `DeviceConnectionTracker`.
class TestUsbConnectionTracker : public UsbConnectionTracker {
 public:
  explicit TestUsbConnectionTracker(Profile* profile);
  TestUsbConnectionTracker(const TestUsbConnectionTracker&) = delete;
  TestUsbConnectionTracker& operator=(const TestUsbConnectionTracker&) = delete;
  ~TestUsbConnectionTracker() override;

  void ShowContentSettingsExceptions() override;
  void ShowSiteSettings(const url::Origin& origin) override;
  MockDeviceConnectionTracker* mock_device_connection_tracker() {
    return &mock_device_connection_tracker_;
  }

 private:
  MockDeviceConnectionTracker mock_device_connection_tracker_;
};

// This is a fake UsbSystemTrayIcon for testing.
// Test code can use `mock_device_system_tray_icon()` to test method calls to
// `DeviceSystemTrayIcon`.
class TestUsbSystemTrayIcon : public UsbSystemTrayIcon {
 public:
  TestUsbSystemTrayIcon();
  TestUsbSystemTrayIcon(const TestUsbSystemTrayIcon&) = delete;
  TestUsbSystemTrayIcon& operator=(const TestUsbSystemTrayIcon&) = delete;
  ~TestUsbSystemTrayIcon() override;

  void StageProfile(Profile* profile) override;
  void UnstageProfile(Profile* profile, bool immediate) override;
  void ProfileAdded(Profile* profile) override;
  void ProfileRemoved(Profile* profile) override;
  void NotifyConnectionCountUpdated(Profile* profile) override;

  MockDeviceSystemTrayIcon* mock_device_system_tray_icon() {
    return &mock_device_system_tray_icon_;
  }

 private:
  MockDeviceSystemTrayIcon mock_device_system_tray_icon_;
};

#endif  // CHROME_BROWSER_USB_USB_TEST_UTILS_H_
