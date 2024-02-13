// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTestBase,
                       AddAcceleratorDisruptive) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator feedback_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kOpenFeedbackPage);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsFeedbackWebContentsId);

  const DeepQuery kErrorMessageConflictQuery{
      "shortcut-customization-app",
      "#editDialog",
      "accelerator-edit-view",
      "#acceleratorInfoText",
  };

  const DeepQuery kCancelButtonQuery{"shortcut-customization-app",
                                     "#editDialog", "#pendingAccelerator",
                                     "#cancelButton"};

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          OpenCalendarShortcutDialog(), ClickAddShortcutButton(),
          Log("Attempting to Add Alt + Shift + I as a custom open/close "
              "calendar shortcut"),
          SendAccelerator(webcontents_id_, feedback_accel),
          Log("Verifying the error message for a locked accelerator is shown"),
          EnsurePresent(webcontents_id_, kErrorMessageConflictQuery),
          Log("Clicking cancel button to reset edit dialog state"),
          ExecuteJsAt(webcontents_id_, kCancelButtonQuery, kClickFn),
          Log("Closing dialog"), ClickDoneButton(),
          InstrumentNextTab(kOsFeedbackWebContentsId, AnyBrowser()),
          SendShortcutAccelerator(feedback_accel),
          Log("Verifying that 'Open feedback tool' accelerator still works"),
          WaitForShow(kOsFeedbackWebContentsId))));
}

}  // namespace
}  // namespace ash
