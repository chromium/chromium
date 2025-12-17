// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace {

const char kFirstTabName[] = "FirstTab";
const char kSecondTabName[] = "SecondTab";
const char kThirdTabName[] = "ThirdTab";
const int kShift = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_SHIFT_DOWN;

class VerticalTabStripControllerInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  bool CheckMenuHasStringId(int message_id) {
    ui::SimpleMenuModel* menu_model = vertical_tab_strip_controller()
                                          ->GetTabContextMenuController()
                                          ->GetMenuModel();
    for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
      if (l10n_util::GetStringUTF16(message_id) == menu_model->GetLabelAt(i)) {
        return true;
      }
    }
    return false;
  }

  int GetPlatformDependentAccelerator() {
#if BUILDFLAG(IS_MAC)
    return ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN;
#else
    return ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN;
#endif
  }

  base::OnceCallback<void(views::View*)> ClickWithFlags(int flags) {
    return base::BindOnce(
        [](int flags, views::View* view) {
          ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), base::TimeTicks::Now(), flags,
                               ui::EF_LEFT_MOUSE_BUTTON);
          view->OnMousePressed(event);
          ui::MouseEvent release_event(ui::EventType::kMouseReleased,
                                       gfx::Point(), gfx::Point(),
                                       base::TimeTicks::Now(), flags,
                                       ui::EF_LEFT_MOUSE_BUTTON);
          view->OnMouseReleased(event);
        },
        flags);
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       VerifyTabSelection) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Create a second tab.
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      // Name views so we can interact with them.
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kFirstTabName, 0),
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kSecondTabName, 1),
      // Verify active tab is at index 1.
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 1),
      // Select tab at index 0 and verify active index.
      MoveMouseTo(kFirstTabName), ClickMouse(ui_controls::LEFT),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); },
          0));
}

// TODO(crbug.com/466106773): Unable to click middle mouse button on MacOS.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VerifyClosingTabWithMiddleMouseButton \
  DISABLED_VerifyClosingTabWithMiddleMouseButton
#else
#define MAYBE_VerifyClosingTabWithMiddleMouseButton \
  VerifyClosingTabWithMiddleMouseButton
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       MAYBE_VerifyClosingTabWithMiddleMouseButton) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Create a second tab.
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2),
      // Name views so we can interact with them.
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kFirstTabName, 0),
      // Close tab at index 0 w/middle mouse button and verify tab count.
      MoveMouseTo(kFirstTabName), ClickMouse(ui_controls::MIDDLE),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      WaitForHide(kFirstTabName));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       ShiftMultiTabSelection) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Create three tabs.
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      // Name views so we can interact with them.
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kFirstTabName, 0),
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kSecondTabName, 1),
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kThirdTabName, 2),
      // Set Tab 2 to be active.
      MoveMouseTo(kSecondTabName), ClickMouse(ui_controls::LEFT),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 1),
      // Shift + Click Tab 3.
      WithView(kThirdTabName, ClickWithFlags(kShift)),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(0); },
          false),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(1); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(2); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 2),
      // Ctrl/Command + Shift + Click Tab 1.
      WithView(kFirstTabName,
               ClickWithFlags(kShift | GetPlatformDependentAccelerator())),
      // Verify all Tabs are selected, Tab 1 is active.
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(0); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(1); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(2); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); },
          0));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       ToggleTabSelection) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Create a second tab.
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      // Name views so we can interact with them.
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kFirstTabName, 0),
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kSecondTabName, 1),
      // Set Tab 1 to be active.
      MoveMouseTo(kFirstTabName), ClickMouse(ui_controls::LEFT),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 0),
      // Shift + Click Tab 2.
      WithView(kSecondTabName,
               ClickWithFlags(GetPlatformDependentAccelerator())),
      // Verify both tabs are selected, but tab 1 is active.
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(0); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(1); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 1),
      // Shift + Click Tab 2.
      WithView(kSecondTabName,
               ClickWithFlags(GetPlatformDependentAccelerator())),
      // Verify only tab 1 is selected and active.
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(0); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(1); },
          false),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); },
          0));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       KeyboardTabSelection) {
  ui::Accelerator previous_tab_accelerator, next_tab_accelerator;

  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SELECT_NEXT_TAB, &previous_tab_accelerator));
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SELECT_PREVIOUS_TAB, &next_tab_accelerator));

  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Create a second tab.
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      // Move to left (Tab 0) and verify active index.
      SendAccelerator(kBrowserViewElementId, previous_tab_accelerator),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 0),
      // Move to right (Tab 1) and verify active index.
      SendAccelerator(kBrowserViewElementId, next_tab_accelerator),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); },
          1));
}

