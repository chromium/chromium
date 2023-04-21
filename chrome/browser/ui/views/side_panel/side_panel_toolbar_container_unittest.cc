// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_container.h"

#include <vector>

#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
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
#include "chrome/common/pref_names.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

class SidePanelToolbarContainerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {companion::features::kSidePanelCompanion,
         features::kSidePanelCompanionDefaultPinned},
        {});
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

  void WaitForAnimation() {
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/1045212): we avoid using animations on Mac due to the lack
    // of support in unit tests. Therefore this is a no-op.
#else
    views::test::WaitForAnimatingLayoutManager(
        browser_view()->toolbar()->side_panel_container());
#endif
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
  ASSERT_TRUE(pinned_buttons[0]->GetVisible());
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
  ASSERT_TRUE(views::InkDrop::Get(search_companion_button)->GetHighlighted());
}

TEST_F(SidePanelToolbarContainerTest,
       HighlightSidePanelButtonIfPinButtonNotVisible) {
  auto* search_companion_button = GetPinnedEntryButtons()[0];
  auto* side_panel_button = browser_view()->toolbar()->GetSidePanelButton();
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, false);
  search_companion_button->SetVisible(false);
  browser_view()->side_panel_coordinator()->Show(
      SidePanelEntry::Id::kSearchCompanion);
  ASSERT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  ASSERT_EQ(browser_view()->side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
  ASSERT_TRUE(views::InkDrop::Get(side_panel_button)->GetHighlighted());
}

TEST_F(SidePanelToolbarContainerTest, PinButtonOnlyVisibleForCompanion) {
  auto* side_panel_button = browser_view()->toolbar()->GetSidePanelButton();
  ClickButton(side_panel_button);
  views::ImageButton* header_pin_button =
      browser_view()->side_panel_coordinator()->GetHeaderPinButtonForTesting();
  // Verify that the pin button is not visible for entries other than the
  // companion.
  ASSERT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  ASSERT_NE(browser_view()->side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
  ASSERT_FALSE(header_pin_button->GetVisible());

  // Switch to the companion side panel and verify the header pin button is
  // visible.
  auto* search_companion_button = GetPinnedEntryButtons()[0];
  ClickButton(search_companion_button);
  ASSERT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  ASSERT_EQ(browser_view()->side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
  ASSERT_TRUE(header_pin_button->GetVisible());
}

TEST_F(SidePanelToolbarContainerTest, PinStateUpdatesOnPrefChange) {
  auto pinned_buttons = GetPinnedEntryButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  ASSERT_TRUE(pinned_buttons[0]->GetVisible());
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, false);
  WaitForAnimation();
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/1045212): GetVisible() relies on an
  // animation running, which is not reliable in unit tests on Mac.
#else
  ASSERT_FALSE(pinned_buttons[0]->GetVisible());
#endif
}

TEST_F(SidePanelToolbarContainerTest, CompanionButtonRemovedOnDSPChange) {
  // Verify the pinned companion button exists.
  auto pinned_buttons = GetPinnedEntryButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);

  // Update the default search provider.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetShortName(u"foo.com");
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  data.new_tab_url = "https://foo.com/newtab";
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // Verify the button no longer exists.
  pinned_buttons = GetPinnedEntryButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
}
