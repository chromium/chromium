// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_notifications/device_connection_tracker_test_base.h"
#include "chrome/browser/usb/usb_connection_tracker_factory.h"
#include "chrome/browser/usb/usb_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class UsbConnectionTrackerTest : public DeviceConnectionTrackerTestBase {
 public:
  UsbConnectionTrackerTest() = default;
  UsbConnectionTrackerTest(const UsbConnectionTrackerTest&) = delete;
  UsbConnectionTrackerTest& operator=(const UsbConnectionTrackerTest&) = delete;
  ~UsbConnectionTrackerTest() override = default;

  void SetUpOnMainThread() override {
    DeviceConnectionTrackerTestBase::SetUpOnMainThread();
    auto usb_system_tray_icon = std::make_unique<TestUsbSystemTrayIcon>();
    g_browser_process->set_usb_system_tray_icon_for_test(
        std::move(usb_system_tray_icon));
  }

  void TearDownOnMainThread() override {
    // Set the system tray icon to null to avoid uninteresting call to it during
    // profile destruction.
    g_browser_process->set_usb_system_tray_icon_for_test(nullptr);
    DeviceConnectionTrackerTestBase::TearDownOnMainThread();
  }

  DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                      bool create) override {
    return UsbConnectionTrackerFactory::GetForProfile(profile, create);
  }

  MockDeviceSystemTrayIcon* GetMockDeviceSystemTrayIcon() override {
    TestUsbSystemTrayIcon* test_usb_system_tray_icon =
        static_cast<TestUsbSystemTrayIcon*>(
            g_browser_process->usb_system_tray_icon());

    if (!test_usb_system_tray_icon) {
      return nullptr;
    }

    return test_usb_system_tray_icon->mock_device_system_tray_icon();
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(UsbConnectionTrackerTest,
                       DeviceConnectionExtensionOrigins) {
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/true);
}

// Test the scenario with null USB system tray icon and it doesn't cause crash.
IN_PROC_BROWSER_TEST_F(UsbConnectionTrackerTest,
                       DeviceConnectionExtensionOriginsWithNullSystemTrayIcon) {
  g_browser_process->set_usb_system_tray_icon_for_test(nullptr);
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/false);
}

IN_PROC_BROWSER_TEST_F(UsbConnectionTrackerTest,
                       ProfileDestroyedExtensionOrigin) {
  TestProfileDestroyedExtensionOrigin();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
