// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <memory>
#include <string>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/test/views_test_utils.h"

using testing::_;

namespace {

// Creates a basic SidePanelEntry for the given `key` that returns an empty view
// when shown.
std::unique_ptr<SidePanelEntry> CreateEntry(const SidePanelEntry::Key& key) {
  return std::make_unique<SidePanelEntry>(
      key, u"basic entry",
      ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
      base::BindRepeating([]() { return std::make_unique<views::View>(); }));
}

}  // namespace

class SidePanelCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
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

    // Add a kSideSearch entry to the contextual registry for the second tab.
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kSideSearch, u"testing1",
        ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));

    coordinator_ = SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
    coordinator_->SetNoDelaysForTesting(true);
    global_registry_ = coordinator_->global_registry_;

    // Verify the first tab has one entry, kSideSearch.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
    active_contents = browser_view()->GetActiveWebContents();
    SidePanelRegistry* contextual_registry =
        SidePanelRegistry::Get(active_contents);
    EXPECT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->key().id(),
              SidePanelEntry::Id::kSideSearch);

    // Verify the second tab has 2 entries, kLens and kSideSearch.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
    active_contents = browser_view()->GetActiveWebContents();
    contextual_registry = SidePanelRegistry::Get(active_contents);
    EXPECT_EQ(contextual_registry->entries().size(), 2u);
    EXPECT_EQ(contextual_registry->entries()[0]->key().id(),
              SidePanelEntry::Id::kLens);
    EXPECT_EQ(contextual_registry->entries()[1]->key().id(),
              SidePanelEntry::Id::kSideSearch);
  }

  void VerifyEntryExistenceAndValue(absl::optional<SidePanelEntry*> entry,
                                    SidePanelEntry::Id id) {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value()->key().id(), id);
  }

  void VerifyEntryExistenceAndValue(absl::optional<SidePanelEntry*> entry,
                                    const SidePanelEntry::Key& key) {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value()->key(), key);
  }

  void VerifyEntryExistenceAndValue(absl::optional<SidePanelEntry::Id> entry,
                                    SidePanelEntry::Id id) {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry.value(), id);
  }

  absl::optional<SidePanelEntry::Key> GetLastActiveEntryKey() {
    return coordinator_->GetLastActiveEntryKey();
  }

  absl::optional<SidePanelEntry::Key> GetSelectedKey() {
    return coordinator_->GetSelectedKey();
  }

  bool ComboboxViewExists() {
    return coordinator_->header_combobox_ != nullptr;
  }

 protected:
  raw_ptr<SidePanelCoordinator, DanglingUntriaged> coordinator_;
  raw_ptr<SidePanelRegistry, DanglingUntriaged> global_registry_;
  std::vector<raw_ptr<SidePanelRegistry, DanglingUntriaged>>
      contextual_registries_;
};

class MockSidePanelViewStateObserver : public SidePanelViewStateObserver {
 public:
  MOCK_METHOD(void, OnSidePanelDidClose, (), (override));
  MOCK_METHOD(void, OnSidePanelDidOpen, (), (override));
};

TEST_F(SidePanelCoordinatorTest, ToggleSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
}

TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidth) {
  // Set side panel to right-aligned
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  coordinator_->Toggle();
  const int starting_width = 500;
  browser_view()->unified_side_panel()->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(), starting_width);

  const int increment = 50;
  browser_view()->unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(),
            starting_width - increment);

  // Set side panel to left-aligned
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  browser_view()->unified_side_panel()->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(), starting_width);

  browser_view()->unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(),
            starting_width + increment);
}

TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidthMaxMin) {
  coordinator_->Toggle();
  const int starting_width = 500;
  browser_view()->unified_side_panel()->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(), starting_width);

  // Use an increment large enough to hit side panel and browser contents
  // minimum width constraints.
  const int large_increment = 1000000000;
  browser_view()->unified_side_panel()->OnResize(large_increment, true);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(),
            browser_view()->unified_side_panel()->GetMinimumSize().width());

  browser_view()->unified_side_panel()->OnResize(-large_increment, true);
  views::test::RunScheduledLayout(browser_view());
  BrowserViewLayout* layout_manager =
      static_cast<BrowserViewLayout*>(browser_view()->GetLayoutManager());
  const int min_web_contents_width =
      layout_manager->GetMinWebContentsWidthForTesting();
  EXPECT_EQ(browser_view()->contents_web_view()->width(),
            min_web_contents_width);
}

TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidthRTL) {
  // Set side panel to right-aligned
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  // Set UI direction to LTR
  base::i18n::SetRTLForTesting(false);
  coordinator_->Toggle();
  const int starting_width = 500;
  browser_view()->unified_side_panel()->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(), starting_width);

  const int increment = 50;
  browser_view()->unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(),
            starting_width - increment);

  // Set UI direction to RTL
  base::i18n::SetRTLForTesting(true);
  browser_view()->unified_side_panel()->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(), starting_width);

  browser_view()->unified_side_panel()->OnResize(increment, true);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(),
            starting_width + increment);
}

