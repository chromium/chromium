// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

namespace {

const ui::TouchpadDevice kSampleTouchpadInternal(1,
                                                 ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleTouchpadInternal",
                                                 "",
                                                 base::FilePath(),
                                                 0x1111,
                                                 0x4444,
                                                 0);

class DeviceSettingsTouchpadInteractiveUiTest : public DeviceSettingsBaseTest {
 public:
  // Query to pierce through Shadow DOM to find the touchpad row.
  const DeepQuery kTouchpadRowQuery{
      "os-settings-ui",       "os-settings-main",      "main-page-container",
      "settings-device-page", "#perDeviceTouchpadRow",
  };

  // Query to pierce through Shadow DOM to find the touchpad header.
  const DeepQuery kTouchpadNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-touchpad",
      "settings-per-device-touchpad-subsection",
      "h2#touchpadName",
  };
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsTouchpadInteractiveUiTest,
                       OpenTouchpadSubpage) {
  SetTouchpadDevices({kSampleTouchpadInternal});
  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kTouchpadRowQuery),
      ClickElement(webcontents_id_, kTouchpadRowQuery),
      WaitForElementTextContains(webcontents_id_, kTouchpadNameQuery,
                                 "Built-in Touchpad"));
}

}  // namespace
}  // namespace ash
