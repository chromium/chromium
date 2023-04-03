// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_model.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/token.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

void CompareSavedTabGroupTabs(const std::vector<SavedTabGroupTab>& v1,
                              const std::vector<SavedTabGroupTab>& v2) {
  ASSERT_EQ(v1.size(), v2.size());
  for (size_t i = 0; i < v1.size(); i++) {
    SavedTabGroupTab tab1 = v1[i];
    SavedTabGroupTab tab2 = v2[i];
    EXPECT_EQ(tab1.url(), tab2.url());
    EXPECT_EQ(tab1.title(), tab2.title());
    EXPECT_EQ(tab1.favicon(), tab2.favicon());
  }
}

bool CompareSavedTabGroups(const SavedTabGroup& g1, const SavedTabGroup& g2) {
  if (g1.title() != g2.title())
    return false;
  if (g1.color() != g2.color())
    return false;
  if (g1.position() != g2.position())
    return false;
  if (g1.saved_guid() != g2.saved_guid())
    return false;
  if (g1.creation_time_windows_epoch_micros() !=
      g2.creation_time_windows_epoch_micros()) {
    return false;
  }

  return true;
}

SavedTabGroup CreateSavedTabGroup(
    const std::u16string& group_title,
    const tab_groups::TabGroupColorId& color,
    const std::vector<SavedTabGroupTab>& group_tabs,
    const base::Uuid& id) {
  return SavedTabGroup(group_title, color, group_tabs, id);
}

SavedTabGroupTab CreateSavedTabGroupTab(const std::string& url,
                                        const std::u16string& title,
                                        const base::Uuid& group_guid,
                                        absl::optional<int> position) {
  SavedTabGroupTab tab(GURL(url), title, group_guid, nullptr, absl::nullopt,
                       absl::nullopt, position);
  tab.SetFavicon(gfx::Image());
  return tab;
}

SavedTabGroup CreateTestSavedTabGroup() {
  base::Uuid id = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Test Test";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;

  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("www.google.com", u"Google", id, absl::nullopt);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("chrome://newtab", u"new tab", id, absl::nullopt);

  tab1.SetFavicon(gfx::Image());
  tab2.SetFavicon(gfx::Image());

  std::vector<SavedTabGroupTab> tabs = {tab1, tab2};

  SavedTabGroup group(title, color, tabs, id);
  return group;
}

}  // namespace

// Serves to test the functions in SavedTabGroupModelObserver.
class SavedTabGroupModelObserverTest : public ::testing::Test,
                                       public SavedTabGroupModelObserver {
 protected:
  SavedTabGroupModelObserverTest() = default;
  ~SavedTabGroupModelObserverTest() override = default;

  void SetUp() override {
    saved_tab_group_model_ = std::make_unique<SavedTabGroupModel>();
    saved_tab_group_model_->AddObserver(this);
  }

  void TearDown() override { saved_tab_group_model_.reset(); }

  void SavedTabGroupAddedLocally(const base::Uuid& guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(guid));
    retrieved_index_ = saved_tab_group_model_->GetIndexOf(guid).value_or(-1);
  }

  void SavedTabGroupRemovedLocally(
      const SavedTabGroup* removed_group) override {
    retrieved_guid_ = removed_group->saved_guid();
  }

  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid = absl::nullopt) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(group_guid));
    retrieved_index_ =
        saved_tab_group_model_->GetIndexOf(group_guid).value_or(-1);
  }

  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(guid));
    retrieved_index_ = saved_tab_group_model_->GetIndexOf(guid).value_or(-1);
  }

  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override {
    retrieved_guid_ = removed_group->saved_guid();
  }

  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid = absl::nullopt) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(group_guid));
    retrieved_index_ =
        saved_tab_group_model_->GetIndexOf(group_guid).value_or(-1);
  }

  void SavedTabGroupReorderedLocally() override { reordered_called_ = true; }

  void ClearSignals() {
    retrieved_group_.clear();
    retrieved_index_ = -1;
    retrieved_old_index_ = -1;
    retrieved_new_index_ = -1;
    reordered_called_ = false;
    retrieved_guid_ = base::Uuid::GenerateRandomV4();
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::vector<SavedTabGroup> retrieved_group_;
  int retrieved_index_ = -1;
  int retrieved_old_index_ = -1;
  int retrieved_new_index_ = -1;
  bool reordered_called_ = false;
  base::Uuid retrieved_guid_ = base::Uuid::GenerateRandomV4();
  std::string base_path_ = "file:///c:/tmp/";
};

