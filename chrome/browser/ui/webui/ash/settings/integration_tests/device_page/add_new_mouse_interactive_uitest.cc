// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"

namespace ash {

namespace {

IN_PROC_BROWSER_TEST_F(DeviceSettingsBaseTest, AddNewMouse) {
  // Query to pierce through Shadow DOM to find the mouse row.
  const DeepQuery kMouseRowQuery{
      "os-settings-ui",       "os-settings-main",   "main-page-container",
      "settings-device-page", "#perDeviceMouseRow",
  };

  // Query to pierce through Shadow DOM to find the mouse header.
  const DeepQuery kMouseNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "h2#mouseName",
  };

  SetMouseDevices({ui::InputDevice(
      /*id=*/3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      Log("Waiting for Mouse to exist"),
      WaitForElementExists(webcontents_id_, kMouseNameQuery),
      WaitForElementTextContains(webcontents_id_, kMouseNameQuery, "mouse"));
}

}  // namespace
}  // namespace ash