TEST_F(SidePanelCoordinatorTest, ChangeSidePanelWidthWindowResize) {
  coordinator_->Toggle();
  const int starting_width = 500;
  browser_view()->unified_side_panel()->SetPanelWidth(starting_width);
  views::test::RunScheduledLayout(browser_view());
  EXPECT_EQ(browser_view()->unified_side_panel()->width(), starting_width);

  // Shrink browser window enough that side panel should also shrink in
  // observance of web contents minimum width.
  gfx::Rect original_bounds(browser_view()->GetBounds());
  gfx::Size new_size(starting_width, starting_width);
  gfx::Rect new_bounds(original_bounds);
  new_bounds.set_size(new_size);
  // Explicitly restore the browser window on ChromeOS, as it would otherwise
  // be maximized and the SetBounds call would be a no-op.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  browser_view()->Restore();
#endif
  browser_view()->SetBounds(new_bounds);
  EXPECT_LT(browser_view()->unified_side_panel()->width(), starting_width);
  BrowserViewLayout* layout_manager =
      static_cast<BrowserViewLayout*>(browser_view()->GetLayoutManager());
  const int min_web_contents_width =
      layout_manager->GetMinWebContentsWidthForTesting();
  EXPECT_EQ(browser_view()->contents_web_view()->width(),
            min_web_contents_width);

  // Return browser window to original size, side panel should also return to
  // size prior to window resize.
  browser_view()->SetBounds(original_bounds);
  EXPECT_EQ(browser_view()->unified_side_panel()->width(), starting_width);
}

TEST_F(SidePanelCoordinatorTest, ChangeSidePanelAlignment) {
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(browser_view()->unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser_view()->unified_side_panel()->GetHorizontalAlignment(),
            SidePanel::kAlignRight);

  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(browser_view()->unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser_view()->unified_side_panel()->GetHorizontalAlignment(),
            SidePanel::kAlignLeft);
}

// Verify that right and left alignment works the same as when in LTR mode.
TEST_F(SidePanelCoordinatorTest, ChangeSidePanelAlignmentRTL) {
  // Forcing the language to hebrew causes the ui to enter RTL mode.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");

  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(browser_view()->unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser_view()->unified_side_panel()->GetHorizontalAlignment(),
            SidePanel::kAlignRight);

  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(browser_view()->unified_side_panel()->IsRightAligned());
  EXPECT_EQ(browser_view()->unified_side_panel()->GetHorizontalAlignment(),
            SidePanel::kAlignLeft);
}

TEST_F(SidePanelCoordinatorTest,
       ClosingSidePanelCallsOnSidePanelClosedObserver) {
  MockSidePanelViewStateObserver view_state_observer;
  EXPECT_CALL(view_state_observer, OnSidePanelDidClose()).Times(1);
  coordinator_->AddSidePanelViewStateObserver(&view_state_observer);
  coordinator_->Show();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->Close();

  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
}

TEST_F(SidePanelCoordinatorTest, OpeningSidePanelCallsOnSidePanelObserver) {
  MockSidePanelViewStateObserver view_state_observer;
  EXPECT_CALL(view_state_observer, OnSidePanelDidOpen()).Times(1);

  coordinator_->AddSidePanelViewStateObserver(&view_state_observer);

  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);

  // Changing the side panel entry after it is opened, should not notify
  // observers.
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);

  coordinator_->RemoveSidePanelViewStateObserver(&view_state_observer);
}

TEST_F(SidePanelCoordinatorTest, RemovingObserverDoesNotIncrementCount) {
  MockSidePanelViewStateObserver view_state_observer;
  EXPECT_CALL(view_state_observer, OnSidePanelDidClose()).Times(1);
  coordinator_->AddSidePanelViewStateObserver(&view_state_observer);
  coordinator_->Show();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->Close();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->Show();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->RemoveSidePanelViewStateObserver(&view_state_observer);

  coordinator_->Close();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
}

TEST_F(SidePanelCoordinatorTest, SidePanelReopensToLastSeenGlobalEntry) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);

  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);

  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);

  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
}

TEST_F(SidePanelCoordinatorTest, ShowOpensSidePanel) {
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);

  // Verify that the combobox entry for bookmarks is selected.
  EXPECT_EQ(GetSelectedKey().value().id(), SidePanelEntry::Id::kBookmarks);
}

TEST_F(SidePanelCoordinatorTest, CloseInvalidatesComboboxPointer) {
  // Verify no combobox exists before opening the side panel.
  EXPECT_FALSE(ComboboxViewExists());

  coordinator_->Toggle();
  EXPECT_TRUE(ComboboxViewExists());

  // Verify that the pointer to the combobox view is invalidated after closing
  // the side panel.
  coordinator_->Toggle();
  EXPECT_FALSE(ComboboxViewExists());
}

TEST_F(SidePanelCoordinatorTest, TabSwitchInvalidatesComboboxPointerOnClose) {
  // Verify no combobox exists before opening the side panel.
  EXPECT_FALSE(ComboboxViewExists());

  // Show a contextual entry on the first tab.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);

  // Switch to the second tab.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);

  // Expect that the side panel closes.
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());

  // Verify that the pointer to the combobox view is invalidated after closing
  // the side panel.
  EXPECT_FALSE(ComboboxViewExists());
}

TEST_F(SidePanelCoordinatorTest, SwapBetweenTabsWithBookmarksOpen) {
  // Verify side panel opens to kBookmarks by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);

  // Verify switching tabs does not change side panel visibility or entry seen
  // if it is in the global registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
}

