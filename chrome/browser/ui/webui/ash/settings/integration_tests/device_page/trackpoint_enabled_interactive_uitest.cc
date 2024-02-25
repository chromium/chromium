// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"

namespace ash {

namespace {

const ui::InputDevice kSamplePointingStickInternal(
    2,
    ui::INPUT_DEVICE_INTERNAL,
    "kSamplePointingStickInternal");

class DeviceSettingsTrackpointInteractiveUiTest
    : public DeviceSettingsBaseTest {
 public:
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
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsTrackpointInteractiveUiTest,
                       TrackpointEnabled) {
  SetPointingStickDevices({kSamplePointingStickInternal});
  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kPointingStickRowQuery),
      ClickElement(webcontents_id_, kPointingStickRowQuery),
      WaitForElementTextContains(webcontents_id_, kPointingStickNameQuery,
                                 "Built-in TrackPoint"));
}

}  // namespace
}  // namespace ash
