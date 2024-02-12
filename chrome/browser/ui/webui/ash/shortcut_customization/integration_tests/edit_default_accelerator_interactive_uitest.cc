// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTestBase,
                       EditDefaultAccelerator) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator new_accel(ui::VKEY_N,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          OpenCalendarShortcutDialog(), EditDefaultShortcut(new_accel),
          Log("Setting Search + Ctrl + n as the default open/close calendar "
              "shortcut"),
          FocusWebContents(webcontents_id_), EnsureAcceleratorsAreProcessed(),
          SendAccelerator(webcontents_id_, new_accel),
          WaitForShow(kCalendarViewElementId),
          Log("New accelerator opens calendar"),
          SendShortcutAccelerator(new_accel),
          WaitForHide(kCalendarViewElementId),
          Log("New accelerator closes calendar"),
          SendShortcutAccelerator(default_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Default accelerator no longer opens the calendar"))));
}

}  // namespace
}  // namespace ash
