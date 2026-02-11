// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace base::test {

class VerticalTabStripBottomContainerInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {};

// This test checks that we can click the new tab button in the bottom container
// of the vertical tab strip
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       VerifyNewTabButton) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  RunTestSequence(
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      Do([&]() {
        histogram_tester.ExpectTotalCount(
            "TabStrip.TimeToCreateNewTabFromPress", 0);
        EXPECT_EQ(0, user_action_tester.GetActionCount("NewTab_Button"));
      }),
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2),
      Do([&]() {
        histogram_tester.ExpectTotalCount(
            "TabStrip.TimeToCreateNewTabFromPress", 1);
        EXPECT_EQ(1, user_action_tester.GetActionCount("NewTab_Button"));
      }));
}

// This test checks that we can click the tab group button in the bottom
// container of the vertical tab strip
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       VerifyTabGroupButton) {
  RunTestSequence(
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kSavedTabGroupButtonElementId),
      PressButton(kSavedTabGroupButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      EnsurePresent(tab_groups::STGEverythingMenu::kCreateNewTabGroup),
      SelectMenuItem(tab_groups::STGEverythingMenu::kCreateNewTabGroup),
      WaitForShow(kTabGroupHeaderElementId),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2));
}

// This test checks that clicking the new tab button doesn't expose window
// caption on the vertical tab strip as it is moving down.
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       DoubleClickOnNewTabButtonDoesNotMaximizeWindow) {
  gfx::Point new_tab_button_center;
  gfx::Point point_above_new_tab_button;
  RunTestSequence(
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kNewTabButtonElementId),
      WithView(kNewTabButtonElementId,
               [&new_tab_button_center,
                &point_above_new_tab_button](views::View* button) {
                 new_tab_button_center =
                     button->GetBoundsInScreen().CenterPoint();
                 gfx::Rect bounds = button->GetBoundsInScreen();
                 gfx::Insets insets = button->GetInsets();
                 // A point just above the contents of the button, but within
                 // the extended hit test area (which includes the top inset).
                 point_above_new_tab_button = gfx::Point(
                     bounds.CenterPoint().x(), bounds.y() + insets.top() / 2);
               }),
      // Move mouse to the new tab button
      MoveMouseTo(kNewTabButtonElementId),
      // Click the new tab button
      ClickMouse(),
      // Check that the points are NO LONGER considered hit test caption
      CheckView(
          kVerticalTabStripRegionElementId,
          [&new_tab_button_center,
           &point_above_new_tab_button](views::View* region_view) {
            auto* vt_region_view =
                static_cast<VerticalTabStripRegionView*>(region_view);

            gfx::Point pt_center = new_tab_button_center;
            views::View::ConvertPointFromScreen(vt_region_view, &pt_center);

            gfx::Point pt_above = point_above_new_tab_button;
            views::View::ConvertPointFromScreen(vt_region_view, &pt_above);

            return !vt_region_view->IsPositionInWindowCaption(pt_center) &&
                   !vt_region_view->IsPositionInWindowCaption(pt_above);
          },
          "Check that clicking new tab does not expose caption space"));
}

}  // namespace base::test
