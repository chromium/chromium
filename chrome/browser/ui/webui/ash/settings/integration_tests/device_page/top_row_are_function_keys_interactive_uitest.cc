// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {

IN_PROC_BROWSER_TEST_F(DeviceSettingsBaseTest, TopRow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsExploreAppWebbContentsId);

  // Query to pierce through Shadow DOM to find the
  // 'Treat top-row keys as function keys' toggle.
  const DeepQuery kTopRowAreFkeysToggleQuery{{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-keyboard",
      "settings-per-device-keyboard-subsection",
      "settings-toggle-button#internalTopRowAreFunctionKeysButton",
  }};

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTopRowAreFkeysEvent);
  StateChange top_row_are_fkeys;
  top_row_are_fkeys.type = StateChange::Type::kExistsAndConditionTrue;
  top_row_are_fkeys.event = kTopRowAreFkeysEvent;
  top_row_are_fkeys.where = kTopRowAreFkeysToggleQuery;
  top_row_are_fkeys.test_function = "btn => btn.checked";

  RunTestSequence(
      Log("Adding a fake internal keyboard"), SetupInternalKeyboard(),
      LaunchSettingsApp(
          webcontents_id_,
          chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath),
      Log("Enabling 'Treat top-row keys as function keys' setting"),
      ClickElement(webcontents_id_, kTopRowAreFkeysToggleQuery),
      WaitForStateChange(webcontents_id_, top_row_are_fkeys),
      Log("Verifying that the top row back button opens the Explore app"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_F1),
      InstrumentNextTab(kOsExploreAppWebbContentsId, AnyBrowser()),
      WaitForShow(kOsExploreAppWebbContentsId));
}

}  // namespace ash