// Serves to test the functions in SavedTabGroupModel.
class SavedTabGroupModelTest : public ::testing::Test {
 protected:
  SavedTabGroupModelTest()
      : id_1_(base::Uuid::GenerateRandomV4()),
        id_2_(base::Uuid::GenerateRandomV4()),
        id_3_(base::Uuid::GenerateRandomV4()) {}

  ~SavedTabGroupModelTest() override { RemoveTestData(); }

  void SetUp() override {
    saved_tab_group_model_ = std::make_unique<SavedTabGroupModel>();
    AddTestData();
  }

  void TearDown() override {
    RemoveTestData();
    saved_tab_group_model_.reset();
  }

  void AddTestData() {
    const std::u16string title_1 = u"Group One";
    const std::u16string title_2 = u"Another Group";
    const std::u16string title_3 = u"The Three Musketeers";

    const tab_groups::TabGroupColorId& color_1 =
        tab_groups::TabGroupColorId::kGrey;
    const tab_groups::TabGroupColorId& color_2 =
        tab_groups::TabGroupColorId::kRed;
    const tab_groups::TabGroupColorId& color_3 =
        tab_groups::TabGroupColorId::kGreen;

    std::vector<SavedTabGroupTab> group_1_tabs = {
        CreateSavedTabGroupTab("A_Link", u"Only Tab", id_1_, 0)};
    std::vector<SavedTabGroupTab> group_2_tabs = {
        CreateSavedTabGroupTab("One_Link", u"One Of Two", id_2_, 0),
        CreateSavedTabGroupTab("Two_Link", u"Second", id_2_, 1)};
    std::vector<SavedTabGroupTab> group_3_tabs = {
        CreateSavedTabGroupTab("Athos", u"All For One", id_3_, 0),
        CreateSavedTabGroupTab("Porthos", u"And", id_3_, 1),
        CreateSavedTabGroupTab("Aramis", u"One For All", id_3_, 2)};

    saved_tab_group_model_->Add(
        CreateSavedTabGroup(title_1, color_1, group_1_tabs, id_1_));
    saved_tab_group_model_->Add(
        CreateSavedTabGroup(title_2, color_2, group_2_tabs, id_2_));
    saved_tab_group_model_->Add(
        CreateSavedTabGroup(title_3, color_3, group_3_tabs, id_3_));
  }

  void RemoveTestData() {
    if (!saved_tab_group_model_)
      return;
    // Copy ids so we do not remove elements while we are accessing the data.
    std::vector<base::Uuid> saved_tab_group_ids;
    for (const SavedTabGroup& saved_group :
         saved_tab_group_model_->saved_tab_groups()) {
      saved_tab_group_ids.emplace_back(saved_group.saved_guid());
    }

    for (const auto& id : saved_tab_group_ids) {
      saved_tab_group_model_->Remove(id);
    }
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::string base_path_ = "file:///c:/tmp/";
  base::Uuid id_1_;
  base::Uuid id_2_;
  base::Uuid id_3_;
};

// Tests that SavedTabGroupModel::Count holds 3 elements initially.
TEST_F(SavedTabGroupModelTest, InitialCountThree) {
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);
  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), 3u);
}

// Tests that SavedTabGroupModel::Contains returns the 3, the number of starting
// ids added to the model.
TEST_F(SavedTabGroupModelTest, InitialGroupsAreSaved) {
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_1_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_2_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_3_));
  EXPECT_FALSE(
      saved_tab_group_model_->Contains(base::Uuid::GenerateRandomV4()));
}

// Tests that the SavedTabGroupModel::GetIndexOf preserves the order the
// SavedTabGroups were inserted into.
TEST_F(SavedTabGroupModelTest, InitialOrderAdded) {
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_1_), 0);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_3_), 2);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_2_), 1);
}

