// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_observer.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

  void SavedTabGroupMoved(const SavedTabGroup& group) override {
    retrieved_group_.emplace_back(group);
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::vector<SavedTabGroup> retrieved_group_;
  int retrieved_index_ = -1;
  std::string base_path_ = "file:///c:/tmp/";
};

// Serves to test the functions in SavedTabGroupModel.
class SavedTabGroupModelTest : public ::testing::Test {
 protected:
  SavedTabGroupModelTest()
      : id_1_(tab_groups::TabGroupId::GenerateNew()),
        id_2_(tab_groups::TabGroupId::GenerateNew()),
        id_3_(tab_groups::TabGroupId::GenerateNew()) {}

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

    std::vector<GURL> urls_1;
    std::vector<GURL> urls_2;
    std::vector<GURL> urls_3;
    urls_1.emplace_back(GURL(base_path_ + "A_Link"));
    urls_2.emplace_back(GURL(base_path_ + "One_Link"));
    urls_2.emplace_back(GURL(base_path_ + "Two_Link"));
    urls_3.emplace_back(GURL(base_path_ + "Athos"));
    urls_3.emplace_back(GURL(base_path_ + "Porthos"));
    urls_3.emplace_back(GURL(base_path_ + "Aramis"));

    SavedTabGroup group_1(id_1_, title_1, color_1, urls_1);
    SavedTabGroup group_2(id_2_, title_2, color_2, urls_2);
    SavedTabGroup group_3(id_3_, title_3, color_3, urls_3);

    saved_tab_group_model_->Add(group_1);
    saved_tab_group_model_->Add(group_2);
    saved_tab_group_model_->Add(group_3);
  }

  void RemoveTestData() {
    if (!saved_tab_group_model_)
      return;
    // Copy ids so we do not remove elements while we are accessing the data.
    std::vector<tab_groups::TabGroupId> saved_tab_group_ids;
    for (const SavedTabGroup& saved_group :
         saved_tab_group_model_->saved_tab_groups()) {
      saved_tab_group_ids.emplace_back(saved_group.group_id);
    }

    for (const auto& id : saved_tab_group_ids) {
      saved_tab_group_model_->Remove(id);
    }
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::string base_path_ = "file:///c:/tmp/";
  tab_groups::TabGroupId id_1_;
  tab_groups::TabGroupId id_2_;
  tab_groups::TabGroupId id_3_;
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
  EXPECT_FALSE(
      saved_tab_group_model_->Contains(tab_groups::TabGroupId::GenerateNew()));
}

// Tests that the SavedTabGroupModel::GetIndexOf preserves the order the
// SavedTabGroups were inserted into.
TEST_F(SavedTabGroupModelTest, InitialOrderAdded) {
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_1_), 0);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_3_), 2);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_2_), 1);
  EXPECT_EQ(
      saved_tab_group_model_->GetIndexOf(tab_groups::TabGroupId::GenerateNew()),
      -1);
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
  tab_groups::TabGroupId id_4 = tab_groups::TabGroupId::GenerateNew();
  const std::u16string title_4 = u"Test Test";
  const tab_groups::TabGroupColorId& color_4 =
      tab_groups::TabGroupColorId::kBlue;
  std::vector<GURL> urls_4;
  urls_4.emplace_back(GURL(base_path_ + "4th group"));
  urls_4.emplace_back(GURL(base_path_ + "2nd link"));

  SavedTabGroup group_4(id_4, title_4, color_4, urls_4);
  saved_tab_group_model_->Add(group_4);

  EXPECT_TRUE(saved_tab_group_model_->Contains(id_4));
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_4), 3);
  EXPECT_EQ(saved_tab_group_model_->Count(), 4);

  const SavedTabGroup* saved_group = saved_tab_group_model_->Get(id_4);
  EXPECT_EQ(saved_group->group_id, id_4);
  EXPECT_EQ(saved_group->title, title_4);
  EXPECT_EQ(saved_group->color, color_4);
}

