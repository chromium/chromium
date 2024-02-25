// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"

namespace ash {

namespace {

IN_PROC_BROWSER_TEST_F(DeviceSettingsBaseTest, AddNewKeyboard) {
  RunTestSequence(SetupInternalKeyboard(),
                  LaunchSettingsApp(
                      webcontents_id_,
                      chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath),
                  Log("Waiting for keyboard to exist"),
                  WaitForElementExists(webcontents_id_, kKeyboardNameQuery),
                  CheckJsResultAt(webcontents_id_, kKeyboardNameQuery,
                                  "el => el.innerText", "Built-in Keyboard"),
                  Log("Test complete"));
}

}  // namespace
}  // namespace ash
