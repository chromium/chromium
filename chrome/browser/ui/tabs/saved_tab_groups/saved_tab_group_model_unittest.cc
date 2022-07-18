// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  void SavedTabGroupAdded(const SavedTabGroup& group, int index) override {
    retrieved_group_.emplace_back(group);
    retrieved_index_ = index;
  }

  void SavedTabGroupRemoved(int index) override { retrieved_index_ = index; }

  void SavedTabGroupUpdated(const SavedTabGroup& group, int index) override {
    retrieved_group_.emplace_back(group);
    retrieved_index_ = index;
  }

  void SavedTabGroupMoved(const SavedTabGroup& group,
                          int old_index,
                          int new_index) override {
    retrieved_group_.emplace_back(group);
    retrieved_old_index_ = old_index;
    retrieved_new_index_ = new_index;
  }

  void ClearSignals() {
    retrieved_group_.clear();
    retrieved_index_ = -1;
    retrieved_old_index_ = -1;
    retrieved_new_index_ = -1;
  }

  SavedTabGroupTab CreateSavedTabGroupTab(const std::string& url,
                                          const std::u16string& title) {
    return SavedTabGroupTab(GURL(base_path_ + url), title,
                            favicon::GetDefaultFavicon());
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
      EXPECT_EQ(tab1.url, tab2.url);
      EXPECT_EQ(tab1.tab_title, tab2.tab_title);
      EXPECT_EQ(tab1.favicon, tab2.favicon);
    }
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::vector<SavedTabGroup> retrieved_group_;
  int retrieved_index_ = -1;
  int retrieved_old_index_ = -1;
  int retrieved_new_index_ = -1;
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
        CreateSavedTabGroupTab("A_Link", u"Only Tab")};
    std::vector<SavedTabGroupTab> group_2_tabs = {
        CreateSavedTabGroupTab("One_Link", u"One Of Two"),
        CreateSavedTabGroupTab("Two_Link", u"Second")};
    std::vector<SavedTabGroupTab> group_3_tabs = {
        CreateSavedTabGroupTab("Athos", u"All For One"),
        CreateSavedTabGroupTab("Porthos", u"And"),
        CreateSavedTabGroupTab("Aramis", u"One For All")};

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
                                          const std::u16string& title) {
    return SavedTabGroupTab(GURL(base_path_ + url), title,
                            favicon::GetDefaultFavicon());
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
      EXPECT_EQ(tab1.url, tab2.url);
      EXPECT_EQ(tab1.tab_title, tab2.tab_title);
      EXPECT_EQ(tab1.favicon, tab2.favicon);
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
      CreateSavedTabGroupTab("4th group", u"First Tab 4th Group");
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("2nd link", u"Second Tab 4th Group");

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

TEST_F(SavedTabGroupModelTest, MoveElement) {
  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
  saved_tab_group_model_->Reorder(id_2_, 2);
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_2_));
  saved_tab_group_model_->Reorder(id_2_, 0);
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_2_));
}

// Tests that SavedTabGroupModelObserver::Added passes the correct element from
// the model.
TEST_F(SavedTabGroupModelObserverTest, AddElement) {
  SavedTabGroup group_4(CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.tab_group_id(), received_group.tab_group_id());
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

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.tab_group_id(), received_group.tab_group_id());
  EXPECT_EQ(group_4.title(), received_group.title());
  EXPECT_EQ(group_4.color(), received_group.color());
  CompareSavedTabGroupTabs(group_4.saved_tabs(), received_group.saved_tabs());

  // The model will have already removed and sent the index our element was at
  // before it was removed from the model. As such, we should get -1 when
  // checking the model and 0 for the retrieved index.
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            -1);
  EXPECT_EQ(retrieved_index_, 0);
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
  EXPECT_EQ(group_4.tab_group_id(), received_group.tab_group_id());
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
  const int index = saved_tab_group_model_->GetIndexOf(group_4.saved_guid());
  ASSERT_GE(index, 0);

  // Expect the saved group that calls update is the one that was removed from
  // the tabstrip.
  saved_tab_group_model_->OnGroupClosedInTabStrip(
      group_4.tab_group_id().value());
  EXPECT_EQ(index, retrieved_index_);

  // Expect the removal of group_4 from the tabstrip makes GetIndexOf not return
  // a valid index when searched by tab group id, but does return the right
  // index when searched by saved guid.
  saved_tab_group_model_->OnGroupClosedInTabStrip(tab_group_id);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(tab_group_id), -1);
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

  ClearSignals();

  saved_tab_group_model_->Reorder(stg_2.saved_guid(), 2);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(stg_2.saved_guid(), received_group.saved_guid());
  EXPECT_EQ(1, retrieved_old_index_);
  EXPECT_EQ(2, retrieved_new_index_);
}
