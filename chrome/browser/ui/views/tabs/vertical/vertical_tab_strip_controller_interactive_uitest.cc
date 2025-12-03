// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace {

class VerticalTabStripControllerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  VerticalTabStripControllerInteractiveUiTest() = default;
  ~VerticalTabStripControllerInteractiveUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{tabs::kVerticalTabs};
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerInteractiveUiTest,
                       VerifyTabContextMenu) {
  const char kNewTabName[] = "NewTab";

  RunTestSequence(
      // Display Vertical Tabs.
      Do([this]() {
        browser()
            ->browser_window_features()
            ->vertical_tab_strip_state_controller()
            ->SetVerticalTabsEnabled(true);
        RunScheduledLayouts();
      }),
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
      CheckResult(
          [this]() { return browser()->tab_strip_model()->GetTabCount(); }, 2));
}

}  // namespace
