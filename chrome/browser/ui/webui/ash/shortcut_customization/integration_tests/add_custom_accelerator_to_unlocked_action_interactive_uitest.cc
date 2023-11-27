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

class AddCustomAcceleratorToUnlockedActionInteractiveUiTest
    : public ShortcutCustomizationInteractiveUiTestBase {
 public:
  const DeepQuery kCustomAcceleratorViewQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-0",
      "#container",
      "accelerator-subsection",
      "tbody#rowList",
      // Action 93 corresponds to the "Open/Close Calendar" shortcut.
      "accelerator-row[action='93']",
      "#container > td > accelerator-view:nth-child(2)",
  };
};

IN_PROC_BROWSER_TEST_F(AddCustomAcceleratorToUnlockedActionInteractiveUiTest,
                       AddCustomAcceleratorToUnlockedAction) {
  ui::Accelerator default_accel(ui::VKEY_C, ui::EF_COMMAND_DOWN);
  ui::Accelerator new_accel(ui::VKEY_N,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShortcutAppWebContentsId);
  webcontents_id_ = kShortcutAppWebContentsId;
  RunTestSequence(
      InstrumentNextTab(kShortcutAppWebContentsId, AnyBrowser()),
      LaunchShortcutCustomizationApp(),
      WaitForWebContentsReady(kShortcutAppWebContentsId,
                              GURL("chrome://shortcut-customization")),
      InAnyContext(Steps(
          SendShortcutAccelerator(new_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Verify that the custom shortcut does not open the calendar "
              "before it's added as a shortcut"),
          OpenCalendarShortcutDialog(), AddCustomCalendarShortcut(new_accel),
          FocusWebContents(webcontents_id_), EnsureAcceleratorsAreProcessed(),
          Log("Adding Search + Ctrl + n as a custom open/close calendar "
              "shortcut"),
          EnsurePresent(webcontents_id_, kCustomAcceleratorViewQuery),
          Log("New shortcut is present in the UI"),
          SendShortcutAccelerator(new_accel),
          WaitForShow(kCalendarViewElementId),
          Log("Custom shortcut opens calendar"),
          SendShortcutAccelerator(new_accel),
          WaitForHide(kCalendarViewElementId),
          Log("Custom shortcut closes calendar"), ResetCalendarShortcuts(),
          Log("Open/Close calendar shortcut reset to defaults"),
          EnsureAcceleratorsAreProcessed(),
          SendShortcutAccelerator(default_accel),
          WaitForShow(kCalendarViewElementId),
          SendShortcutAccelerator(default_accel),
          WaitForHide(kCalendarViewElementId),
          Log("Default shortcut still works"),
          SendShortcutAccelerator(new_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Custom shortcut no longer works"))));
}

}  // namespace
}  // namespace ash
