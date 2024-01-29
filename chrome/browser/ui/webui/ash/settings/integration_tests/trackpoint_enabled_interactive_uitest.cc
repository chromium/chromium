// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

namespace {

constexpr char kDeviceSectionPath[] = "device";
const ui::InputDevice kSamplePointingStickInternal(
    2,
    ui::INPUT_DEVICE_INTERNAL,
    "kSamplePointingStickInternal");

class DeviceSettingsTrackpointInteractiveUiTest : public InteractiveAshTest {
 public:
  DeviceSettingsTrackpointInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(features::kInputDeviceSettingsSplit);
  }

  // Query to pierce through Shadow DOM to find the pointing stick row.
  const DeepQuery kPointingStickRowQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "#perDevicePointingStickRow",
  };

  // Query to pierce through Shadow DOM to find the pointing stick header.
  const DeepQuery kPointingStickNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-pointing-stick",
      "settings-per-device-pointing-stick-subsection",
      "h2#pointingStickName",
  };

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    // Initialize pointing stick.
    ui::DeviceDataManagerTestApi().SetPointingStickDevices(
        {kSamplePointingStickInternal});
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsTrackpointInteractiveUiTest,
                       TrackpointEnabled) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  RunTestSequence(
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(), kDeviceSectionPath);
      }),
      WaitForShow(kOsSettingsWebContentsId),
      WaitForWebContentsReady(kOsSettingsWebContentsId,
                              chrome::GetOSSettingsUrl(kDeviceSectionPath)),
      WaitForElementExists(kOsSettingsWebContentsId, kPointingStickRowQuery),
      ClickElement(kOsSettingsWebContentsId, kPointingStickRowQuery),
      WaitForElementTextContains(kOsSettingsWebContentsId,
                                 kPointingStickNameQuery,
                                 "Built-in TrackPoint"));
}

}  // namespace
}  // namespace ash