// Tests that the SavedTabGroupModel::IsEmpty has elements and once all elements
// are removed is empty.
TEST_F(SavedTabGroupModelTest, ContainsNoElementsOnRemoval) {
  EXPECT_FALSE(saved_tab_group_model_->IsEmpty());
  RemoveTestData();
  EXPECT_TRUE(saved_tab_group_model_->IsEmpty());
}

// Tests that the SavedTabGroupModel::Remove removes the correct element given
// an id.
TEST_F(SavedTabGroupModelTest, RemovesCorrectElements) {
  saved_tab_group_model_->Remove(id_3_);
  EXPECT_FALSE(saved_tab_group_model_->Contains(id_3_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_2_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_1_));
}

// Tests that the SavedTabGroupModel only adds unique TabGroupIds.
TEST_F(SavedTabGroupModelTest, OnlyAddUniqueElements) {
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);
  EXPECT_CHECK_DEATH(AddTestData());
}

// Tests that SavedTabGroupModel::Add adds an extra element into the model and
// keeps the data.
TEST_F(SavedTabGroupModelTest, AddNewElement) {
  base::Uuid id_4 = base::Uuid::GenerateRandomV4();
  const std::u16string title_4 = u"Test Test";
  const tab_groups::TabGroupColorId& color_4 =
      tab_groups::TabGroupColorId::kBlue;

  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_4, 0);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_4, 1);

  std::vector<SavedTabGroupTab> group_4_tabs = {tab1, tab2};
  SavedTabGroup group_4(title_4, color_4, group_4_tabs, id_4);
  saved_tab_group_model_->Add(group_4);

  EXPECT_TRUE(saved_tab_group_model_->Contains(id_4));
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_4), 3);
  EXPECT_EQ(saved_tab_group_model_->Count(), 4);

  const SavedTabGroup* saved_group = saved_tab_group_model_->Get(id_4);
  EXPECT_EQ(saved_group->saved_guid(), id_4);
  EXPECT_EQ(saved_group->title(), title_4);
  EXPECT_EQ(saved_group->color(), color_4);
  CompareSavedTabGroupTabs(saved_group->saved_tabs(), group_4_tabs);
}

// Tests that SavedTabGroupModel::Update updates the correct element if the
// title or color are different.
TEST_F(SavedTabGroupModelTest, UpdateElement) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  const std::u16string original_title = group->title();
  const tab_groups::TabGroupColorId& original_color = group->color();

  // Should only update the element if title or color are different
  const std::u16string same_title = u"Group One";
  const tab_groups::TabGroupColorId& same_color =
      tab_groups::TabGroupColorId::kGrey;
  const tab_groups::TabGroupVisualData same_visual_data(same_title, same_color,
                                                        /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(id_1_, &same_visual_data);
  EXPECT_EQ(group->title(), original_title);
  EXPECT_EQ(group->color(), original_color);

  // Updates both color and title
  const std::u16string new_title = u"New Title";
  const tab_groups::TabGroupColorId& new_color =
      tab_groups::TabGroupColorId::kCyan;
  const tab_groups::TabGroupVisualData new_visual_data(new_title, new_color,
                                                       /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(id_1_, &new_visual_data);
  EXPECT_EQ(group->title(), new_title);
  EXPECT_EQ(group->color(), new_color);

  // Update only title
  const std::u16string random_title = u"Random Title";
  const tab_groups::TabGroupVisualData change_title_visual_data(
      random_title, original_color, /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(id_1_, &change_title_visual_data);
  EXPECT_EQ(group->title(), random_title);
  EXPECT_EQ(group->color(), original_color);

  // Update only color
  const tab_groups::TabGroupColorId& random_color =
      tab_groups::TabGroupColorId::kGrey;
  const tab_groups::TabGroupVisualData change_color_visual_data(
      original_title, random_color, /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(id_1_, &change_color_visual_data);
  EXPECT_EQ(group->title(), original_title);
  EXPECT_EQ(group->color(), random_color);
}

// Tests that the correct tabs are added to the correct position in group 1.
TEST_F(SavedTabGroupModelTest, AddTabToGroup) {
  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_1_, 0);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_1_, 2);

  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1,
                                        /*update_tab_positions=*/true);
  EXPECT_EQ(group->saved_tabs().size(), size_t(2));
  EXPECT_EQ(0, group->GetIndexOfTab(tab1.saved_tab_guid()));
  EXPECT_TRUE(group->ContainsTab(tab1.saved_tab_guid()));
  ASSERT_TRUE(group->GetTab(tab1.saved_tab_guid()));
  CompareSavedTabGroupTabs({*group->GetTab(tab1.saved_tab_guid())}, {tab1});

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2,
                                        /*update_tab_positions=*/true);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));
  EXPECT_EQ(2, group->GetIndexOfTab(tab2.saved_tab_guid()));
  EXPECT_TRUE(group->ContainsTab(tab2.saved_tab_guid()));
  ASSERT_TRUE(group->GetTab(tab2.saved_tab_guid()));
  CompareSavedTabGroupTabs({*group->GetTab(tab2.saved_tab_guid())}, {tab2});
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {tab1, group->saved_tabs()[1], tab2});
}

