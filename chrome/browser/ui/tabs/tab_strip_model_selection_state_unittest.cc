// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_selection_state.h"

#include <memory>
#include <unordered_set>

#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

class TabStripModelSelectionStateTest : public testing::Test {
 public:
  void SetUp() override {
    tab1_ = std::make_unique<MockTabInterface>();
    tab2_ = std::make_unique<MockTabInterface>();
    tab3_ = std::make_unique<MockTabInterface>();
  }

 protected:
  std::unique_ptr<MockTabInterface> tab1_;
  std::unique_ptr<MockTabInterface> tab2_;
  std::unique_ptr<MockTabInterface> tab3_;
};

TEST_F(TabStripModelSelectionStateTest, InitialState) {
  TabStripModelSelectionState selection_state({}, nullptr, nullptr);
  EXPECT_TRUE(selection_state.selected_tabs().empty());
  EXPECT_EQ(nullptr, selection_state.active_tab());
  EXPECT_EQ(nullptr, selection_state.anchor_tab());
  EXPECT_TRUE(selection_state.Valid());
}

TEST_F(TabStripModelSelectionStateTest, Constructor) {
  std::unordered_set<raw_ptr<TabInterface>> selected_tabs = {tab1_.get(),
                                                             tab2_.get()};
  TabStripModelSelectionState selection_state(selected_tabs, tab1_.get(),
                                              tab2_.get());
  EXPECT_EQ(2u, selection_state.selected_tabs().size());
  EXPECT_TRUE(selection_state.IsSelected(tab1_.get()));
  EXPECT_TRUE(selection_state.IsSelected(tab2_.get()));
  EXPECT_EQ(tab1_.get(), selection_state.active_tab());
  EXPECT_EQ(tab2_.get(), selection_state.anchor_tab());
  EXPECT_TRUE(selection_state.Valid());
}

TEST_F(TabStripModelSelectionStateTest, IsSelected) {
  std::unordered_set<raw_ptr<TabInterface>> selected_tabs = {tab1_.get()};
  TabStripModelSelectionState selection_state(selected_tabs, tab1_.get(),
                                              tab1_.get());
  EXPECT_TRUE(selection_state.IsSelected(tab1_.get()));
  EXPECT_FALSE(selection_state.IsSelected(tab2_.get()));
}

TEST_F(TabStripModelSelectionStateTest, AddAndRemoveTabFromSelection) {
  TabStripModelSelectionState selection_state({tab1_.get()}, tab1_.get(),
                                              tab1_.get());
  EXPECT_TRUE(selection_state.IsSelected(tab1_.get()));
  EXPECT_FALSE(selection_state.IsSelected(tab2_.get()));

  selection_state.AddTabToSelection(tab2_.get());
  EXPECT_TRUE(selection_state.IsSelected(tab2_.get()));
  EXPECT_EQ(2u, selection_state.selected_tabs().size());

  // Adding again should do nothing.
  selection_state.AddTabToSelection(tab2_.get());
  EXPECT_EQ(2u, selection_state.selected_tabs().size());

  selection_state.RemoveTabFromSelection(tab1_.get());
  EXPECT_FALSE(selection_state.IsSelected(tab1_.get()));
  EXPECT_EQ(1u, selection_state.selected_tabs().size());
}

TEST_F(TabStripModelSelectionStateTest, SetActiveTab) {
  TabStripModelSelectionState selection_state({tab1_.get()}, tab1_.get(),
                                              tab1_.get());
  selection_state.SetActiveTab(tab2_.get());
  EXPECT_EQ(tab2_.get(), selection_state.active_tab());
  EXPECT_TRUE(selection_state.IsSelected(tab2_.get()));
  EXPECT_EQ(2u, selection_state.selected_tabs().size());
  EXPECT_TRUE(selection_state.Valid());
}

TEST_F(TabStripModelSelectionStateTest, SetAnchorTab) {
  TabStripModelSelectionState selection_state({tab1_.get()}, tab1_.get(),
                                              tab1_.get());
  selection_state.SetAnchorTab(tab2_.get());
  EXPECT_EQ(tab2_.get(), selection_state.anchor_tab());
  EXPECT_TRUE(selection_state.IsSelected(tab2_.get()));
  EXPECT_EQ(2u, selection_state.selected_tabs().size());
  EXPECT_TRUE(selection_state.Valid());
}

