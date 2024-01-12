// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>

#include "ash/ash_element_identifiers.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/shortcut_customization/shortcut_customization_test_base.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTestBase,
                       AddAcceleratorWithConflict) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator new_accel(ui::VKEY_S,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShortcutAppWebContentsId);
  webcontents_id_ = kShortcutAppWebContentsId;

  const DeepQuery kErrorMessageConflictQuery{
        "shortcut-customization-app",
        "#editDialog",
        "accelerator-edit-view",
        "#acceleratorInfoText",
   };

  RunTestSequence(
      InstrumentNextTab(kShortcutAppWebContentsId, AnyBrowser()),
      LaunchShortcutCustomizationApp(),
      WaitForWebContentsReady(kShortcutAppWebContentsId,
                              GURL("chrome://shortcut-customization")),
      InAnyContext(Steps(
          OpenCalendarShortcutDialog(),
          ClickAddShortcutButton(),
          SendAccelerator(webcontents_id_, new_accel),
          Log("Attempting to Add Search + Ctrl + s as a custom open/close calendar "
              "shortcut"),
          EnsurePresent(webcontents_id_, kErrorMessageConflictQuery),
          Log("Verifying the conflict error message is shown"),
          SendAccelerator(webcontents_id_, new_accel),
          ClickDoneButton(),
          Log("Pressed the shortcut again to bypass the warning message"),
          EnsureAcceleratorsAreProcessed(),
          SendAccelerator(webcontents_id_, new_accel),
          WaitForShow(kCalendarViewElementId),
          Log("New accelerator opens calendar"),
          SendShortcutAccelerator(new_accel),
          WaitForHide(kCalendarViewElementId),
          Log("New accelerator closes calendar"),
          SendShortcutAccelerator(default_accel),
          EnsurePresent(kCalendarViewElementId),
          Log("Default accelerator also opens the calendar"))));
}

}  // namespace
}  // namespace ash