// Tests that the correct tabs are removed from the correct position in group 1.
TEST_F(SavedTabGroupModelTest, RemoveTabFromGroup) {
  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_1_, 0);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_1_, 2);

  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1,
                                        /*update_tab_positions=*/true);
  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2,
                                        /*update_tab_positions=*/true);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->RemoveTabFromGroup(group->saved_guid(),
                                             tab1.saved_tab_guid(),
                                             /*update_tab_positions=*/true);
  EXPECT_EQ(group->saved_tabs().size(), size_t(2));
  CompareSavedTabGroupTabs(group->saved_tabs(), {group->saved_tabs()[0], tab2});

  saved_tab_group_model_->RemoveTabFromGroup(group->saved_guid(),
                                             tab2.saved_tab_guid(),
                                             /*update_tab_positions=*/true);
  EXPECT_EQ(group->saved_tabs().size(), size_t(1));
  CompareSavedTabGroupTabs(group->saved_tabs(), {group->saved_tabs()[0]});
}

// Tests that a group is removed from the model when the last tab is removed
// from it.
TEST_F(SavedTabGroupModelTest, RemoveLastTabFromGroup) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->RemoveTabFromGroup(
      group->saved_guid(), group->saved_tabs()[0].saved_tab_guid(),
      /*update_tab_positions=*/true);

  EXPECT_FALSE(saved_tab_group_model_->Contains(id_1_));
}

// Tests that the correct tabs are replaced in group 1.
TEST_F(SavedTabGroupModelTest, ReplaceTabInGroup) {
  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("first", u"First Tab", id_1_, 0);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("second", u"Second Tab", id_1_, 2);
  SavedTabGroupTab tab3 =
      CreateSavedTabGroupTab("third", u"Third Tab", id_1_, absl::nullopt);

  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1,
                                        /*update_tab_positions=*/true);
  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2,
                                        /*update_tab_positions=*/true);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->ReplaceTabInGroupAt(group->saved_guid(),
                                              tab1.saved_tab_guid(), tab3);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {tab3, group->saved_tabs()[1], tab2});

  saved_tab_group_model_->ReplaceTabInGroupAt(group->saved_guid(),
                                              tab2.saved_tab_guid(), tab1);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {tab3, group->saved_tabs()[1], tab1});

  saved_tab_group_model_->ReplaceTabInGroupAt(
      group->saved_guid(), group->saved_tabs()[1].saved_tab_guid(), tab2);
  CompareSavedTabGroupTabs(group->saved_tabs(), {tab3, tab2, tab1});
}

// Tests that the correct tabs are moved in group 1.
TEST_F(SavedTabGroupModelTest, MoveTabInGroup) {
  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_1_, 0);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_1_, 2);

  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1,
                                        /*update_tab_positions=*/true);
  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2,
                                        /*update_tab_positions=*/true);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->MoveTabInGroupTo(group->saved_guid(),
                                           tab1.saved_tab_guid(), 2);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {group->saved_tabs()[0], tab2, tab1});

  saved_tab_group_model_->MoveTabInGroupTo(group->saved_guid(),
                                           tab1.saved_tab_guid(), 1);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {group->saved_tabs()[0], tab1, tab2});
}