TEST_F(SidePanelCoordinatorTest, SwapBetweenTabsWithReadingListOpen) {
  // Open side panel and switch to kReadingList and verify the active entry is
  // updated.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);

  // Verify switching tabs does not change entry seen if it is in the global
  // registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
}

TEST_F(SidePanelCoordinatorTest, ContextualEntryDeregistered) {
  // Verify the first tab has one entry, kSideSearch.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(contextual_registries_[0]->entries().size(), 1u);
  EXPECT_EQ(contextual_registries_[0]->entries()[0]->key().id(),
            SidePanelEntry::Id::kSideSearch);

  // Deregister kSideSearch from the first tab.
  contextual_registries_[0]->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch));
  EXPECT_EQ(contextual_registries_[0]->entries().size(), 0u);
}

TEST_F(SidePanelCoordinatorTest, ContextualEntryDeregisteredWhileVisible) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Deregister kSideSearch from the first tab.
  contextual_registries_[0]->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch));
  EXPECT_EQ(contextual_registries_[0]->entries().size(), 0u);

  // Verify the panel defaults back to the last visible global entry or the
  // reading list.
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

// Test that the side panel closes if a contextual entry is deregistered while
// visible when no global entries have been shown since the panel was opened.
TEST_F(
    SidePanelCoordinatorTest,
    ContextualEntryDeregisteredWhileVisibleClosesPanelIfNoLastSeenGlobalEntryExists) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Deregister kSideSearch from the first tab.
  contextual_registries_[0]->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch));
  EXPECT_EQ(contextual_registries_[0]->entries().size(), 0u);

  // Verify the panel closes.
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_FALSE(GetLastActiveEntryKey().has_value());
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest, ShowContextualEntry) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
}

TEST_F(SidePanelCoordinatorTest, SwapBetweenTwoContextualEntryWithTheSameId) {
  // Open side search for the first tab.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  auto* reading_list_entry = coordinator_->GetCurrentSidePanelEntryForTesting();
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  auto* side_search_entry1 = coordinator_->GetCurrentSidePanelEntryForTesting();

  // Switch to the second tab and open side search.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_EQ(reading_list_entry,
            coordinator_->GetCurrentSidePanelEntryForTesting());
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_NE(side_search_entry1,
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // Switch back to the first tab.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_EQ(side_search_entry1,
            coordinator_->GetCurrentSidePanelEntryForTesting());
}

TEST_F(SidePanelCoordinatorTest,
       SwapBetweenTabsAfterNavigatingToContextualEntry) {
  // Open side panel and verify it opens to kBookmarks by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different global entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  auto* bookmarks_entry = coordinator_->GetCurrentSidePanelEntryForTesting();

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  auto* side_search_entry = coordinator_->GetCurrentSidePanelEntryForTesting();

  // Switch to a tab where this contextual entry is not available and verify we
  // fall back to the last seen global entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(bookmarks_entry,
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // Switch back to the tab where the contextual entry was visible and verify it
  // is shown.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_EQ(side_search_entry,
            coordinator_->GetCurrentSidePanelEntryForTesting());
}

TEST_F(SidePanelCoordinatorTest, TogglePanelWithContextualEntryShowing) {
  // Open side panel and verify it opens to kBookmarks by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different global entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Close the side panel and verify the contextual registry's last active entry
  // remains set.
  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Reopen the side panel and verify it reopens to the last active contextual
  // entry.
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest,
       TogglePanelWithGlobalEntryShowingWithTabSwitch) {
  // Open side panel and verify it opens to kBookmarks by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a global entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Close the side panel and verify the global registry's last active entry
  // is set and the contextual registry's last active entry is reset.
  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->last_active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and open a contextual entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->last_active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);

  // Switch back to the first tab, reopen the side panel, and verify it reopens
  // to the last active global entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
}

