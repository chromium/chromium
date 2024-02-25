// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"

namespace ash {

namespace {

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTestBase,
                       ShortcutOnKeyboardPluggedIn) {
  const DeepQuery kToggleLauncherAcceleratorQuery{
      "shortcut-customization-app",
      "#navigationPanel",
      "#category-0",
      "#contentWrapper > accelerator-subsection:nth-child(1)",
      "#rowList > accelerator-row:nth-child(1)",
      "#container > td",
  };

  RunTestSequence(
      AddKeyboard(/*is_external=*/false), LaunchShortcutCustomizationApp(),
      Log("Verifying that 'Open/close Launcher' shortcut contains 1 "
          "accelerator"),
      WaitForElementExists(webcontents_id_, kToggleLauncherAcceleratorQuery),
      WaitForShortcutToContainNumAcceleartors(kToggleLauncherAcceleratorQuery,
                                              /*expected=*/1),
      AddKeyboard(/*is_external=*/true),
      Log("Verifying that 'Open/close Launcher' shortcut now contains 2 "
          "accelerators since an external keyboard has been added"),
      WaitForShortcutToContainNumAcceleartors(kToggleLauncherAcceleratorQuery,
                                              /*expected=*/2));
}

}  // namespace
}  // namespace ash
