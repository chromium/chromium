// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_container.h"

#include <vector>

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/events/base_event_utils.h"

class SidePanelToolbarContainerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSidePanelCompanion);
    TestWithBrowserView::SetUp();
    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
    auto* contextual_registry =
        SidePanelRegistry::Get(browser_view()->GetActiveWebContents());
    // Deregister and reregister the companion side panel entry so we don't try
    // to create the untrusted UI in tests.
    contextual_registry->Deregister(
        SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion));
    auto* search_companion_coordinator =
        SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(
            browser_view()->browser());
    contextual_registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kSearchCompanion,
        search_companion_coordinator->name(),
        ui::ImageModel::FromVectorIcon(search_companion_coordinator->icon()),
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));

    browser_view()->side_panel_coordinator()->SetNoDelaysForTesting(true);
  }

  void ClickButton(views::Button* button) const {
    ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               0);
    button->OnMousePressed(press_event);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                 gfx::Point(), ui::EventTimeForNow(),
                                 ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMouseReleased(release_event);
  }

  std::vector<ToolbarButton*> GetPinnedEntryButtons() {
    std::vector<ToolbarButton*> result;
    for (views::View* child :
         browser_view()->toolbar()->side_panel_container()->children()) {
      if (child != browser_view()->toolbar()->GetSidePanelButton()) {
        ToolbarButton* button = static_cast<ToolbarButton*>(child);
        result.push_back(button);
      }
    }
    return result;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SidePanelToolbarContainerTest, CompanionPinnedByDefault) {
  auto pinned_buttons = GetPinnedEntryButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  auto* search_companion_coordinator =
      SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(
          browser_view()->browser());
  ASSERT_EQ(pinned_buttons[0]->GetTooltipText(gfx::Point()),
            search_companion_coordinator->name());
}

TEST_F(SidePanelToolbarContainerTest, ClickingPinnedEntryOpensSidePanel) {
  auto* search_companion_button = GetPinnedEntryButtons()[0];
  ClickButton(search_companion_button);
  ASSERT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  ASSERT_EQ(browser_view()->side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
}
