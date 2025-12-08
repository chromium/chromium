// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace {

const char kNewTabName[] = "NewTab";

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
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       VerifyTabContextMenu) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      // Identify Tab by Type (VerticalTabView).
      NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                kNewTabName, 0),
      // Open Tab Context Menu.
      MoveMouseTo(kNewTabName),
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
                                                kNewTabName, 0),
      // Open Tab Context Menu.
      MoveMouseTo(kNewTabName),
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
