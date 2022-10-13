// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_model.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
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

namespace {

base::GUID GenerateNextGUID() {
  static uint64_t guid_increment;
  if (!guid_increment) {
    guid_increment = 0;
  }

  uint64_t kBytes[] = {0, guid_increment};
  base::GUID guid =
      base::GUID::ParseCaseInsensitive(base::RandomDataToGUIDString(kBytes));

  guid_increment++;
  return guid;
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

  void SavedTabGroupAddedLocally(const base::GUID& guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(guid));
    retrieved_index_ = saved_tab_group_model_->GetIndexOf(guid).value_or(-1);
  }

  void SavedTabGroupRemovedLocally(
      const SavedTabGroup* removed_group) override {
    retrieved_guid_ = removed_group->saved_guid();
  }

  void SavedTabGroupUpdatedLocally(const base::GUID& guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(guid));
    retrieved_index_ = saved_tab_group_model_->GetIndexOf(guid).value_or(-1);
  }

  void SavedTabGroupAddedFromSync(const base::GUID& guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(guid));
    retrieved_index_ = saved_tab_group_model_->GetIndexOf(guid).value_or(-1);
  }

  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override {
    retrieved_guid_ = removed_group->saved_guid();
  }

  void SavedTabGroupUpdatedFromSync(const base::GUID& guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(guid));
    retrieved_index_ = saved_tab_group_model_->GetIndexOf(guid).value_or(-1);
  }

  void SavedTabGroupReorderedLocally() override { reordered_called_ = true; }

  void ClearSignals() {
    retrieved_group_.clear();
    retrieved_index_ = -1;
    retrieved_old_index_ = -1;
    retrieved_new_index_ = -1;
    reordered_called_ = false;
    retrieved_guid_ = base::GUID::GenerateRandomV4();
  }

  SavedTabGroupTab CreateSavedTabGroupTab(const std::string& url,
                                          const std::u16string& title) {
    SavedTabGroupTab tab(GURL(base_path_ + url),
                         base::GUID::GenerateRandomV4());
    tab.SetTitle(title).SetFavicon(gfx::Image());
    return tab;
  }

  SavedTabGroup CreateTestSavedTabGroup() {
    base::GUID id_4 = GenerateNextGUID();
    const std::u16string title_4 = u"Test Test";
    const tab_groups::TabGroupColorId& color_4 =
        tab_groups::TabGroupColorId::kBlue;

    SavedTabGroupTab tab1 = CreateSavedTabGroupTab("4th group", u"first tab");
    SavedTabGroupTab tab2 = CreateSavedTabGroupTab("2nd link", u"new tab");
    std::vector<SavedTabGroupTab> group_4_tabs = {tab1, tab2};

    SavedTabGroup group_4(title_4, color_4, group_4_tabs);
    return group_4;
  }

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

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::vector<SavedTabGroup> retrieved_group_;
  int retrieved_index_ = -1;
  int retrieved_old_index_ = -1;
  int retrieved_new_index_ = -1;
  bool reordered_called_ = false;
  base::GUID retrieved_guid_ = base::GUID::GenerateRandomV4();
  std::string base_path_ = "file:///c:/tmp/";
};

// Serves to test the functions in SavedTabGroupModel.
class SavedTabGroupModelTest : public ::testing::Test {
 protected:
  SavedTabGroupModelTest()
      : id_1_(GenerateNextGUID()),
        id_2_(GenerateNextGUID()),
        id_3_(GenerateNextGUID()) {}

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
        CreateSavedTabGroupTab("A_Link", u"Only Tab", id_1_)};
    std::vector<SavedTabGroupTab> group_2_tabs = {
        CreateSavedTabGroupTab("One_Link", u"One Of Two", id_2_),
        CreateSavedTabGroupTab("Two_Link", u"Second", id_2_)};
    std::vector<SavedTabGroupTab> group_3_tabs = {
        CreateSavedTabGroupTab("Athos", u"All For One", id_3_),
        CreateSavedTabGroupTab("Porthos", u"And", id_3_),
        CreateSavedTabGroupTab("Aramis", u"One For All", id_3_)};

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
    std::vector<base::GUID> saved_tab_group_ids;
    for (const SavedTabGroup& saved_group :
         saved_tab_group_model_->saved_tab_groups()) {
      saved_tab_group_ids.emplace_back(saved_group.saved_guid());
    }

    for (const auto& id : saved_tab_group_ids) {
      saved_tab_group_model_->Remove(id);
    }
  }

  SavedTabGroupTab CreateSavedTabGroupTab(const std::string& url,
                                          const std::u16string& title,
                                          const base::GUID& group_guid) {
    SavedTabGroupTab tab(GURL(base_path_ + url), group_guid);
    tab.SetTitle(title).SetFavicon(gfx::Image());
    return tab;
  }

  SavedTabGroup CreateSavedTabGroup(
      const std::u16string& group_title,
      const tab_groups::TabGroupColorId& color,
      const std::vector<SavedTabGroupTab>& group_tabs,
      const base::GUID& id) {
    return SavedTabGroup(group_title, color, group_tabs, id);
  }

  void CompareSavedTabGroupTabs(const std::vector<SavedTabGroupTab>& v1,
                                const std::vector<SavedTabGroupTab>& v2) {
    EXPECT_EQ(v1.size(), v2.size());
    for (size_t i = 0; i < v1.size(); i++) {
      const SavedTabGroupTab& tab1 = v1[i];
      const SavedTabGroupTab& tab2 = v2[i];
      EXPECT_EQ(tab1.url(), tab2.url());
      EXPECT_EQ(tab1.title(), tab2.title());
      EXPECT_EQ(tab1.favicon(), tab2.favicon());
    }
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::string base_path_ = "file:///c:/tmp/";
  base::GUID id_1_;
  base::GUID id_2_;
  base::GUID id_3_;
};