TEST_F(SavedTabGroupModelTest, MoveElement) {
  ASSERT_EQ(0, saved_tab_group_model_->GetIndexOf(id_1_));
  ASSERT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
  ASSERT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
  saved_tab_group_model_->Reorder(id_2_, 2);
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_1_));
  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_3_));
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_2_));
  saved_tab_group_model_->Reorder(id_2_, 0);
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_2_));
  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_1_));
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
  saved_tab_group_model_->Reorder(id_2_, 1);
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_1_));
  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
}

TEST_F(SavedTabGroupModelTest, LoadStoredEntriesPopulatesModel) {
  std::unique_ptr<SavedTabGroup> group =
      std::make_unique<SavedTabGroup>(*saved_tab_group_model_->Get(id_3_));

  std::vector<sync_pb::SavedTabGroupSpecifics> specifics;
  specifics.emplace_back(*group->ToSpecifics());

  for (SavedTabGroupTab tab : group->saved_tabs())
    specifics.emplace_back(*tab.ToSpecifics());

  EXPECT_EQ(specifics.size(), size_t(4));
  saved_tab_group_model_->Remove(id_3_);

  saved_tab_group_model_->LoadStoredEntries(specifics);

  EXPECT_TRUE(saved_tab_group_model_->Contains(id_3_));
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_3_), 2);
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);

  const SavedTabGroup* saved_group = saved_tab_group_model_->Get(id_3_);
  EXPECT_EQ(saved_group->saved_guid(), id_3_);
  EXPECT_EQ(saved_group->title(), group->title());
  EXPECT_EQ(saved_group->color(), group->color());

  // We can not use CompareSavedTabGroupTabs because the favicons are not loaded
  // until the tab is opened through the saved group button.
  EXPECT_EQ(saved_group->saved_tabs().size(), group->saved_tabs().size());
}

// Tests that merging a group with the same group_id changes the state of the
// object correctly.
TEST_F(SavedTabGroupModelTest, MergeGroupsFromModel) {
  const SavedTabGroup* group1 = saved_tab_group_model_->Get(id_1_);
  SavedTabGroup group2 = SavedTabGroup::FromSpecifics(*group1->ToSpecifics());
  group2.SetColor(tab_groups::TabGroupColorId::kPink);
  group2.SetTitle(u"Updated title");
  SavedTabGroup merged_group = SavedTabGroup::FromSpecifics(
      *saved_tab_group_model_->MergeGroup(*group2.ToSpecifics()));

  EXPECT_EQ(group2.title(), merged_group.title());
  EXPECT_EQ(group2.color(), merged_group.color());
  EXPECT_EQ(group2.saved_guid(), merged_group.saved_guid());
  EXPECT_EQ(group2.creation_time_windows_epoch_micros(),
            merged_group.creation_time_windows_epoch_micros());
  EXPECT_EQ(group2.update_time_windows_epoch_micros(),
            merged_group.update_time_windows_epoch_micros());
}

// Tests that merging a tab with the same tab_id changes the state of the object
// correctly.
TEST_F(SavedTabGroupModelTest, MergeTabsFromModel) {
  SavedTabGroupTab tab1 = saved_tab_group_model_->Get(id_1_)->saved_tabs()[0];
  SavedTabGroupTab tab2 = SavedTabGroupTab::FromSpecifics(*tab1.ToSpecifics());
  tab2.SetTitle(u"Updated Title");
  tab2.SetURL(GURL("chrome://updated_url"));

  SavedTabGroupTab merged_tab = SavedTabGroupTab::FromSpecifics(
      *saved_tab_group_model_->MergeTab(*tab2.ToSpecifics()));

  EXPECT_EQ(tab2.url(), merged_tab.url());
  EXPECT_EQ(tab2.saved_tab_guid(), merged_tab.saved_tab_guid());
  EXPECT_EQ(tab2.saved_group_guid(), merged_tab.saved_group_guid());
  EXPECT_EQ(tab2.creation_time_windows_epoch_micros(),
            merged_tab.creation_time_windows_epoch_micros());
  EXPECT_EQ(tab2.update_time_windows_epoch_micros(),
            merged_tab.update_time_windows_epoch_micros());
}

