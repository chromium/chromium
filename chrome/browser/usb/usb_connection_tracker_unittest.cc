// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_connection_tracker_unittest.h"
#include "chrome/browser/usb/usb_connection_tracker_factory.h"
#include "chrome/browser/usb/usb_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

class UsbConnectionTrackerTest : public DeviceConnectionTrackerTestBase {
 public:
  UsbConnectionTrackerTest() = default;
  UsbConnectionTrackerTest(const UsbConnectionTrackerTest&) = delete;
  UsbConnectionTrackerTest& operator=(const UsbConnectionTrackerTest&) = delete;
  ~UsbConnectionTrackerTest() override = default;

  void SetUp() override {
    DeviceConnectionTrackerTestBase::SetUp();
    auto usb_system_tray_icon = std::make_unique<TestUsbSystemTrayIcon>();
    TestingBrowserProcess::GetGlobal()->SetUsbSystemTrayIcon(
        std::move(usb_system_tray_icon));
  }

  void TearDown() override {
    // Set the system tray icon to null to avoid uninteresting call to it during
    // profile destruction.
    TestingBrowserProcess::GetGlobal()->SetUsbSystemTrayIcon(nullptr);
    DeviceConnectionTrackerTestBase::TearDown();
  }

  DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                      bool create) override {
    return UsbConnectionTrackerFactory::GetForProfile(profile, create);
  }

  MockDeviceSystemTrayIcon* GetMockDeviceSystemTrayIcon() override {
    TestUsbSystemTrayIcon* test_usb_system_tray_icon =
        static_cast<TestUsbSystemTrayIcon*>(
            TestingBrowserProcess::GetGlobal()->usb_system_tray_icon());

    if (!test_usb_system_tray_icon) {
      return nullptr;
    }

    return test_usb_system_tray_icon->mock_device_system_tray_icon();
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(UsbConnectionTrackerTest, DeviceConnectionExtensionOrigins) {
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/true);
}

// Test the scenario with null USB system tray icon and it doesn't cause crash.
TEST_F(UsbConnectionTrackerTest,
       DeviceConnectionExtensionOriginsWithNullSystemTrayIcon) {
  TestingBrowserProcess::GetGlobal()->SetUsbSystemTrayIcon(nullptr);
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/false);
}

TEST_F(UsbConnectionTrackerTest, ProfileDestroyedExtensionOrigin) {
  TestProfileDestroyedExtensionOrigin();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
