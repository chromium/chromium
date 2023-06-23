// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {
constexpr char kAppListElementName[] = "App List Element";
}

class AppListInteractiveTest : public InteractiveBrowserTest {
 public:
  AppListInteractiveTest() {
    // This test suite does not require a browser window.
    set_launch_browser_for_testing(nullptr);
  }
};

IN_PROC_BROWSER_TEST_F(AppListInteractiveTest, OpenAndClose) {
  views::Widget* navigation_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->navigation_widget();
  SetContextWidget(navigation_widget);

  RunTestSequence(
      // Clicking the home (launcher) button once opens the bubble widget, which
      // shows the bubble view.
      PressButton(kHomeButtonElementId),

      // Verify the view is shown and name the element to capture its context.
      InAnyContext(AfterShow(
          kAppListBubbleViewElementId,
          [](ui::InteractionSequence* sequence, ui::TrackedElement* el) {
            sequence->NameElement(el, kAppListElementName);
          })),

      // Clicking the home (launcher) button again hides the bubble widget,
      // which hides the bubble view.
      PressButton(kHomeButtonElementId),

      // Use the element name because we're not in the same context as the home
      // button.
      WaitForHide(kAppListElementName));
}

}  // namespace ash
