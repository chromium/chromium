// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

constexpr char kDeviceSectionPath[] = "device";

class DeviceSettingsMouseInteractiveUiTest : public InteractiveAshTest {
 public:
  DeviceSettingsMouseInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(features::kInputDeviceSettingsSplit);
  }

  // Query to pierce through Shadow DOM to find the mouse row.
  const DeepQuery kMouseRowQuery{
      "os-settings-ui",       "os-settings-main",   "main-page-container",
      "settings-device-page", "#perDeviceMouseRow",
  };

  const DeepQuery kScrollingSpeedSliderQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseScrollSpeedSlider",
  };

  const DeepQuery kControlledScrollingButtonQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseControlledScrolling",
  };

  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    // Initialize mouse.
    ui::DeviceDataManagerTestApi().SetMouseDevices(
        {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsMouseInteractiveUiTest,
                       MouseScrollAcceleration) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollingSpeedSliderDisabledEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollingSpeedSliderEnabledEvent);

  StateChange scrolling_speed_slider_disabled;
  scrolling_speed_slider_disabled.type =
      StateChange::Type::kExistsAndConditionTrue;
  scrolling_speed_slider_disabled.event = kScrollingSpeedSliderDisabledEvent;
  scrolling_speed_slider_disabled.where = kScrollingSpeedSliderQuery;
  scrolling_speed_slider_disabled.test_function = "el => el.disabled";

  StateChange scrolling_speed_slider_enabled;
  scrolling_speed_slider_enabled.type =
      StateChange::Type::kExistsAndConditionTrue;
  scrolling_speed_slider_enabled.event = kScrollingSpeedSliderEnabledEvent;
  scrolling_speed_slider_enabled.where = kScrollingSpeedSliderQuery;
  scrolling_speed_slider_enabled.test_function = "el => !el.disabled";

  RunTestSequence(
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(), kDeviceSectionPath);
      }),
      WaitForShow(kOsSettingsWebContentsId),
      Log("Waiting for per device section to load"),
      WaitForWebContentsReady(kOsSettingsWebContentsId,
                              chrome::GetOSSettingsUrl(kDeviceSectionPath)),
      WaitForElementExists(kOsSettingsWebContentsId, kMouseRowQuery),
      ClickElement(kOsSettingsWebContentsId, kMouseRowQuery),
      WaitForStateChange(kOsSettingsWebContentsId,
                         scrolling_speed_slider_disabled),
      ClickElement(kOsSettingsWebContentsId, kControlledScrollingButtonQuery),
      WaitForStateChange(kOsSettingsWebContentsId,
                         scrolling_speed_slider_enabled));
}

}  // namespace
}  // namespace ash