TEST_F(SidePanelCoordinatorTest,
       TogglePanelWithContextualEntryShowingWithTabSwitch) {
  // Open side panel and verify it opens to kBookmarks by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a contextual entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Close the side panel and verify the contextual registry's last active entry
  // is set.
  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab, open the side panel, and verify the contextual
  // registry's last active entry is still set.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Close the side panel and verify the contextual registry's last active entry
  // is still set.
  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->last_active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the first tab, reopen the side panel, and verify it reopens
  // to the last active contextual entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest,
       SwitchBetweenTabWithGlobalEntryAndTabWithLastActiveContextualEntry) {
  // Open side panel and verify it opens to kBookmarks by default.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another global entry and verify the active entry is updated.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and open a contextual entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);

  // Close the side panel and verify the contextual registry's last active entry
  // is set.
  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->last_active_entry(),
                               SidePanelEntry::Id::kSideSearch);

  // Switch back to the first tab and open the side panel.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->last_active_entry(),
                               SidePanelEntry::Id::kSideSearch);

  // Switch back to the second tab and verify that the last active global entry
  // is set.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[1]->last_active_entry(),
                               SidePanelEntry::Id::kSideSearch);

  // Close the side panel and verify that the last active contextual entry is
  // reset.
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->last_active_entry().has_value());

  // Reopen the side panel and verify that the global entry is open.
  coordinator_->Toggle();
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest,
       SwitchBetweenTabWithContextualEntryAndTabWithNoEntry) {
  // Open side panel to contextual entry and verify.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_FALSE(GetLastActiveEntryKey().has_value());
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(
    SidePanelCoordinatorTest,
    SwitchBetweenTabWithContextualEntryAndTabWithNoEntryWhenThereIsALastActiveGlobalEntry) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  VerifyEntryExistenceAndValue(global_registry_->last_active_entry(),
                               SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Open side panel to contextual entry and verify.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the tab with the contextual entry open and verify the side
  // panel is then open.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest,
       SwitchBackToTabWithPreviouslyVisibleContextualEntry) {
  // Open side panel to contextual entry and verify.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a global entry and verify the contextual entry is no longer
  // active.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to a different tab and verify state.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch back to the original tab and verify the contextual entry is not
  // active or showing.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(contextual_registries_[0]->active_entry().has_value());
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

TEST_F(SidePanelCoordinatorTest,
       SwitchBackToTabWithContextualEntryAfterClosingGlobal) {
  // Open side panel to contextual entry and verify.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Switch to another tab and verify the side panel is closed.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_FALSE(GetLastActiveEntryKey().has_value());
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Open a global entry and verify.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(global_registry_->active_entry(),
                               SidePanelEntry::Id::kReadingList);
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Verify the panel closes but the first tab still has an active entry.
  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());

  // Verify returning to the first tab reopens the side panel to the active
  // contextual entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(global_registry_->active_entry().has_value());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(contextual_registries_[1]->active_entry().has_value());
}

class TestSidePanelObserver : public SidePanelEntryObserver {
 public:
  explicit TestSidePanelObserver(SidePanelRegistry* registry)
      : registry_(registry) {}
  ~TestSidePanelObserver() override = default;

  void OnEntryHidden(SidePanelEntry* entry) override {
    registry_->Deregister(entry->key());
  }

 private:
  raw_ptr<SidePanelRegistry> registry_;
};

TEST_F(SidePanelCoordinatorTest,
       EntryRegistersOnBeingHiddenFromSwitchToOtherEntry) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

  // Create an observer that deregisters the entry once it is hidden.
  auto observer =
      std::make_unique<TestSidePanelObserver>(contextual_registries_[0]);
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kAssistant, u"Assistant",
      ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
      base::BindRepeating([]() { return std::make_unique<views::View>(); }));
  entry->AddObserver(observer.get());
  contextual_registries_[0]->Register(std::move(entry));
  coordinator_->Show(SidePanelEntry::Id::kAssistant);

  // Switch to another entry.
  coordinator_->Show(SidePanelEntry::Id::kReadingList);

  // Verify that the previous entry has deregistered.
  EXPECT_FALSE(contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kAssistant)));
}

TEST_F(SidePanelCoordinatorTest,
       EntryRegistersOnBeingHiddenFromSidePanelClose) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

  // Create an observer that deregisters the entry once it is hidden.
  auto observer =
      std::make_unique<TestSidePanelObserver>(contextual_registries_[0]);
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kAssistant, u"Assistant",
      ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
      base::BindRepeating([]() { return std::make_unique<views::View>(); }));
  entry->AddObserver(observer.get());
  contextual_registries_[0]->Register(std::move(entry));
  coordinator_->Show(SidePanelEntry::Id::kAssistant);

  // Close the sidepanel.
  coordinator_->Toggle();

  // Verify that the previous entry has deregistered.
  EXPECT_FALSE(contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kAssistant)));
}

TEST_F(SidePanelCoordinatorTest, ShouldNotRecreateTheSameEntry) {
  // Switch to a tab without a contextual entry for lens, so that Show() shows
  // the global entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

  int count = 0;
  global_registry_->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kLens, u"lens",
      ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
      base::BindRepeating(
          [](int* count) {
            (*count)++;
            return std::make_unique<views::View>();
          },
          &count)));
  coordinator_->Show(SidePanelEntry::Id::kLens);
  ASSERT_EQ(1, count);
  coordinator_->Show(SidePanelEntry::Id::kLens);
  ASSERT_EQ(1, count);
}

// closes side panel if the active entry is de-registered when open
TEST_F(SidePanelCoordinatorTest, GlobalEntryDeregisteredWhenVisible) {
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  global_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));

  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_FALSE(GetLastActiveEntryKey().has_value());
}

// resets last active entry id if active global entry de-registers when closed
TEST_F(SidePanelCoordinatorTest, GlobalEntryDeregisteredWhenClosed) {
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->Close();
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  global_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));

  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_FALSE(GetLastActiveEntryKey().has_value());
}

TEST_F(SidePanelCoordinatorTest, ComboboxAdditionsDoNotChangeSelection) {
  SidePanelEntry::Id earlier_sorted_entry =
      std::min(SidePanelEntry::Id::kSideSearch, SidePanelEntry::Id::kLens);
  SidePanelEntry::Id later_sorted_entry =
      std::max(SidePanelEntry::Id::kSideSearch, SidePanelEntry::Id::kLens);
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  content::WebContents* active_contents =
      browser_view()->GetActiveWebContents();
  auto* contextual_registry = SidePanelRegistry::Get(active_contents);
  contextual_registry->Deregister(SidePanelEntry::Key(earlier_sorted_entry));
  coordinator_->Show(later_sorted_entry);
  // Verify the selected index in the combobox is the later entry.
  absl::optional<size_t> selected_index =
      coordinator_->GetComboboxForTesting()->GetSelectedIndex();
  EXPECT_TRUE(selected_index.has_value());
  EXPECT_EQ(coordinator_->GetComboboxModelForTesting()
                ->GetKeyAt(selected_index.value())
                .id(),
            later_sorted_entry);
  // Add back the earlier entry and verify the selected index is still correct.
  contextual_registry->Register(std::make_unique<SidePanelEntry>(
      earlier_sorted_entry, u"testing1",
      ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));
  EXPECT_EQ(coordinator_->GetCurrentSidePanelEntryForTesting()->key().id(),
            later_sorted_entry);
  selected_index = coordinator_->GetComboboxForTesting()->GetSelectedIndex();
  EXPECT_TRUE(selected_index.has_value());
  EXPECT_EQ(coordinator_->GetComboboxModelForTesting()
                ->GetKeyAt(selected_index.value())
                .id(),
            later_sorted_entry);
}

