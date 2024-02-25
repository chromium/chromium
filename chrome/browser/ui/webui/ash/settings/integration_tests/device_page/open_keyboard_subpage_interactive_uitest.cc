// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"

namespace ash {

namespace {

class DeviceSettingsKeyboardInteractiveUiTest : public DeviceSettingsBaseTest {
 public:
  // Query to pierce through Shadow DOM to find the touchpad row.
  const DeepQuery kKeyboardRowQuery{
      "os-settings-ui",       "os-settings-main",      "main-page-container",
      "settings-device-page", "#perDeviceKeyboardRow",
  };
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsKeyboardInteractiveUiTest,
                       OpenKeyboardSubpage) {
  RunTestSequence(
      SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kKeyboardRowQuery),
      ClickElement(webcontents_id_, kKeyboardRowQuery),
      WaitForElementTextContains(webcontents_id_, kKeyboardNameQuery,
                                 "Built-in Keyboard"));
}

}  // namespace
}  // namespace ash
