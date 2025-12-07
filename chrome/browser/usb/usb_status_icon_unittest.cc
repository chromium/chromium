// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_status_icon.h"

#include <string>

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/device_notifications/device_status_icon_unittest.h"
#include "chrome/browser/usb/usb_connection_tracker.h"
#include "chrome/browser/usb/usb_connection_tracker_factory.h"
#include "chrome/browser/usb/usb_system_tray_icon.h"
#include "chrome/browser/usb/usb_test_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class UsbStatusIconTest : public DeviceStatusIconTestBase {
 public:
  UsbStatusIconTest()
      : DeviceStatusIconTestBase(
            /*about_device_label=*/u"About USB devices",
            /*device_content_settings_label_=*/u"USB settings") {}

  void ResetTestingBrowserProcessSystemTrayIcon() override {
    TestingBrowserProcess::GetGlobal()->SetUsbSystemTrayIcon(nullptr);
  }

  std::u16string GetExpectedTitle(size_t num_origins,
                                  size_t num_connections) override {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // The text might use "Google Chrome" or "Chromium" depending
    // is_chrome_branded in the build config file, hence using l10n_util to get
    // the expected string.
    return l10n_util::GetPluralStringFUTF16(IDS_WEBUSB_SYSTEM_TRAY_ICON_TITLE,
                                            static_cast<int>(num_connections));
#else
    NOTREACHED();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }

  void SetDeviceConnectionTrackerTestingFactory(Profile* profile) override {
    UsbConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindRepeating([](content::BrowserContext* browser_context) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<TestUsbConnectionTracker>(
                  Profile::FromBrowserContext(browser_context)));
        }));
  }

  DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                      bool create) override {
    return static_cast<DeviceConnectionTracker*>(
        UsbConnectionTrackerFactory::GetForProfile(profile, create));
  }

  MockDeviceConnectionTracker* GetMockDeviceConnectionTracker(
      DeviceConnectionTracker* connection_tracker) override {
    return static_cast<TestUsbConnectionTracker*>(connection_tracker)
        ->mock_device_connection_tracker();
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(UsbStatusIconTest, SingleProfileEmptyNameExtentionOrigins) {
  // Current TestingProfileManager can't support empty profile name as it uses
  // profile name for profile path. Passing empty would result in a failure in
  // ProfileManager::IsAllowedProfilePath(). Changing the way
  // TestingProfileManager creating profile path like adding "profile" prefix
  // doesn't work either as some tests are written in a way that takes
  // assumption of testing profile path pattern. Hence it creates testing
  // profile with non-empty name and then change the profile name to empty which
  // can still achieve what this file wants to test.
  profile()->set_profile_name("");
  TestSingleProfileExtentionOrigins();
}

TEST_F(UsbStatusIconTest, SingleProfileNonEmptyNameExtentionOrigins) {
  TestSingleProfileExtentionOrigins();
}

TEST_F(UsbStatusIconTest, BounceConnectionExtensionOrigins) {
  TestBounceConnectionExtensionOrigins();
}

TEST_F(UsbStatusIconTest, MultipleProfilesExtensionOrigins) {
  TestMultipleProfilesExtensionOrigins();
}

TEST_F(UsbStatusIconTest, NumCommandIdOverLimitExtensionOrigin) {
  TestNumCommandIdOverLimitExtensionOrigin();
}

TEST_F(UsbStatusIconTest, ExtensionRemoval) {
  TestExtensionRemoval();
}

TEST_F(UsbStatusIconTest, ProfileUserNameExtensionOrigin) {
  TestProfileUserNameExtensionOrigin();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
