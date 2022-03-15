// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

class SidePanelCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        {features::kSidePanel, features::kUnifiedSidePanel}, {});
    TestWithBrowserView::SetUp();

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    AddTab(browser_view()->browser(), GURL("http://foo2.com"));

    // Add a kSideSearch entry to the contextual registry for the first tab.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
    content::WebContents* active_contents =
        browser_view()->GetActiveWebContents();
    auto* registry = SidePanelRegistry::Get(active_contents);
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kSideSearch, u"testing1",
        ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));
    contextual_registries_.push_back(registry);

    // Add a kLens entry to the contextual registry for the second tab.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
    active_contents = browser_view()->GetActiveWebContents();
    registry = SidePanelRegistry::Get(active_contents);
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLens, u"testing1",
        ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));
    contextual_registries_.push_back(SidePanelRegistry::Get(active_contents));

    coordinator_ = browser_view()->side_panel_coordinator();
    global_registry_ = coordinator_->global_registry_;

    // Verify the first tab has one entry, kSideSearch.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
    active_contents = browser_view()->GetActiveWebContents();
    SidePanelRegistry* contextual_registry =
        SidePanelRegistry::Get(active_contents);
    EXPECT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->id(),
              SidePanelEntry::Id::kSideSearch);

    // Verify the second tab has one entry, kLens.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
    active_contents = browser_view()->GetActiveWebContents();
    contextual_registry = SidePanelRegistry::Get(active_contents);
    EXPECT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->id(),
              SidePanelEntry::Id::kLens);
  }

  void VerifyEntryExistanceAndValue(absl::optional<SidePanelEntry*> entry,
                                    SidePanelEntry::Id id) {
    EXPECT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value()->id(), id);
  }

  void VerifyEntryExistanceAndValue(absl::optional<SidePanelEntry::Id> entry,
                                    SidePanelEntry::Id id) {
    EXPECT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value(), id);
  }

  absl::optional<SidePanelEntry::Id> GetLastActiveEntryId() {
    return coordinator_->GetLastActiveEntryId();
  }

  absl::optional<SidePanelEntry::Id> GetLastActiveGlobalEntry() {
    return coordinator_->last_active_global_entry_id_;
  }

 protected:
  raw_ptr<SidePanelCoordinator> coordinator_;
  raw_ptr<SidePanelRegistry> global_registry_;
  std::vector<raw_ptr<SidePanelRegistry>> contextual_registries_;
};

TEST_F(SidePanelCoordinatorTest, ToggleSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());

  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->right_aligned_side_panel()->GetVisible());
}

TEST_F(SidePanelCoordinatorTest, SidePanelReopensToLastSeenGlobalEntry) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);

  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);

  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);

  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
}

TEST_F(SidePanelCoordinatorTest, ShowOpensSidePanel) {
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
}

TEST_F(SidePanelCoordinatorTest, SwapBetweenTabsWithReadingListOpen) {
  // Verify side panel opens to kReadingList by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);

  // Verify switching tabs does not change side panel visibility or entry seen
  // if it is in the global registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
}

TEST_F(SidePanelCoordinatorTest, SwapBetweenTabsWithBookmarksOpen) {
  // Open side panel and switch to kBookmarks and verify the active entry is
  // updated.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);

  // Verify switching tabs does not change entry seen if it is in the global
  // registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
}

TEST_F(SidePanelCoordinatorTest, ContextualEntryDeregistered) {
  // Verify the first tab has one entry, kSideSearch.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(contextual_registries_[0]->entries().size(), 1u);
  EXPECT_EQ(contextual_registries_[0]->entries()[0]->id(),
            SidePanelEntry::Id::kSideSearch);

  // Deregister kSideSearch from the first tab.
  contextual_registries_[0]->Deregister(SidePanelEntry::Id::kSideSearch);
  EXPECT_EQ(contextual_registries_[0]->entries().size(), 0u);
}

TEST_F(SidePanelCoordinatorTest, ContextualEntryDeregisteredWhileVisible) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Deregister kSideSearch from the first tab.
  contextual_registries_[0]->Deregister(SidePanelEntry::Id::kSideSearch);
  EXPECT_EQ(contextual_registries_[0]->entries().size(), 0u);

  // Verify the panel defaults back to the last visible global entry or the
  // reading list.
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest, ShowContextualEntry) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
}

TEST_F(SidePanelCoordinatorTest,
       SwapBetweenTabsAfterNavaigatingToContextualEntry) {
  // Open side panel and verify it opens to kReadingList by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different global entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a tab where this contextual entry is not available and verify we
  // fall back to the last seen global entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the tab where the contextual entry was visible and verify it
  // is shown.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest, TogglePanelWithContextualEntryShowing) {
  // Open side panel and verify it opens to kReadingList by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different global entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Close the side panel and verify the contextual registry's last active entry
  // is reset.
  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(GetLastActiveGlobalEntry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Reopen the side panel and verify it reopens to the last active global
  // entry.
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest,
       SwitchBetweenTabWithContextualEntryAndTabWithNoEntry) {
  // Open side panel to contextual entry and verify.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_FALSE(GetLastActiveEntryId().has_value());
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(
    SidePanelCoordinatorTest,
    SwitchBetweenTabWithContextualEntryAndTabWithNoEntryWhenThereIsALastActiveGlobalEntry) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(GetLastActiveGlobalEntry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Open side panel to contextual entry and verify.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest,
       SwitchBackToTabWithPreviouslyVisibleContextualEntry) {
  // Open side panel to contextual entry and verify.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistanceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a global entry and verify the contextual entry is no longer
  // active.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different tab and verify state.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the original tab and verify the contextual entry is not
  // active or showing.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryId().has_value());
  EXPECT_EQ(GetLastActiveEntryId().value(), SidePanelEntry::Id::kReadingList);
  VerifyEntryExistanceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}