// TODO(crbug.com/466391046): Tab Group Accelerators are not defined on
// ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_KeyboardTabGroupCommands DISABLED_KeyboardTabGroupCommands
#else
#define MAYBE_KeyboardTabGroupCommands KeyboardTabGroupCommands
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       MAYBE_KeyboardTabGroupCommands) {
  ui::Accelerator create_new_tab_group_accelerator,
      add_new_tab_to_group_accelerator, close_tab_group_accelerator;

  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CREATE_NEW_TAB_GROUP, &create_new_tab_group_accelerator));
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_ADD_NEW_TAB_TO_GROUP, &add_new_tab_to_group_accelerator));
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CLOSE_TAB_GROUP, &close_tab_group_accelerator));

  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Keyboard Command to Create New Tab Group.
      SendAccelerator(kBrowserViewElementId, create_new_tab_group_accelerator),
      // Verify One Tab Group Exists.
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->group_model()
                ->ListTabGroups()
                .size();
          },
          1),
      // Hide Tab Group Bubble to avoid failing future inputs.
      SendAccelerator(kTabGroupEditorBubbleId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForHide(kTabGroupEditorBubbleId),
      // Keyboard Command to Add New Tab in Group.
      SendAccelerator(kBrowserViewElementId, add_new_tab_to_group_accelerator),
      // Verify One Tab Group Exists and its Tab Count is 2.
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->group_model()
                ->ListTabGroups()
                .size();
          },
          1),
      CheckResult(
          [this]() {
            auto* group_model = browser()->tab_strip_model()->group_model();
            return group_model
                ->GetTabGroup(group_model->ListTabGroups().front())
                ->ListTabs()
                .length();
          },
          2),
      // Keyboard Command to Delete Current Tab Group.
      SendAccelerator(kBrowserViewElementId, close_tab_group_accelerator),
      // Verify No Tab Groups Exist.
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->group_model()
                ->ListTabGroups()
                .size();
          },
          0));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       VerifyTabContextMenu) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Identify Tab by Type (VerticalTabView).
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kFirstTabName, 0),
      // Open Tab Context Menu.
      MoveMouseTo(kFirstTabName),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(TabMenuModel::kAddNewTabAdjacentMenuItem),
          SelectMenuItem(TabMenuModel::kAddNewTabAdjacentMenuItem)),
      // Verify functionality of command in the Tab Context Menu.
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       VerifyTabContextMenuText) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Identify Tab by Type (VerticalTabView).
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kFirstTabName, 0),
      // Open Tab Context Menu.
      MoveMouseTo(kFirstTabName),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(TabMenuModel::kAddNewTabAdjacentMenuItem), Do([this]() {
            // Verify "New Tab Below" text is present.
            EXPECT_TRUE(CheckMenuHasStringId(IDS_TAB_CXMENU_NEWTABBELOW));
            // Verify "Close Tabs Below" text is present.
            EXPECT_TRUE(CheckMenuHasStringId(IDS_TAB_CXMENU_CLOSETABSBELOW));
          }),
          // Close menu to avoid the test hanging.
          Do([this]() {
            vertical_tab_strip_controller()
                ->GetTabContextMenuController()
                ->CloseMenu();
          })));
}

}  // namespace