// Tests that SavedTabGroupModel::Count holds 3 elements initially.
TEST_F(SavedTabGroupModelTest, InitialCountThree) {
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);
  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(),
            static_cast<unsigned long>(3));
}

// Tests that SavedTabGroupModel::Contains returns the 3, the number of starting
// ids added to the model.
TEST_F(SavedTabGroupModelTest, InitialGroupsAreSaved) {
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_1_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_2_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_3_));
  EXPECT_FALSE(saved_tab_group_model_->Contains(GenerateNextGUID()));
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
  AddTestData();
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);
}

// Tests that SavedTabGroupModel::Add adds an extra element into the model and
// keeps the data.
TEST_F(SavedTabGroupModelTest, AddNewElement) {
  base::GUID id_4 = GenerateNextGUID();
  const std::u16string title_4 = u"Test Test";
  const tab_groups::TabGroupColorId& color_4 =
      tab_groups::TabGroupColorId::kBlue;

  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_4);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_4);

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

  // Update update time
  base::Time time = base::Time::Now();
  saved_tab_group_model_->Get(id_1_)->SetUpdateTimeWindowsEpochMicros(time);
  EXPECT_EQ(
      time,
      saved_tab_group_model_->Get(id_1_)->update_time_windows_epoch_micros());
}

// Tests that the correct tabs are added to the correct position in group 1.
TEST_F(SavedTabGroupModelTest, AddTabToGroup) {
  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_1_);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_1_);

  SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1, 0);
  EXPECT_EQ(group->saved_tabs().size(), size_t(2));
  EXPECT_EQ(0, group->GetIndexOfTab(tab1.guid()));
  EXPECT_TRUE(group->ContainsTab(tab1.guid()));
  ASSERT_TRUE(group->GetTab(tab1.guid()).has_value());
  CompareSavedTabGroupTabs({group->GetTab(tab1.guid()).value()}, {tab1});

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2, 2);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));
  EXPECT_EQ(2, group->GetIndexOfTab(tab2.guid()));
  EXPECT_TRUE(group->ContainsTab(tab2.guid()));
  ASSERT_TRUE(group->GetTab(tab2.guid()).has_value());
  CompareSavedTabGroupTabs({group->GetTab(tab2.guid()).value()}, {tab2});
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {tab1, group->saved_tabs()[1], tab2});
}

// Tests that the correct tabs are removed from the correct position in group 1.
TEST_F(SavedTabGroupModelTest, RemoveTabFromGroup) {
  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_1_);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_1_);

  SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1, 0);
  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2, 2);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->RemoveTabFromGroup(group->saved_guid(), tab1.guid());
  EXPECT_EQ(group->saved_tabs().size(), size_t(2));
  CompareSavedTabGroupTabs(group->saved_tabs(), {group->saved_tabs()[0], tab2});

  saved_tab_group_model_->RemoveTabFromGroup(group->saved_guid(), tab2.guid());
  EXPECT_EQ(group->saved_tabs().size(), size_t(1));
  CompareSavedTabGroupTabs(group->saved_tabs(), {group->saved_tabs()[0]});
}

