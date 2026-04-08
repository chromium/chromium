// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"

class TabsFromOtherDevicesSidePanelBrowserTest : public InProcessBrowserTest {
 public:
  TabsFromOtherDevicesSidePanelBrowserTest() {
    features_.InitAndEnableFeature(features::kTabsFromOtherDevicesSidePanel);
  }
  ~TabsFromOtherDevicesSidePanelBrowserTest() override = default;

  SidePanelCoordinator* coordinator() {
    return SidePanelCoordinator::From(browser());
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(TabsFromOtherDevicesSidePanelBrowserTest,
                       ShowSidePanel) {
  coordinator()->SetNoDelaysForTesting(true);
  coordinator()->Show(SidePanelEntryId::kTabsFromOtherDevices);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->GetBrowserView()
        .contents_height_side_panel()
        ->GetVisible();
  }));

  EXPECT_EQ(coordinator()->GetCurrentEntryId(SidePanelType::kContent),
            SidePanelEntryId::kTabsFromOtherDevices);

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionSidePanelShowTabsFromOtherDevices,
      browser()->browser_actions()->root_action_item());
  EXPECT_NE(action_item, nullptr);
  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_EQ(action_item->GetProperty(actions::kActionItemPinnableKey),
            static_cast<int>(actions::ActionPinnableState::kPinnable));
}

IN_PROC_BROWSER_TEST_F(TabsFromOtherDevicesSidePanelBrowserTest, ShowFromMenu) {
  coordinator()->SetNoDelaysForTesting(true);
  chrome::ExecuteCommand(browser(),
                         IDC_SHOW_TABS_FROM_OTHER_DEVICES_SIDE_PANEL);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->GetBrowserView()
        .contents_height_side_panel()
        ->GetVisible();
  }));

  EXPECT_EQ(coordinator()->GetCurrentEntryId(SidePanelType::kContent),
            SidePanelEntryId::kTabsFromOtherDevices);
}
