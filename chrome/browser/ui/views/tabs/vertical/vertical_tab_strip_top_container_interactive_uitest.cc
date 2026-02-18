// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace {

class VerticalTabStripTopContainerInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  auto SendTabSearchAccelerator() {
#if BUILDFLAG(IS_MAC)
    constexpr int kModifiers = ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN;
#else
    constexpr int kModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;
#endif
    return SendAccelerator(kBrowserViewElementId,
                           ui::Accelerator(ui::VKEY_A, kModifiers));
  }

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->profile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }
};

// This test checks that we can click the tab search button starting from the
// vertical tab strip and then switching to the horizontal layout.
IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerInteractiveUiTest,
                       VerifyTabSearchVerticalToHorizontal) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Ensure the tab search button is showing.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
      WaitForShow(kTabSearchButtonElementId),
      // Send Press to Vertical Tabs Tab Search Button.
      SendTabSearchAccelerator(), WaitForShow(kTabSearchBubbleElementId),
      // Display Horizontal Tabs.
      ExitVerticalTabsMode(), WaitForShow(kTabStripFrameGrabHandleElementId),
      EnsurePresent(kTabStripFrameGrabHandleElementId),
      // Send Press to Horizontal Tabs Tab Search Button
      SendTabSearchAccelerator(), WaitForShow(kTabSearchBubbleElementId));
}

// This test checks that we can click the tab search button starting from the
// horizontal tab strip and then switching to the vertical layout.
IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerInteractiveUiTest,
                       VerifyTabSearchHorizontalToVertical) {
  RunTestSequence(
      // Start with Horizontal Tabs Displayed
      ExitVerticalTabsMode(), WaitForShow(kTabStripFrameGrabHandleElementId),
      EnsurePresent(kTabStripFrameGrabHandleElementId),
      // Send Press to Horizontal Tabs Tab Search Button
      SendTabSearchAccelerator(), WaitForShow(kTabSearchBubbleElementId),
      SendKeyPress(kTabSearchBubbleElementId, ui::VKEY_ESCAPE),
      WaitForHide(kTabSearchBubbleElementId),
      // Display Vertical Tabs
      EnterVerticalTabsMode(),
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Ensure the tab search button is showing.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
      WaitForShow(kTabSearchButtonElementId),
      // Send Press to Vertical Tabs Tab Search Button
      SendTabSearchAccelerator(), WaitForShow(kTabSearchBubbleElementId));
}

// This test checks that we can click the collapse button in the vertical tab
// strip
IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerInteractiveUiTest,
                       VerifyCollapseButton) {
  RunTestSequence(
      // Verify not collapsed
      CheckResult(
          [this]() {
            return vertical_tab_strip_state_controller()->IsCollapsed();
          },
          false),
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripCollapseButtonElementId),
      // Press Collapse Button
      PressButton(kVerticalTabStripCollapseButtonElementId),
      // Verify collapsed
      CheckResult(
          [this]() {
            return vertical_tab_strip_state_controller()->IsCollapsed();
          },
          true));
}

// Checks that clicking the collapse button logs the correct metrics.
IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerInteractiveUiTest,
                       VerifyCollapseButtonMetrics) {
  base::UserActionTester user_action_tester;
  RunTestSequence(
      CheckResult(
          [this]() {
            return vertical_tab_strip_state_controller()->IsCollapsed();
          },
          false),
      Do([&]() {
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "VerticalTabs_TabStrip_ButtonToggleCollapsed"));
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "VerticalTabs_TabStrip_ButtonToggleUncollapsed"));
      }),
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripCollapseButtonElementId),
      PressButton(kVerticalTabStripCollapseButtonElementId),
      CheckResult(
          [this]() {
            return vertical_tab_strip_state_controller()->IsCollapsed();
          },
          true),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "VerticalTabs_TabStrip_ButtonToggleCollapsed"));
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "VerticalTabs_TabStrip_ButtonToggleUncollapsed"));
      }),
      PressButton(kVerticalTabStripCollapseButtonElementId),
      CheckResult(
          [this]() {
            return vertical_tab_strip_state_controller()->IsCollapsed();
          },
          false),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "VerticalTabs_TabStrip_ButtonToggleCollapsed"));
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "VerticalTabs_TabStrip_ButtonToggleUncollapsed"));
      }));
}

}  // namespace