// Tests that the correct tabs are replaced in group 1.
TEST_F(SavedTabGroupModelTest, ReplaceTabInGroup) {
  SavedTabGroupTab tab1 = CreateSavedTabGroupTab("first", u"First Tab", id_1_);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("second", u"Second Tab", id_1_);
  SavedTabGroupTab tab3 = CreateSavedTabGroupTab("third", u"Third Tab", id_1_);

  SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1, 0);
  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2, 2);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->ReplaceTabInGroupAt(group->saved_guid(), tab1.guid(),
                                              tab3);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {tab3, group->saved_tabs()[1], tab2});

  saved_tab_group_model_->ReplaceTabInGroupAt(group->saved_guid(), tab2.guid(),
                                              tab1);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {tab3, group->saved_tabs()[1], tab1});

  saved_tab_group_model_->ReplaceTabInGroupAt(
      group->saved_guid(), group->saved_tabs()[1].guid(), tab2);
  CompareSavedTabGroupTabs(group->saved_tabs(), {tab3, tab2, tab1});
}

// Tests that the correct tabs are moved in group 1.
TEST_F(SavedTabGroupModelTest, MoveTabInGroup) {
  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group", id_1_);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group", id_1_);

  SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab1, 0);
  saved_tab_group_model_->AddTabToGroup(group->saved_guid(), tab2, 2);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->MoveTabInGroupTo(group->saved_guid(), tab1.guid(), 2);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {group->saved_tabs()[0], tab2, tab1});

  saved_tab_group_model_->MoveTabInGroupTo(group->saved_guid(), tab1.guid(), 1);
  CompareSavedTabGroupTabs(group->saved_tabs(),
                           {group->saved_tabs()[0], tab1, tab2});
}

TEST_F(SavedTabGroupModelTest, MoveElement) {
  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
  saved_tab_group_model_->Reorder(id_2_, 2);
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_2_));
  saved_tab_group_model_->Reorder(id_2_, 0);
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_2_));
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
  // TODO(dljames): Use CompareSavedTabGroupTabs when we can ensure the order of
  // tabs and groups are maintained.
  EXPECT_EQ(saved_group->saved_tabs().size(), group->saved_tabs().size());
}

// Tests that merging a group with the same group_id changes the state of the
// object correctly.
TEST_F(SavedTabGroupModelTest, MergeGroupsFromModel) {
  SavedTabGroup* group1 = saved_tab_group_model_->Get(id_1_);
  SavedTabGroup group2 = SavedTabGroup::FromSpecifics(*group1->ToSpecifics());
  group2.SetColor(tab_groups::TabGroupColorId::kPink);
  group2.SetTitle(u"Updated title");
  SavedTabGroup merged_group = SavedTabGroup::FromSpecifics(
      *saved_tab_group_model_->MergeGroup(group2.ToSpecifics()));

  EXPECT_EQ(group1->title(), group2.title());
  EXPECT_EQ(group1->color(), group2.color());
  EXPECT_EQ(group1->saved_guid(), group2.saved_guid());
  EXPECT_EQ(group1->creation_time_windows_epoch_micros(),
            group2.creation_time_windows_epoch_micros());
  EXPECT_EQ(group1->update_time_windows_epoch_micros(),
            group2.update_time_windows_epoch_micros());
}

// Tests that merging a tab with the same tab_id changes the state of the object
// correctly.
TEST_F(SavedTabGroupModelTest, MergeTabsFromModel) {
  SavedTabGroupTab tab1 = saved_tab_group_model_->Get(id_1_)->saved_tabs()[0];
  SavedTabGroupTab tab2 = SavedTabGroupTab::FromSpecifics(*tab1.ToSpecifics());
  tab2.SetTitle(u"Updated Title");
  tab2.SetURL(GURL("chrome://updated_url"));

  SavedTabGroupTab merged_tab = SavedTabGroupTab::FromSpecifics(
      *saved_tab_group_model_->MergeTab(tab2.ToSpecifics()));

  EXPECT_EQ(tab1.url(), merged_tab.url());
  EXPECT_EQ(tab1.guid(), merged_tab.guid());
  EXPECT_EQ(tab1.group_guid(), merged_tab.group_guid());
  EXPECT_EQ(tab1.creation_time_windows_epoch_micros(),
            merged_tab.creation_time_windows_epoch_micros());
  EXPECT_EQ(tab1.update_time_windows_epoch_micros(),
            merged_tab.update_time_windows_epoch_micros());
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
                      GenerateNextGUID());
  SavedTabGroup stg_2(std::u16string(u"stg_2"),
                      tab_groups::TabGroupColorId::kGrey, {},
                      GenerateNextGUID());
  SavedTabGroup stg_3(std::u16string(u"stg_3"),
                      tab_groups::TabGroupColorId::kGrey, {},
                      GenerateNextGUID());

  saved_tab_group_model_->Add(stg_1);
  saved_tab_group_model_->Add(stg_2);
  saved_tab_group_model_->Add(stg_3);

  saved_tab_group_model_->Reorder(stg_2.saved_guid(), 2);

  EXPECT_TRUE(reordered_called_);
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(stg_2.saved_guid()));
}