// Tests that groups inserted in the model are in order stay inserted in sorted
// order.
TEST_F(SavedTabGroupModelTest, GroupsSortedWithInOrderPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {});
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {});
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {});
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {});
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {});

  // Set the positions the groups should sit in the bookmarks bar.
  group_1.SetPosition(0);
  group_2.SetPosition(1);
  group_3.SetPosition(2);
  group_4.SetPosition(3);
  group_5.SetPosition(4);
  group_6.SetPosition(5);

  // This is the order we expect the groups in the model to be.
  std::vector<SavedTabGroup> groups = {group_1, group_2, group_3,
                                       group_4, group_5, group_6};

  // Add the groups into the model in order.
  saved_tab_group_model_->Add(group_1);
  saved_tab_group_model_->Add(group_2);
  saved_tab_group_model_->Add(group_3);
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Add(group_5);
  saved_tab_group_model_->Add(group_6);

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model out of order are still inserted in
// sorted order.
TEST_F(SavedTabGroupModelTest, GroupsSortedWithOutOfOrderPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {});
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {});
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {});
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {});
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {});

  // Set the positions the groups should sit in the bookmarks bar.
  group_1.SetPosition(0);
  group_2.SetPosition(1);
  group_3.SetPosition(2);
  group_4.SetPosition(3);
  group_5.SetPosition(4);
  group_6.SetPosition(5);

  // This is the order we expect the groups in the model to be.
  std::vector<SavedTabGroup> groups = {group_1, group_2, group_3,
                                       group_4, group_5, group_6};

  // Add the groups into the model in an arbitrary order.
  saved_tab_group_model_->Add(group_6);
  saved_tab_group_model_->Add(group_1);
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Add(group_3);
  saved_tab_group_model_->Add(group_5);
  saved_tab_group_model_->Add(group_2);

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with gaps between the positions are
// still inserted in sorted order.
TEST_F(SavedTabGroupModelTest, GroupsSortedWithGapsInPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {});
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {});
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {});
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {});
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {});

  // Set the positions the groups should sit in the bookmarks bar.
  group_1.SetPosition(0);
  group_2.SetPosition(3);
  group_3.SetPosition(8);
  group_4.SetPosition(19);
  group_5.SetPosition(21);
  group_6.SetPosition(34);

  // This is the order we expect the groups in the model to be.
  std::vector<SavedTabGroup> groups = {group_1, group_2, group_3,
                                       group_4, group_5, group_6};

  // Add the groups into the model in an arbitrary order.
  saved_tab_group_model_->Add(group_6);
  saved_tab_group_model_->Add(group_1);
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Add(group_3);
  saved_tab_group_model_->Add(group_5);
  saved_tab_group_model_->Add(group_2);

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with gaps and in decreasing order
// between the positions are still inserted in increasing sorted order.
TEST_F(SavedTabGroupModelTest, GroupsSortedWithDecreasingPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {});
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {});
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {});
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {});
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {});

  // Set the positions the groups should sit in the bookmarks bar.
  group_1.SetPosition(0);
  group_2.SetPosition(3);
  group_3.SetPosition(8);
  group_4.SetPosition(19);
  group_5.SetPosition(21);
  group_6.SetPosition(34);

  // This is the order we expect the groups in the model to be.
  std::vector<SavedTabGroup> groups = {group_1, group_2, group_3,
                                       group_4, group_5, group_6};

  // Add the groups into the model in an arbitrary order.
  saved_tab_group_model_->Add(group_6);
  saved_tab_group_model_->Add(group_5);
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Add(group_3);
  saved_tab_group_model_->Add(group_2);
  saved_tab_group_model_->Add(group_1);

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with a more recent update time take
// precedence over groups with the same position.
TEST_F(SavedTabGroupModelTest, GroupWithSamePositionSortedByUpdateTime) {
  RemoveTestData();

  // Create an arbitrary number of groups.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {});
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {});

  // Set the positions the groups should sit in the bookmarks bar.
  group_1.SetPosition(0);
  group_2.SetPosition(0);

  // This is the order we expect the groups in the model to be.
  std::vector<SavedTabGroup> groups = {group_2, group_1};

  // Add the groups into the model in an arbitrary order.
  saved_tab_group_model_->Add(group_1);
  saved_tab_group_model_->Add(group_2);

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with no position are inserted at the
// back of the model and have their position set to the last index at the time
// they were inserted.
TEST_F(SavedTabGroupModelTest, GroupsWithNoPositionInsertedAtEnd) {
  RemoveTestData();

  // Create an arbitrary number of groups.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {});
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {});
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {});
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {});
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {});

  // Set the positions the groups should sit in the bookmarks bar.
  group_1.SetPosition(0);
  group_2.SetPosition(1);
  group_3.SetPosition(2);
  group_4.SetPosition(3);
  group_5.SetPosition(4);

  // This is the order we expect the groups in the model to be.
  std::vector<SavedTabGroup> groups = {group_1, group_2, group_3,
                                       group_4, group_5, group_6};

  // Add the groups into the model in an arbitrary order.
  saved_tab_group_model_->Add(group_1);
  saved_tab_group_model_->Add(group_2);
  saved_tab_group_model_->Add(group_3);
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Add(group_5);
  saved_tab_group_model_->Add(group_6);

  groups[5].SetPosition(5);

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());

  // Expect the 6th group to have a position of 5 (0-based indexing).
  EXPECT_EQ(saved_tab_group_model_
                ->saved_tab_groups()
                    [saved_tab_group_model_->saved_tab_groups().size() - 1]
                .position(),
            groups[5].position());

  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that SavedTabGroupModelObserver::Added passes the correct element from