// Tests that SavedTabGroupModel::Update updates the correct element if the
// title or color are different.
TEST_F(SavedTabGroupModelTest, UpdateElement) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  const std::u16string original_title = group->title;
  const tab_groups::TabGroupColorId& original_color = group->color;

  // Should only update the element if title or color are different
  const std::u16string same_title = u"Group One";
  const tab_groups::TabGroupColorId& same_color =
      tab_groups::TabGroupColorId::kGrey;
  const tab_groups::TabGroupVisualData same_visual_data(same_title, same_color,
                                                        /*is_collapsed*/ false);
  saved_tab_group_model_->Update(id_1_, &same_visual_data);
  EXPECT_EQ(group->title, original_title);
  EXPECT_EQ(group->color, original_color);

  // Updates both color and title
  const std::u16string new_title = u"New Title";
  const tab_groups::TabGroupColorId& new_color =
      tab_groups::TabGroupColorId::kCyan;
  const tab_groups::TabGroupVisualData new_visual_data(new_title, new_color,
                                                       /*is_collapsed*/ false);
  saved_tab_group_model_->Update(id_1_, &new_visual_data);
  EXPECT_EQ(group->title, new_title);
  EXPECT_EQ(group->color, new_color);

  // Update only title
  const std::u16string random_title = u"Random Title";
  const tab_groups::TabGroupVisualData change_title_visual_data(
      random_title, original_color, /*is_collapsed*/ false);
  saved_tab_group_model_->Update(id_1_, &change_title_visual_data);
  EXPECT_EQ(group->title, random_title);
  EXPECT_EQ(group->color, original_color);

  // Update only color
  const tab_groups::TabGroupColorId& random_color =
      tab_groups::TabGroupColorId::kGrey;
  const tab_groups::TabGroupVisualData change_color_visual_data(
      original_title, random_color, /*is_collapsed*/ false);
  saved_tab_group_model_->Update(id_1_, &change_color_visual_data);
  EXPECT_EQ(group->title, original_title);
  EXPECT_EQ(group->color, random_color);
}

// Tests that SavedTabGroupModelObserver::Added passes the correct element from
// the model.
TEST_F(SavedTabGroupModelObserverTest, AddElement) {
  tab_groups::TabGroupId id_4 = tab_groups::TabGroupId::GenerateNew();
  const std::u16string title_4 = u"Test Test";
  const tab_groups::TabGroupColorId& color_4 =
      tab_groups::TabGroupColorId::kBlue;
  std::vector<GURL> urls_4;
  urls_4.emplace_back(GURL(base_path_ + "4th group"));
  urls_4.emplace_back(GURL(base_path_ + "2nd link"));

  SavedTabGroup group_4(id_4, title_4, color_4, urls_4);
  saved_tab_group_model_->Add(group_4);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.group_id, received_group.group_id);
  EXPECT_EQ(group_4.title, received_group.title);
  EXPECT_EQ(group_4.color, received_group.color);
  EXPECT_EQ(group_4.urls, received_group.urls);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.group_id),
            retrieved_index_);
}

// Tests that SavedTabGroupModelObserver::Removed passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, RemovedElement) {
  tab_groups::TabGroupId id_4 = tab_groups::TabGroupId::GenerateNew();
  const std::u16string title_4 = u"Test Test";
  const tab_groups::TabGroupColorId& color_4 =
      tab_groups::TabGroupColorId::kBlue;
  std::vector<GURL> urls_4;
  urls_4.emplace_back(GURL(base_path_ + "4th group"));
  urls_4.emplace_back(GURL(base_path_ + "2nd link"));

  SavedTabGroup group_4(id_4, title_4, color_4, urls_4);
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Remove(id_4);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.group_id, received_group.group_id);
  EXPECT_EQ(group_4.title, received_group.title);
  EXPECT_EQ(group_4.color, received_group.color);
  EXPECT_EQ(group_4.urls, received_group.urls);

  // The model will removed an and send the index that element was at before it
  // was removed. Because the only element in the model exists, we get -1.
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.group_id), -1);
  EXPECT_EQ(retrieved_index_, 0);
}

// Tests that SavedTabGroupModelObserver::Updated passes the correct
// element from the model.
TEST_F(SavedTabGroupModelObserverTest, UpdatedElement) {
  tab_groups::TabGroupId id_4 = tab_groups::TabGroupId::GenerateNew();
  const std::u16string title_4 = u"Test Test";
  const tab_groups::TabGroupColorId& color_4 =
      tab_groups::TabGroupColorId::kBlue;
  std::vector<GURL> urls_4;
  urls_4.emplace_back(GURL(base_path_ + "4th group"));
  urls_4.emplace_back(GURL(base_path_ + "2nd link"));

  SavedTabGroup group_4(id_4, title_4, color_4, urls_4);
  saved_tab_group_model_->Add(group_4);

  const std::u16string new_title = u"New Title";
  const tab_groups::TabGroupColorId& new_color =
      tab_groups::TabGroupColorId::kBlue;

  const tab_groups::TabGroupVisualData new_visual_data(new_title, new_color,
                                                       /*is_collapsed*/ false);
  saved_tab_group_model_->Update(id_4, &new_visual_data);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(id_4, received_group.group_id);
  EXPECT_EQ(new_title, received_group.title);
  EXPECT_EQ(new_color, received_group.color);
  EXPECT_EQ(group_4.urls, received_group.urls);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.group_id),
            retrieved_index_);
}
