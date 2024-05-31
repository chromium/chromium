// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/test/gtest_tags.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"

namespace ash {
namespace {

using AppListInteractiveUiTest = InteractiveAshTest;

// Basic smoke test of the bubble launcher.
// Contacts: chromeos-launcher@google.com, chromeos-sw-engprod@google.com
// Ported from Tast by: jamescook@chromium.org
// BugComponent: b:1288350
IN_PROC_BROWSER_TEST_F(AppListInteractiveUiTest, BubbleSmoke) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-90e4fecc-d2ea-40dc-b9db-eb9d61089e22");

  SetupContextWidget();

  const gfx::Point screen_origin(0, 0);
  const ui::Accelerator escape_key(ui::VKEY_ESCAPE, ui::EF_NONE);

  RunTestSequence(
      // Clicking the home (launcher) button once opens the bubble widget, which
      // shows the bubble view.
      Log("Clicking home button"), PressButton(kHomeButtonElementId),
      WaitForShow(kAppListBubbleViewElementId),

      // Clicking in the corner of the screen closes the bubble widget and view.
      Log("Clicking screen corner"), MoveMouseTo(screen_origin), ClickMouse(),
      WaitForHide(kAppListBubbleViewElementId),

      // Pressing the Search key opens the bubble. Don't use SendAccelerator()
      // because there's no target element.
      Log("Pressing search key"), Do([]() {
        ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_COMMAND,
                                  /*control=*/false, /*shift=*/false,
                                  /*alt=*/false, /*command=*/false);
      }),
      WaitForShow(kAppListBubbleViewElementId),

      // Typing the escape key closes the bubble.
      Log("Pressing escape"),
      SendAccelerator(kAppListBubbleViewElementId, escape_key),
      WaitForHide(kAppListBubbleViewElementId));
}

}  // namespace
}  // namespace ash