// Test that a crash does not occur when the browser is closed when the side
// panel view is shown but before the entry to be displayed has finished
// loading. Regression for crbug.com/1408947.
TEST_F(SidePanelCoordinatorTest, BrowserClosedBeforeEntryLoaded) {
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());

  // Allow content delays to more closely mimic real behavior.
  coordinator_->SetNoDelaysForTesting(false);
  coordinator_->Toggle();
  browser_view()->Close();
}

// Test that Show() shows the contextual extension entry if available for the
// current tab. Otherwise it shows the global extension entry. Note: only
// extensions will be able to have their entries exist in both the global and
// contextual registries.
TEST_F(SidePanelCoordinatorTest, ShowGlobalAndContextualExtensionEntries) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    "extension_id");
  global_registry_->Register(CreateEntry(extension_key));
  contextual_registries_[0]->Register(CreateEntry(extension_key));

  coordinator_->Show(extension_key);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // Switch to a tab that does not have an extension entry registered for its
  // contextual registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator_->Show(extension_key);
  EXPECT_EQ(global_registry_->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
}

// Test that the combobox shows the correct number of extension entries when
// global or contextual entries are registered, and that a new contextual
// extension entry gets shown if it's registered for the active tab and the
// global extension entry is showing.
TEST_F(SidePanelCoordinatorTest, RegisterExtensionEntries) {
  // Make sure the second tab is active.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  SidePanelEntry::Key extension_1_key(SidePanelEntry::Id::kExtension,
                                      "extension_1");
  SidePanelEntry::Key extension_2_key(SidePanelEntry::Id::kExtension,
                                      "extension_2");
  auto* combobox_model = coordinator_->GetComboboxModelForTesting();
  EXPECT_FALSE(combobox_model->HasKey(extension_1_key));

  // Currently on the second tab. Sanity check that registering an entry on the
  // first tab should not show an entry in the combobox.
  contextual_registries_[0]->Register(CreateEntry(extension_1_key));
  EXPECT_FALSE(combobox_model->HasKey(extension_1_key));

  contextual_registries_[1]->Register(CreateEntry(extension_1_key));
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_1_key));

  // Check that registering a global entry while the combobox contains an item
  // for the contextual entry still results in one item for an extension.
  global_registry_->Register(CreateEntry(extension_1_key));
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_1_key));

  EXPECT_FALSE(combobox_model->HasKey(extension_2_key));
  global_registry_->Register(CreateEntry(extension_2_key));
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_2_key));

  // Show the global entry for `extension_2`.
  coordinator_->Show(extension_2_key);
  EXPECT_EQ(global_registry_->GetEntryForKey(extension_2_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(global_registry_->GetEntryForKey(extension_2_key),
            global_registry_->active_entry());

  // Check that registering an entry on the active tab while the combobox
  // contains an item for the global entry still results in one item for an
  // extension.
  contextual_registries_[1]->Register(CreateEntry(extension_2_key));
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_2_key));

  // Since `extension_2`'s global entry was showing when the contextual entry
  // was registered for the active tab, the contextual entry should be shown
  // right after registration.
  EXPECT_EQ(contextual_registries_[1]->GetEntryForKey(extension_2_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(contextual_registries_[1]->GetEntryForKey(extension_2_key),
            contextual_registries_[1]->active_entry());
}

// Test that the combobox shows the correct number of extension entries when
// global or contextual entries are deregistered, and if it exists, the global
// extension entry is shown if the active tab's extension entry is deregistered.
TEST_F(SidePanelCoordinatorTest, DeregisterExtensionEntries) {
  // Make sure the second tab is active.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    "extension");
  auto* combobox_model = coordinator_->GetComboboxModelForTesting();

  // Registers an entry in the global and active contextual registry.
  auto register_entries = [this, &combobox_model, &extension_key]() {
    contextual_registries_[1]->Register(CreateEntry(extension_key));
    global_registry_->Register(CreateEntry(extension_key));
    EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_key));
  };

  register_entries();

  // The contextual entry should be shown.
  coordinator_->Show(extension_key);
  EXPECT_EQ(contextual_registries_[1]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // If the contextual entry is deregistered while there exists a global entry,
  // an entry should still be shown in the combobox.
  contextual_registries_[1]->Deregister(extension_key);
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_key));
  // Since there exists a global entry for the extension, it should be shown
  // after the contextual entry (that was shown) is deregistered.
  EXPECT_EQ(global_registry_->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // The side panel should be closed after the global entry is deregistered.
  global_registry_->Deregister(extension_key);
  EXPECT_FALSE(combobox_model->HasKey(extension_key));
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());

  register_entries();
  coordinator_->Show(extension_key);

  // If the global entry is deregistered while there exists an active contextual
  // entry, an entry should still be shown in the combobox.
  global_registry_->Deregister(extension_key);
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_key));

  // The contextual entry should still be shown after the global entry is
  // deregistered.
  EXPECT_EQ(contextual_registries_[1]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  contextual_registries_[1]->Deregister(extension_key);
  EXPECT_FALSE(combobox_model->HasKey(extension_key));
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
}