// the model.
TEST_F(SavedTabGroupModelObserverTest, AddElement) {
  SavedTabGroup group_4(CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(group_4.title(), received_group.title());
  EXPECT_EQ(group_4.color(), received_group.color());
  CompareSavedTabGroupTabs(group_4.saved_tabs(), received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Tests that SavedTabGroupModelObserver::Removed passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, RemovedElement) {
  SavedTabGroup group_4(CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Remove(group_4.saved_guid());

  EXPECT_EQ(group_4.saved_guid(), retrieved_guid_);
  EXPECT_FALSE(saved_tab_group_model_->Contains(retrieved_guid_));

  // The model will have already removed and sent the index our element was at
  // before it was removed from the model. As such, we should get -1 when
  // checking the model and 0 for the retrieved index.
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(retrieved_guid_), absl::nullopt);
}

// Tests that SavedTabGroupModelObserver::Updated passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, UpdatedElement) {
  SavedTabGroup group_4(CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);

  const std::u16string new_title = u"New Title";
  const tab_groups::TabGroupColorId& new_color =
      tab_groups::TabGroupColorId::kBlue;

  const tab_groups::TabGroupVisualData new_visual_data(new_title, new_color,
                                                       /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(group_4.saved_guid(),
                                           &new_visual_data);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(new_title, received_group.title());
  EXPECT_EQ(new_color, received_group.color());
  CompareSavedTabGroupTabs(group_4.saved_tabs(), received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Tests that SavedTabGroupModelObserver::AddedFromSync passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, AddElementFromSync) {
  SavedTabGroup group_4(CreateTestSavedTabGroup());
  saved_tab_group_model_->AddedFromSync(group_4);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(group_4.title(), received_group.title());
  EXPECT_EQ(group_4.color(), received_group.color());
  CompareSavedTabGroupTabs(group_4.saved_tabs(), received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Tests that SavedTabGroupModelObserver::RemovedFromSync passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, RemovedElementFromSync) {
  SavedTabGroup group_4(CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->RemovedFromSync(group_4.saved_guid());

  EXPECT_EQ(group_4.saved_guid(), retrieved_guid_);
  EXPECT_FALSE(saved_tab_group_model_->Contains(retrieved_guid_));

  // The model will have already removed and sent the index our element was at
  // before it was removed from the model. As such, we should get -1 when
  // checking the model and 0 for the retrieved index.
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(retrieved_guid_), absl::nullopt);
}

// Tests that SavedTabGroupModelObserver::UpdatedFromSync passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, UpdatedElementFromSync) {
  SavedTabGroup group_4(CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);

  const std::u16string new_title = u"New Title";
  const tab_groups::TabGroupColorId& new_color =
      tab_groups::TabGroupColorId::kBlue;

  const tab_groups::TabGroupVisualData new_visual_data(new_title, new_color,
                                                       /*is_collapsed*/ false);
  saved_tab_group_model_->UpdatedVisualDataFromSync(group_4.saved_guid(),
                                                    &new_visual_data);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(new_title, received_group.title());
  EXPECT_EQ(new_color, received_group.color());
  CompareSavedTabGroupTabs(group_4.saved_tabs(), received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Verify that SavedTabGroupModel::OnGroupClosedInTabStrip passes the correct
// index.
TEST_F(SavedTabGroupModelObserverTest, OnGroupClosedInTabStrip) {
  SavedTabGroup group_4 = CreateTestSavedTabGroup();
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  group_4.SetLocalGroupId(tab_group_id);
  saved_tab_group_model_->Add(group_4);
  const int index =
      saved_tab_group_model_->GetIndexOf(group_4.saved_guid()).value();
  ASSERT_GE(index, 0);

  // Expect the saved group that calls update is the one that was removed from
  // the tabstrip.
  saved_tab_group_model_->OnGroupClosedInTabStrip(
      group_4.local_group_id().value());
  EXPECT_EQ(index, retrieved_index_);

  // Expect the removal of group_4 from the tabstrip makes GetIndexOf not return
  // a valid index when searched by tab group id, but does return the right
  // index when searched by saved guid.
  saved_tab_group_model_->OnGroupClosedInTabStrip(tab_group_id);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(tab_group_id), absl::nullopt);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(group_4.saved_guid()), index);
}

// Tests that SavedTabGroupModelObserver::Moved passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, MoveElement) {
  SavedTabGroup stg_1(std::u16string(u"stg_1"),
                      tab_groups::TabGroupColorId::kGrey, {},
                      base::Uuid::GenerateRandomV4());
  SavedTabGroup stg_2(std::u16string(u"stg_2"),
                      tab_groups::TabGroupColorId::kGrey, {},
                      base::Uuid::GenerateRandomV4());
  SavedTabGroup stg_3(std::u16string(u"stg_3"),
                      tab_groups::TabGroupColorId::kGrey, {},
                      base::Uuid::GenerateRandomV4());

  saved_tab_group_model_->Add(stg_1);
  saved_tab_group_model_->Add(stg_2);
  saved_tab_group_model_->Add(stg_3);

  saved_tab_group_model_->Reorder(stg_2.saved_guid(), 2);

  EXPECT_TRUE(reordered_called_);
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(stg_2.saved_guid()));
}

TEST_F(SavedTabGroupModelObserverTest, GetGroupContainingTab) {
  // Add a non matching SavedTabGroup.
  saved_tab_group_model_->Add(CreateTestSavedTabGroup());

  // Add a matching group/tab and save the ids used for GetGroupContainingTab.
  SavedTabGroup matching_group = CreateTestSavedTabGroup();
  base::Uuid matching_group_guid = matching_group.saved_guid();

  base::Uuid matching_tab_guid = base::Uuid::GenerateRandomV4();
  base::Token matching_local_tab_id = base::Token::CreateRandom();

  SavedTabGroupTab tab(GURL(url::kAboutBlankURL), std::u16string(u"title"),
                       matching_group.saved_guid(), &matching_group,
                       matching_tab_guid, matching_local_tab_id);
  matching_group.AddTab(std::move(tab));
  saved_tab_group_model_->Add(std::move(matching_group));

  // Add another non matching SavedTabGroup.
  saved_tab_group_model_->Add(CreateTestSavedTabGroup());
  ASSERT_EQ(3, saved_tab_group_model_->Count());

  // call GetGroupContainingTab with the 2 ids and expect them to return.
  EXPECT_EQ(saved_tab_group_model_->Get(matching_group_guid),
            saved_tab_group_model_->GetGroupContainingTab(matching_tab_guid));
  EXPECT_EQ(
      saved_tab_group_model_->Get(matching_group_guid),
      saved_tab_group_model_->GetGroupContainingTab(matching_local_tab_id));

  // Expect GetGroupContainingTab to return null when there is no match.
  EXPECT_EQ(nullptr, saved_tab_group_model_->GetGroupContainingTab(
                         base::Uuid::GenerateRandomV4()));
  EXPECT_EQ(nullptr,
            saved_tab_group_model_->GetGroupContainingTab(base::Token()));
}
