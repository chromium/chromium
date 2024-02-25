// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

const ui::InputDevice kMouse(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse");

class DeviceSettingsMouseInteractiveUiTest : public DeviceSettingsBaseTest {
 public:
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
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsMouseInteractiveUiTest,
                       MouseScrollAcceleration) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollingSpeedSliderDisabledEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollingSpeedSliderEnabledEvent);
  SetMouseDevices({kMouse});

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
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kMouseRowQuery),
      ClickElement(webcontents_id_, kMouseRowQuery),
      WaitForStateChange(webcontents_id_, scrolling_speed_slider_disabled),
      ClickElement(webcontents_id_, kControlledScrollingButtonQuery),
      WaitForStateChange(webcontents_id_, scrolling_speed_slider_enabled));
}

}  // namespace
}  // namespace ash