// Test that the combobox shows the correct number of extension entries in
// between tab switches.
TEST_F(SidePanelCoordinatorTest, ExtensionEntriesTabSwitchCombobox) {
  // Show the side search entry on the first tab so the contextual registry for
  // that tab has an active entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kSideSearch);

  // Switch to the second tab.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    "extension");
  auto* combobox_model = coordinator_->GetComboboxModelForTesting();

  // Register a contextual extension entry for the second tab and show it.
  contextual_registries_[1]->Register(CreateEntry(extension_key));
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_key));
  coordinator_->Show(extension_key);

  // Switch to the first tab, which does not have an extension entry for its
  // registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_FALSE(combobox_model->HasKey(extension_key));

  // Since there's no extension entry in the global registry nor the registry
  // for the first tab, fall back to showing the last active entry, which is the
  // side search entry for the first tab.
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(
                SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch)),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // Register an extension entry to the global registry and show it.
  global_registry_->Register(CreateEntry(extension_key));
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_key));
  coordinator_->Show(extension_key);

  // Switch back to the second tab. There should be only one extension entry in
  // the combobox, corresponding to the contextual registry's extension entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_key));

  // The extension entry for the second tab should be showing.
  EXPECT_EQ(contextual_registries_[1]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // Switch back to the first tab. There should be only one extension entry in
  // the combobox, corresponding to the global registry's extension entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(1, combobox_model->GetKeyCountForTesting(extension_key));
}

// Test that an extension with only contextual entries should behave like other
// contextual entry types when switching tabs.
TEST_F(SidePanelCoordinatorTest, ExtensionEntriesTabSwitchNoGlobalEntry) {
  // Switch to the first tab, then register and show an extension entry on its
  // contextual registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    "extension");
  contextual_registries_[0]->Register(CreateEntry(extension_key));
  coordinator_->Show(extension_key);

  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // Switch to the second tab. Since there is no active contextual/global entry
  // and no global entry with `extension_key`, the side panel should close.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

// Test that an extension with both contextual and global entries should behave
// like global entries when switching tabs and its entries take precedence over
// all other entries except active contextual entries (this case is covered in
// ExtensionEntriesTabSwitchWithActiveContextualEntry).
TEST_F(SidePanelCoordinatorTest, ExtensionEntriesTabSwitchGlobalEntry) {
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    "extension");
  contextual_registries_[0]->Register(CreateEntry(extension_key));
  global_registry_->Register(CreateEntry(extension_key));

  // Switching from a tab showing the extension's active entry to a
  // tab with no active contextual entry should show the extension's entry
  // (global in this case).
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(extension_key);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(global_registry_->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
  VerifyEntryExistenceAndValue(global_registry_->active_entry(), extension_key);

  // Reset the active entry on the first tab.
  contextual_registries_[0]->ResetActiveEntry();

  // Switching from a tab with the global extension entry to a tab with a
  // contextual extension entry should show the contextual entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
  VerifyEntryExistenceAndValue(contextual_registries_[0]->active_entry(),
                               extension_key);

  // Now register a reading list entry in the global registry and show it on the
  // second tab.
  SidePanelEntry::Key reading_list_key(SidePanelEntry::Id::kReadingList);
  global_registry_->Register(CreateEntry(reading_list_key));
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator_->Show(reading_list_key);

  // Show the extension's contextual entry on the first tab.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(extension_key);

  // Switch to the second tab. The extension's global entry should show and be
  // the active entry in the global registry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(global_registry_->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
  VerifyEntryExistenceAndValue(global_registry_->active_entry(), extension_key);

  // Show side search on the second tab.
  coordinator_->Show(SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch));
  // Reset the active entry on the first tab.
  contextual_registries_[0]->ResetActiveEntry();

  // Switch to the first tab. There's no active contextual entry but there
  // exists a contextual entry for that tab with the same key as the global
  // active entry, so it should be shown.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
}

// Test that when switching tabs while an extension's entry is showing, the new
// tab's active contextual entry should still take precedence over the
// extensions' entries.
TEST_F(SidePanelCoordinatorTest,
       ExtensionEntriesTabSwitchWithActiveContextualEntry) {
  SidePanelEntry::Key side_search_key(SidePanelEntry::Id::kSideSearch);
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    "extension");
  contextual_registries_[0]->Register(CreateEntry(extension_key));
  global_registry_->Register(CreateEntry(extension_key));

  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(side_search_key);

  // Show the extension's global entry on the second tab.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
  coordinator_->Show(extension_key);
  EXPECT_EQ(global_registry_->GetEntryForKey(extension_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());

  // Switch to the first tab. Since there is already an active contextual entry
  // for that tab, that entry should be showing instead of the extension's
  // entry.
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(contextual_registries_[0]->GetEntryForKey(side_search_key),
            coordinator_->GetCurrentSidePanelEntryForTesting());
}