TEST_F(TabStripModelSelectionStateTest, AppendTabsToSelection) {
  TabStripModelSelectionState selection_state({tab1_.get()}, tab1_.get(),
                                              tab1_.get());
  std::unordered_set<TabInterface*> new_tabs = {tab2_.get(), tab3_.get()};
  EXPECT_TRUE(selection_state.AppendTabsToSelection(new_tabs));
  EXPECT_EQ(3u, selection_state.selected_tabs().size());
  EXPECT_TRUE(selection_state.IsSelected(tab1_.get()));
  EXPECT_TRUE(selection_state.IsSelected(tab2_.get()));
  EXPECT_TRUE(selection_state.IsSelected(tab3_.get()));

  // Appending same tabs again should return false.
  EXPECT_FALSE(selection_state.AppendTabsToSelection(new_tabs));
}

TEST_F(TabStripModelSelectionStateTest, SetSelectedTabs) {
  TabStripModelSelectionState selection_state({tab1_.get()}, tab1_.get(),
                                              tab1_.get());
  std::unordered_set<TabInterface*> new_tabs = {tab2_.get(), tab3_.get()};
  selection_state.SetSelectedTabs(new_tabs, tab2_.get(), tab3_.get());

  EXPECT_EQ(2u, selection_state.selected_tabs().size());
  EXPECT_FALSE(selection_state.IsSelected(tab1_.get()));
  EXPECT_TRUE(selection_state.IsSelected(tab2_.get()));
  EXPECT_TRUE(selection_state.IsSelected(tab3_.get()));
  EXPECT_EQ(tab2_.get(), selection_state.active_tab());
  EXPECT_EQ(tab3_.get(), selection_state.anchor_tab());
  EXPECT_TRUE(selection_state.Valid());
}

TEST_F(TabStripModelSelectionStateTest, SetSelectedTabsDefaultActiveAnchor) {
  TabStripModelSelectionState selection_state({tab1_.get()}, tab1_.get(),
                                              tab1_.get());
  std::unordered_set<TabInterface*> new_tabs = {tab2_.get(), tab3_.get()};
  selection_state.SetSelectedTabs(new_tabs);

  EXPECT_EQ(2u, selection_state.selected_tabs().size());
  EXPECT_FALSE(selection_state.IsSelected(tab1_.get()));
  EXPECT_TRUE(selection_state.IsSelected(tab2_.get()));
  EXPECT_TRUE(selection_state.IsSelected(tab3_.get()));
  // Active and anchor should be one of the new tabs.
  EXPECT_TRUE(selection_state.IsSelected(selection_state.active_tab()));
  EXPECT_TRUE(selection_state.IsSelected(selection_state.anchor_tab()));
  EXPECT_TRUE(selection_state.Valid());
}

TEST_F(TabStripModelSelectionStateTest, Valid) {
  // Empty is valid.
  TabStripModelSelectionState selection_state_empty({}, nullptr, nullptr);
  EXPECT_TRUE(selection_state_empty.Valid());

  // Non-empty with active and anchor is valid.
  TabStripModelSelectionState selection_state_valid({tab1_.get()}, tab1_.get(),
                                                    tab1_.get());
  EXPECT_TRUE(selection_state_valid.Valid());
}

TEST_F(TabStripModelSelectionStateTest, InvalidStates) {
  // Empty selection with active tab is invalid.
  EXPECT_FALSE(TabStripModelSelectionState({}, tab1_.get(), nullptr).Valid());

  // Empty selection with anchor tab is invalid.
  EXPECT_FALSE(TabStripModelSelectionState({}, nullptr, tab1_.get()).Valid());

  // Non-empty selection with null active tab is invalid.
  EXPECT_FALSE(
      TabStripModelSelectionState({tab1_.get()}, nullptr, tab1_.get()).Valid());

  // Non-empty selection with null anchor tab is invalid.
  EXPECT_FALSE(
      TabStripModelSelectionState({tab1_.get()}, tab1_.get(), nullptr).Valid());

  // Active tab not in selection is invalid.
  EXPECT_FALSE(
      TabStripModelSelectionState({tab1_.get()}, tab2_.get(), tab1_.get())
          .Valid());

  // Anchor tab not in selection is invalid.
  EXPECT_FALSE(
      TabStripModelSelectionState({tab1_.get()}, tab1_.get(), tab2_.get())
          .Valid());
}

}  // namespace tabs
