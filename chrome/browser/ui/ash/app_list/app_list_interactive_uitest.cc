// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace ash {
namespace {

class AppListInteractiveTest : public InteractiveBrowserTest {
 public:
  AppListInteractiveTest() {
    // This test suite does not require a browser window.
    set_launch_browser_for_testing(nullptr);

    // Give all widgets the same Kombucha context.This is useful for ash system
    // UI because the UI uses a variety of small widgets. Note that if this test
    // used multiple displays we would need to provide a different context per
    // display (i.e. the widget's native window's root window). Elements like
    // the home button, shelf, etc. appear once per display.
    views::ElementTrackerViews::SetContextOverrideCallback(
        base::BindRepeating([](views::Widget* widget) {
          return ui::ElementContext(Shell::GetPrimaryRootWindow());
        }));
  }
};

IN_PROC_BROWSER_TEST_F(AppListInteractiveTest, OpenAndClose) {
  // Kombucha needs a widget to be able to click on things.
  views::Widget* navigation_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->navigation_widget();
  SetContextWidget(navigation_widget);

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