// Tests that DeregisterAndReturnView returns the deregistered entry's view if
// it exists, whether or not the entry is showing.
TEST_F(SidePanelCoordinatorTest, DeregisterAndReturnView) {
  // A view with a counter as an internal state, used to check that the correct
  // view is returned by DeregisterAndReturnView.
  class ViewWithCounter : public views::View {
   public:
    explicit ViewWithCounter(int counter) : views::View(), counter_(counter) {}
    ~ViewWithCounter() override = default;

    int counter() { return counter_; }

   private:
    int counter_ = 0;
  };

  SidePanelEntry::Key side_search_key(SidePanelEntry::Id::kSideSearch);
  SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                    "extension");

  auto create_entry_with_counter = [](const SidePanelEntry::Key& key,
                                      int counter) {
    return std::make_unique<SidePanelEntry>(
        key, u"basic entry",
        ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
        base::BindRepeating(
            [](int counter) -> std::unique_ptr<views::View> {
              return std::make_unique<ViewWithCounter>(counter);
            },
            counter));
  };

  // Register the entry but don't show it.
  global_registry_->Register(create_entry_with_counter(extension_key, 11));

  // Since the entry was never shown, its view was never created and
  // `returned_view` should be null.
  std::unique_ptr<views::View> returned_view =
      SidePanelUtil::DeregisterAndReturnView(global_registry_, extension_key);
  EXPECT_FALSE(returned_view);

  // Register the entry and show it.
  global_registry_->Register(create_entry_with_counter(extension_key, 22));
  coordinator_->Show(extension_key);

  // Since the entry was shown, its view was created. Check that the correct
  // view is returned by checking its state that was set at creation time.
  returned_view =
      SidePanelUtil::DeregisterAndReturnView(global_registry_, extension_key);
  ASSERT_TRUE(returned_view);
  EXPECT_EQ(22, static_cast<ViewWithCounter*>(returned_view.get())->counter());

  // Register the entry, show it, then show another entry so the entry for
  // `extension_key` has its view cached.
  global_registry_->Register(create_entry_with_counter(extension_key, 33));
  coordinator_->Show(extension_key);
  coordinator_->Show(side_search_key);

  // Since the entry was shown, its view was created. Check that the correct
  // view is returned by checking its state that was set at creation time.
  returned_view =
      SidePanelUtil::DeregisterAndReturnView(global_registry_, extension_key);
  ASSERT_TRUE(returned_view);
  EXPECT_EQ(33, static_cast<ViewWithCounter*>(returned_view.get())->counter());
}

class SidePanelPinningCoordinatorTest : public SidePanelCoordinatorTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSidePanelPinning);
    SidePanelCoordinatorTest::SetUp();
    content::WebContents* const web_contents =
        browser_view()->browser()->tab_strip_model()->GetWebContentsAt(0);
    auto* const registry = SidePanelRegistry::Get(web_contents);
    registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kAboutThisSite, std::u16string(), ui::ImageModel(),
        base::BindRepeating([]() { return std::make_unique<views::View>(); })));
    contextual_registries_.push_back(registry);
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        PinnedToolbarActionsModelFactory::GetInstance(),
        base::BindRepeating(
            &SidePanelPinningCoordinatorTest::BuildPinnedToolbarActionsModel));
    return factories;
  }

  static std::unique_ptr<KeyedService> BuildPinnedToolbarActionsModel(
      content::BrowserContext* context) {
    return std::make_unique<PinnedToolbarActionsModel>(
        Profile::FromBrowserContext(context));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SidePanelPinningCoordinatorTest, SidePanelTitleUpdates) {
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kBookmarks);
  EXPECT_EQ(coordinator_->panel_title_->GetText(),
            l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE));

  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kReadingList);
  EXPECT_EQ(coordinator_->panel_title_->GetText(),
            l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));

  // Checks that the title updates even for contextual side panels
  coordinator_->Show(SidePanelEntry::Id::kAboutThisSite);
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            SidePanelEntry::Id::kAboutThisSite);
  EXPECT_EQ(coordinator_->panel_title_->GetText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE));
}

// Test that the SidePanelCoordinator behaves and updates corrected when dealing
// with entries that load/display asynchronously.
class SidePanelCoordinatorLoadingContentTest : public SidePanelCoordinatorTest {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    AddTab(browser_view()->browser(), GURL("http://foo2.com"));

    coordinator_ = SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
    global_registry_ = SidePanelCoordinator::GetGlobalSidePanelRegistry(
        browser_view()->browser());

    // Add a kSideSearch entry to the global registry with loading content not
    // available.
    std::unique_ptr<SidePanelEntry> entry1 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kSideSearch, u"testing1",
        ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
        base::BindRepeating([]() {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(false);
          return view;
        }));
    loading_content_entry1_ = entry1.get();
    global_registry_->Register(std::move(entry1));

    // Add a kLens entry to the global registry with loading content not
    // available.
    std::unique_ptr<SidePanelEntry> entry2 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLens, u"testing2",
        ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
        base::BindRepeating([]() {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(false);
          return view;
        }));
    loading_content_entry2_ = entry2.get();
    global_registry_->Register(std::move(entry2));

    // Add a kAssistant entry to the global registry with content available.
    std::unique_ptr<SidePanelEntry> entry3 = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kAssistant, u"testing3",
        ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
        base::BindRepeating([]() {
          auto view = std::make_unique<views::View>();
          SidePanelUtil::GetSidePanelContentProxy(view.get())
              ->SetAvailable(true);
          return view;
        }));
    loaded_content_entry1_ = entry3.get();
    global_registry_->Register(std::move(entry3));
  }

  raw_ptr<SidePanelEntry, DanglingUntriaged> loading_content_entry1_;
  raw_ptr<SidePanelEntry, DanglingUntriaged> loading_content_entry2_;
  raw_ptr<SidePanelEntry, DanglingUntriaged> loaded_content_entry1_;
};

TEST_F(SidePanelCoordinatorLoadingContentTest,
       ContentAndComboboxDelayForLoadingContent) {
  coordinator_->Show(loading_content_entry1_->key().id());
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  // A loading entry's view should be stored as the cached view and be
  // unavailable.
  views::View* loading_content = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy->IsAvailable());
  // Set the content proxy to available.
  loading_content_proxy->SetAvailable(true);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  // Switch to another entry that has loading content.
  coordinator_->Show(loading_content_entry2_->key().id());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            loading_content_entry1_->key().id());
  loading_content = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  loading_content_proxy =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy->IsAvailable());
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loading_content_entry1_->key().id());
  // Set as available and make sure the combobox has updated.
  loading_content_proxy->SetAvailable(true);
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loading_content_entry2_->key().id());
}

TEST_F(SidePanelCoordinatorLoadingContentTest,
       TriggerSwitchToNewEntryDuringContentLoad) {
  coordinator_->Show(loaded_content_entry1_->key().id());
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loaded_content_entry1_->key().id());

  // Switch to loading_content_entry1_ that has loading content.
  coordinator_->Show(loading_content_entry1_->key().id());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            loaded_content_entry1_->key().id());
  views::View* loading_content1 = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content1, nullptr);
  SidePanelContentProxy* loading_content_proxy1 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content1);
  EXPECT_FALSE(loading_content_proxy1->IsAvailable());
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loaded_content_entry1_->key().id());
  // Verify the loading_content_entry1_ is the loading entry.
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), loading_content_entry1_);

  // While that entry is loading, switch to a different entry with content that
  // needs to load.
  coordinator_->Show(loading_content_entry2_->key().id());
  views::View* loading_content2 = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content2, nullptr);
  SidePanelContentProxy* loading_content_proxy2 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content2);
  EXPECT_FALSE(loading_content_proxy2->IsAvailable());
  // Verify the loading_content_entry2_ is no longer the loading entry.
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), loading_content_entry2_);
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loaded_content_entry1_->key().id());

  // Set loading_content_entry1_ as available and verify it is not made the
  // active entry.
  loading_content_proxy1->SetAvailable(true);
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), loading_content_entry2_);
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loaded_content_entry1_->key().id());

  // Set loading_content_entry2_ as available and verify it is made the active
  // entry.
  loading_content_proxy2->SetAvailable(true);
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), nullptr);
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loading_content_entry2_->key().id());
}

TEST_F(SidePanelCoordinatorLoadingContentTest,
       TriggerSwitchToCurrentVisibleEntryDuringContentLoad) {
  coordinator_->Show(loading_content_entry1_->key().id());
  EXPECT_FALSE(browser_view()->unified_side_panel()->GetVisible());
  // A loading entry's view should be stored as the cached view and be
  // unavailable.
  views::View* loading_content = loading_content_entry1_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy1 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy1->IsAvailable());
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), loading_content_entry1_);
  // Set the content proxy to available.
  loading_content_proxy1->SetAvailable(true);
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  // Switch to loading_content_entry2_ that has loading content.
  coordinator_->Show(loading_content_entry2_->key().id());
  EXPECT_TRUE(GetLastActiveEntryKey().has_value());
  EXPECT_EQ(GetLastActiveEntryKey().value().id(),
            loading_content_entry1_->key().id());
  loading_content = loading_content_entry2_->CachedView();
  EXPECT_NE(loading_content, nullptr);
  SidePanelContentProxy* loading_content_proxy2 =
      SidePanelUtil::GetSidePanelContentProxy(loading_content);
  EXPECT_FALSE(loading_content_proxy2->IsAvailable());
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loading_content_entry1_->key().id());
  // Verify the loading_content_entry2_ is the loading entry.
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), loading_content_entry2_);

  // While that entry is loading, switch back to the currently showing entry.
  coordinator_->Show(loading_content_entry1_->key().id());
  // Verify the loading_content_entry2_ is no longer the loading entry.
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), nullptr);
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loading_content_entry1_->key().id());

  // Set loading_content_entry2_ as available and verify it is not made the
  // active entry.
  loading_content_proxy2->SetAvailable(true);
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loading_content_entry1_->key().id());

  // Show loading_content_entry2_ and verify it shows without availability
  // needing to be set again.
  coordinator_->Show(loading_content_entry2_->key().id());
  EXPECT_EQ(coordinator_->GetLoadingEntryForTesting(), nullptr);
  EXPECT_EQ(coordinator_->GetComboboxDisplayedEntryIdForTesting(),
            loading_content_entry2_->key().id());
}
