// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/saved_tab_group_model.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace tab_groups {

namespace {

using testing::IsEmpty;
using testing::Not;
using testing::NotNull;
using testing::Pointee;
using testing::UnorderedElementsAre;

MATCHER_P(HasGroupId, guid, "") {
  return arg.saved_guid() == guid;
}

// Serves to test the functions in SavedTabGroupModelObserver.
class SavedTabGroupModelObserverTest
    : public ::testing::Test,
      public SavedTabGroupModelObserver,
      public ::testing::WithParamInterface<bool> {
 protected:
  SavedTabGroupModelObserverTest() {
    if (IsV2UIEnabled()) {
      feature_list_.InitWithFeatures({tab_groups::kTabGroupsSaveUIUpdate}, {});
    } else {
      feature_list_.InitWithFeatures({}, {tab_groups::kTabGroupsSaveUIUpdate});
    }
  }
  ~SavedTabGroupModelObserverTest() override = default;

  bool IsV2UIEnabled() const { return GetParam(); }

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
      const SavedTabGroup& removed_group) override {
    retrieved_guid_ = removed_group.saved_guid();
  }

  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(group_guid));
    retrieved_index_ =
        saved_tab_group_model_->GetIndexOf(group_guid).value_or(-1);
  }

  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(guid));
    retrieved_index_ = saved_tab_group_model_->GetIndexOf(guid).value_or(-1);
  }

  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup& removed_group) override {
    retrieved_guid_ = removed_group.saved_guid();
  }

  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override {
    retrieved_group_.emplace_back(*saved_tab_group_model_->Get(group_guid));
    retrieved_index_ =
        saved_tab_group_model_->GetIndexOf(group_guid).value_or(-1);
  }

  void SavedTabGroupReorderedLocally() override { reordered_called_ = true; }
  void SavedTabGroupTabMovedLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid) override {
    tabs_reodered_called_ = true;
  }

  void ClearSignals() {
    retrieved_group_.clear();
    retrieved_index_ = -1;
    retrieved_old_index_ = -1;
    retrieved_new_index_ = -1;
    reordered_called_ = false;
    tabs_reodered_called_ = false;
    retrieved_guid_ = base::Uuid::GenerateRandomV4();
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::vector<SavedTabGroup> retrieved_group_;
  int retrieved_index_ = -1;
  int retrieved_old_index_ = -1;
  int retrieved_new_index_ = -1;
  bool reordered_called_ = false;
  bool tabs_reodered_called_ = false;

  base::Uuid retrieved_guid_ = base::Uuid::GenerateRandomV4();
  std::string base_path_ = "file:///c:/tmp/";

  base::test::ScopedFeatureList feature_list_;
};

// Serves to test the functions in SavedTabGroupModel.
class SavedTabGroupModelTest : public ::testing::Test,
                               public ::testing::WithParamInterface<bool> {
 protected:
  SavedTabGroupModelTest()
      : id_1_(base::Uuid::GenerateRandomV4()),
        id_2_(base::Uuid::GenerateRandomV4()),
        id_3_(base::Uuid::GenerateRandomV4()) {
    if (IsV2UIEnabled()) {
      feature_list_.InitWithFeatures({tab_groups::kTabGroupsSaveUIUpdate}, {});
    } else {
      feature_list_.InitWithFeatures({}, {tab_groups::kTabGroupsSaveUIUpdate});
    }
  }

  ~SavedTabGroupModelTest() override { RemoveTestData(); }

  bool IsV2UIEnabled() const { return GetParam(); }

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

    std::vector<SavedTabGroupTab> group_1_tabs = {test::CreateSavedTabGroupTab(
        "A_Link", u"Only Tab", id_1_, /*position=*/0)};
    std::vector<SavedTabGroupTab> group_2_tabs = {
        test::CreateSavedTabGroupTab("One_Link", u"One Of Two", id_2_,
                                     /*position=*/0),
        test::CreateSavedTabGroupTab("Two_Link", u"Second", id_2_,
                                     /*position=*/1)};
    std::vector<SavedTabGroupTab> group_3_tabs = {
        test::CreateSavedTabGroupTab("Athos", u"All For One", id_3_,
                                     /*position=*/0),
        test::CreateSavedTabGroupTab("Porthos", u"And", id_3_, /*position=*/1),
        test::CreateSavedTabGroupTab("Aramis", u"One For All", id_3_,
                                     /*position=*/2)};

    saved_tab_group_model_->Add(
        SavedTabGroup(title_1, color_1, group_1_tabs, std::nullopt, id_1_));
    saved_tab_group_model_->Add(
        SavedTabGroup(title_2, color_2, group_2_tabs, std::nullopt, id_2_));
    saved_tab_group_model_->Add(
        SavedTabGroup(title_3, color_3, group_3_tabs, std::nullopt, id_3_));
  }

  void RemoveTestData() {
    if (!saved_tab_group_model_) {
      return;
    }
    // Copy ids so we do not remove elements while we are accessing the data.
    std::vector<base::Uuid> saved_tab_group_ids = GetSavedTabGroupIds();
    for (const auto& id : saved_tab_group_ids) {
      saved_tab_group_model_->Remove(id);
    }
  }

  std::vector<base::Uuid> GetSavedTabGroupIds() {
    std::vector<base::Uuid> saved_tab_group_ids;
    for (const SavedTabGroup& saved_group :
         saved_tab_group_model_->saved_tab_groups()) {
      saved_tab_group_ids.emplace_back(saved_group.saved_guid());
    }
    return saved_tab_group_ids;
  }

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::string base_path_ = "file:///c:/tmp/";
  base::Uuid id_1_;
  base::Uuid id_2_;
  base::Uuid id_3_;

  base::test::ScopedFeatureList feature_list_;
};

// Tests that SavedTabGroupModel::Count holds 3 elements initially.
TEST_P(SavedTabGroupModelTest, InitialCountThree) {
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);
  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), 3u);
}

// Tests that SavedTabGroupModel::Contains returns the 3, the number of starting
// ids added to the model.
TEST_P(SavedTabGroupModelTest, InitialGroupsAreSaved) {
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_1_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_2_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_3_));
  EXPECT_FALSE(
      saved_tab_group_model_->Contains(base::Uuid::GenerateRandomV4()));
}

// Tests that the SavedTabGroupModel::GetIndexOf preserves the order the
// SavedTabGroups were inserted into.
TEST_P(SavedTabGroupModelTest, InitialOrderAdded) {
  if (IsV2UIEnabled()) {
    EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_1_), 2);
    EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_2_), 1);
    EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_3_), 0);
  } else {
    EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_1_), 0);
    EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_2_), 1);
    EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_3_), 2);
  }
}

// Tests that the SavedTabGroupModel::IsEmpty has elements and once all elements
// are removed is empty.
TEST_P(SavedTabGroupModelTest, ContainsNoElementsOnRemoval) {
  EXPECT_FALSE(saved_tab_group_model_->IsEmpty());
  RemoveTestData();
  EXPECT_TRUE(saved_tab_group_model_->IsEmpty());
}

// Tests that the SavedTabGroupModel::Remove removes the correct element given
// an id.
TEST_P(SavedTabGroupModelTest, RemovesCorrectElements) {
  saved_tab_group_model_->Remove(id_3_);
  EXPECT_FALSE(saved_tab_group_model_->Contains(id_3_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_2_));
  EXPECT_TRUE(saved_tab_group_model_->Contains(id_1_));
}

// Tests that the SavedTabGroupModel only adds unique TabGroupIds.
TEST_P(SavedTabGroupModelTest, OnlyAddUniqueElements) {
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);
  EXPECT_CHECK_DEATH(AddTestData());
}

// Tests that SavedTabGroupModel::Add adds an extra element into the model and
// keeps the data.
TEST_P(SavedTabGroupModelTest, AddNewElement) {
  base::Uuid id_4 = base::Uuid::GenerateRandomV4();
  const std::u16string title_4 = u"Test Test";
  const tab_groups::TabGroupColorId& color_4 =
      tab_groups::TabGroupColorId::kBlue;

  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "4th group", u"First Tab 4th Group", id_4, /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "2nd link", u"Second Tab 4th Group", id_4, /*position=*/1);

  std::vector<SavedTabGroupTab> group_4_tabs = {tab1, tab2};
  SavedTabGroup group_4(title_4, color_4, group_4_tabs, std::nullopt, id_4);
  saved_tab_group_model_->Add(group_4);

  EXPECT_TRUE(saved_tab_group_model_->Contains(id_4));
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_4), IsV2UIEnabled() ? 0 : 3);
  EXPECT_EQ(saved_tab_group_model_->Count(), 4);

  const SavedTabGroup* saved_group = saved_tab_group_model_->Get(id_4);
  EXPECT_EQ(saved_group->saved_guid(), id_4);
  EXPECT_EQ(saved_group->title(), title_4);
  EXPECT_EQ(saved_group->color(), color_4);
  test::CompareSavedTabGroupTabs(saved_group->saved_tabs(), group_4_tabs);
}

// Tests that SavedTabGroupModel::Update updates the correct element if the
// title or color are different.
TEST_P(SavedTabGroupModelTest, UpdateElement) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  const std::u16string original_title = group->title();
  const tab_groups::TabGroupColorId& original_color = group->color();
  saved_tab_group_model_->OnGroupOpenedInTabStrip(
      id_1_, test::GenerateRandomTabGroupID());

  // Should only update the element if title or color are different
  const std::u16string same_title = u"Group One";
  const tab_groups::TabGroupColorId& same_color =
      tab_groups::TabGroupColorId::kGrey;
  const tab_groups::TabGroupVisualData same_visual_data(same_title, same_color,
                                                        /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(group->local_group_id().value(),
                                           &same_visual_data);
  EXPECT_EQ(group->title(), original_title);
  EXPECT_EQ(group->color(), original_color);

  // Updates both color and title
  const std::u16string new_title = u"New Title";
  const tab_groups::TabGroupColorId& new_color =
      tab_groups::TabGroupColorId::kCyan;
  const tab_groups::TabGroupVisualData new_visual_data(new_title, new_color,
                                                       /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(group->local_group_id().value(),
                                           &new_visual_data);
  EXPECT_EQ(group->title(), new_title);
  EXPECT_EQ(group->color(), new_color);

  // Update only title
  const std::u16string random_title = u"Random Title";
  const tab_groups::TabGroupVisualData change_title_visual_data(
      random_title, original_color, /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(group->local_group_id().value(),
                                           &change_title_visual_data);
  EXPECT_EQ(group->title(), random_title);
  EXPECT_EQ(group->color(), original_color);

  // Update only color
  const tab_groups::TabGroupColorId& random_color =
      tab_groups::TabGroupColorId::kGrey;
  const tab_groups::TabGroupVisualData change_color_visual_data(
      original_title, random_color, /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(group->local_group_id().value(),
                                           &change_color_visual_data);
  EXPECT_EQ(group->title(), original_title);
  EXPECT_EQ(group->color(), random_color);
}

TEST_P(SavedTabGroupModelTest, MakeTabGroupShared) {
  // Use `id_3_` because it contains several tabs, to verify that tabs are
  // copied over correctly.
  const SavedTabGroup* group = saved_tab_group_model_->Get(id_3_);
  const LocalTabGroupID local_group_id = test::GenerateRandomTabGroupID();
  saved_tab_group_model_->OnGroupOpenedInTabStrip(id_3_, local_group_id);
  ASSERT_FALSE(group->is_shared_tab_group());

  // Prepare the fields which are not expected to be copied over to the shared
  // group, apart from the local tab ID.
  if (!group->position().has_value()) {
    saved_tab_group_model_->TogglePinState(group->saved_guid());
  }
  saved_tab_group_model_->UpdateLocalCacheGuid(/*old_cache_guid=*/std::nullopt,
                                               /*new_cache_guid=*/"cache_guid");
  saved_tab_group_model_->UpdateLastUpdaterCacheGuidForGroup(
      "updater_cache_guid", local_group_id, /*tab_id=*/std::nullopt);
  for (const SavedTabGroupTab& tab : group->saved_tabs()) {
    saved_tab_group_model_->UpdateLocalTabId(group->saved_guid(), tab,
                                             test::GenerateRandomTabID());
    saved_tab_group_model_->UpdateLastUpdaterCacheGuidForGroup(
        "updater_cache_guid", local_group_id, tab.local_tab_id());
  }
  saved_tab_group_model_->UpdateLastUserInteractionTimeLocally(local_group_id);

  ASSERT_NE(group->position(), std::nullopt);

  // Transition the saved tab group to a shared tab group, and excessively
  // verify the contents of the shared group.
  saved_tab_group_model_->MakeTabGroupShared(local_group_id, "collaboration");

  // The originating group should remain unchanged.
  ASSERT_EQ(group, saved_tab_group_model_->Get(id_3_));
  EXPECT_FALSE(group->is_shared_tab_group());
  EXPECT_NE(group->position(), std::nullopt);
  EXPECT_NE(group->creator_cache_guid(), std::nullopt);
  EXPECT_NE(group->last_updater_cache_guid(), std::nullopt);
  EXPECT_FALSE(group->last_user_interaction_time().is_null());

  const SavedTabGroup* shared_group =
      saved_tab_group_model_->Get(local_group_id);
  ASSERT_THAT(shared_group, NotNull());
  EXPECT_NE(shared_group->saved_guid(), group->saved_guid());
  EXPECT_TRUE(shared_group->saved_guid().is_valid());
  EXPECT_EQ(shared_group->collaboration_id(), "collaboration");
  EXPECT_EQ(shared_group->originating_saved_tab_group_guid(),
            group->saved_guid());

  // Verify that both groups have the same fields.
  EXPECT_EQ(shared_group->title(), group->title());
  EXPECT_EQ(shared_group->color(), group->color());

  // Verify that the shared group has updated fields.
  EXPECT_GT(shared_group->creation_time_windows_epoch_micros(),
            group->creation_time_windows_epoch_micros());
  EXPECT_GT(shared_group->update_time_windows_epoch_micros(),
            group->update_time_windows_epoch_micros());
  EXPECT_EQ(shared_group->creator_cache_guid(), std::nullopt);
  EXPECT_EQ(shared_group->last_updater_cache_guid(), std::nullopt);
  EXPECT_EQ(shared_group->position(), std::nullopt);
  EXPECT_TRUE(shared_group->last_user_interaction_time().is_null());

  // Verify group tabs, there should be at least two tabs in the group. Note
  // that the order is expected to remain the same.
  ASSERT_EQ(shared_group->saved_tabs().size(), group->saved_tabs().size());
  ASSERT_GE(shared_group->saved_tabs().size(), 2u);
  for (size_t i = 0; i < shared_group->saved_tabs().size(); ++i) {
    const SavedTabGroupTab& shared_tab = shared_group->saved_tabs()[i];
    const SavedTabGroupTab& saved_tab = group->saved_tabs()[i];

    // Verify the same fields.
    EXPECT_EQ(shared_tab.url(), saved_tab.url());
    EXPECT_EQ(shared_tab.title(), saved_tab.title());
    EXPECT_EQ(shared_tab.favicon(), saved_tab.favicon());
    EXPECT_EQ(shared_tab.saved_group_guid(), shared_group->saved_guid());

    // Verify updated fields.
    EXPECT_NE(shared_tab.saved_tab_guid(), saved_tab.saved_tab_guid());
    EXPECT_NE(shared_tab.local_tab_id(), std::nullopt);
    EXPECT_EQ(saved_tab.local_tab_id(), std::nullopt);
    EXPECT_EQ(shared_tab.creator_cache_guid(), std::nullopt);
    EXPECT_NE(saved_tab.creator_cache_guid(), std::nullopt);
    EXPECT_EQ(shared_tab.last_updater_cache_guid(), std::nullopt);
    EXPECT_NE(saved_tab.last_updater_cache_guid(), std::nullopt);
    EXPECT_GT(shared_group->creation_time_windows_epoch_micros(),
              saved_tab.creation_time_windows_epoch_micros());
    EXPECT_GT(shared_tab.update_time_windows_epoch_micros(),
              saved_tab.update_time_windows_epoch_micros());

    // Do not verify the position of the original tab because its meaning
    // differs for shared tab groups: it's the index of the tab in the shared
    // group.
    EXPECT_EQ(shared_tab.position(), i);
  }
}

// Tests that the correct tabs are added to the correct position in group 1.
TEST_P(SavedTabGroupModelTest, AddTabToGroup) {
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "4th group", u"First Tab 4th Group", id_1_, /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "2nd link", u"Second Tab 4th Group", id_1_, /*position=*/2);

  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroupLocally(group->saved_guid(), tab1);
  EXPECT_EQ(group->saved_tabs().size(), size_t(2));
  EXPECT_EQ(0, group->GetIndexOfTab(tab1.saved_tab_guid()));
  EXPECT_TRUE(group->ContainsTab(tab1.saved_tab_guid()));
  ASSERT_TRUE(group->GetTab(tab1.saved_tab_guid()));
  test::CompareSavedTabGroupTabs({*group->GetTab(tab1.saved_tab_guid())},
                                 {tab1});

  saved_tab_group_model_->AddTabToGroupLocally(group->saved_guid(), tab2);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));
  EXPECT_EQ(2, group->GetIndexOfTab(tab2.saved_tab_guid()));
  EXPECT_TRUE(group->ContainsTab(tab2.saved_tab_guid()));
  ASSERT_TRUE(group->GetTab(tab2.saved_tab_guid()));
  test::CompareSavedTabGroupTabs({*group->GetTab(tab2.saved_tab_guid())},
                                 {tab2});
  test::CompareSavedTabGroupTabs(group->saved_tabs(),
                                 {tab1, group->saved_tabs()[1], tab2});
}

// Tests that the correct tabs are removed from the correct position in group 1.
TEST_P(SavedTabGroupModelTest, RemoveTabFromGroup) {
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "4th group", u"First Tab 4th Group", id_1_, /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "2nd link", u"Second Tab 4th Group", id_1_, /*position=*/2);

  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroupLocally(group->saved_guid(), tab1);
  saved_tab_group_model_->AddTabToGroupLocally(group->saved_guid(), tab2);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->RemoveTabFromGroupLocally(group->saved_guid(),
                                                    tab1.saved_tab_guid());
  EXPECT_EQ(group->saved_tabs().size(), size_t(2));
  test::CompareSavedTabGroupTabs(group->saved_tabs(),
                                 {group->saved_tabs()[0], tab2});

  saved_tab_group_model_->RemoveTabFromGroupLocally(group->saved_guid(),
                                                    tab2.saved_tab_guid());
  EXPECT_EQ(group->saved_tabs().size(), size_t(1));
  test::CompareSavedTabGroupTabs(group->saved_tabs(), {group->saved_tabs()[0]});
}

// Tests that a group is removed from the model when the last tab is removed
// from it.
TEST_P(SavedTabGroupModelTest, RemoveLastTabFromGroup) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->RemoveTabFromGroupLocally(
      group->saved_guid(), group->saved_tabs()[0].saved_tab_guid());

  EXPECT_FALSE(saved_tab_group_model_->Contains(id_1_));
}

// Tests updating a tab in a saved group.
TEST_P(SavedTabGroupModelTest, UpdateTabInGroup) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  // Update the tab by changing the title.
  SavedTabGroupTab tab1 = group->saved_tabs()[0];
  tab1.SetTitle(u"Updated Title");
  saved_tab_group_model_->UpdateTabInGroup(id_1_, tab1);

  // The group should contain the updated tab.
  test::CompareSavedTabGroupTabs(group->saved_tabs(), {tab1});
}

// Tests that the correct tabs are moved in group 1.
TEST_P(SavedTabGroupModelTest, MoveTabInGroup) {
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "4th group", u"First Tab 4th Group", id_1_, /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "2nd link", u"Second Tab 4th Group", id_1_, /*position=*/2);

  const SavedTabGroup* group = saved_tab_group_model_->Get(id_1_);
  ASSERT_EQ(group->saved_tabs().size(), size_t(1));

  saved_tab_group_model_->AddTabToGroupLocally(group->saved_guid(), tab1);
  saved_tab_group_model_->AddTabToGroupLocally(group->saved_guid(), tab2);
  EXPECT_EQ(group->saved_tabs().size(), size_t(3));

  saved_tab_group_model_->MoveTabInGroupTo(group->saved_guid(),
                                           tab1.saved_tab_guid(), 2);
  test::CompareSavedTabGroupTabs(group->saved_tabs(),
                                 {group->saved_tabs()[0], tab2, tab1});

  saved_tab_group_model_->MoveTabInGroupTo(group->saved_guid(),
                                           tab1.saved_tab_guid(), 1);
  test::CompareSavedTabGroupTabs(group->saved_tabs(),
                                 {group->saved_tabs()[0], tab1, tab2});
}

TEST_P(SavedTabGroupModelTest, MoveElement) {
  if (IsV2UIEnabled()) {
    ASSERT_EQ(0, saved_tab_group_model_->GetIndexOf(id_3_));
    ASSERT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
    ASSERT_EQ(2, saved_tab_group_model_->GetIndexOf(id_1_));
    saved_tab_group_model_->ReorderGroupLocally(id_2_, 2);
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_3_));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_1_));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_2_));
    saved_tab_group_model_->ReorderGroupLocally(id_2_, 0);
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_2_));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_3_));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_1_));
    saved_tab_group_model_->ReorderGroupLocally(id_2_, 1);
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_3_));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_1_));
  } else {
    ASSERT_EQ(0, saved_tab_group_model_->GetIndexOf(id_1_));
    ASSERT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
    ASSERT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
    saved_tab_group_model_->ReorderGroupLocally(id_2_, 2);
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_1_));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_3_));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_2_));
    saved_tab_group_model_->ReorderGroupLocally(id_2_, 0);
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_2_));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_1_));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
    saved_tab_group_model_->ReorderGroupLocally(id_2_, 1);
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(id_1_));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(id_2_));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
  }
}

TEST_P(SavedTabGroupModelTest, ShouldDistinguishSavedAndSharedGroups) {
  const LocalTabGroupID local_group_id = test::GenerateRandomTabGroupID();
  saved_tab_group_model_->OnGroupOpenedInTabStrip(id_1_, local_group_id);
  saved_tab_group_model_->MakeTabGroupShared(local_group_id, "collaboration");

  const SavedTabGroup* shared_group =
      saved_tab_group_model_->Get(local_group_id);
  ASSERT_TRUE(shared_group->is_shared_tab_group());

  ASSERT_FALSE(saved_tab_group_model_->Get(id_1_)->is_shared_tab_group());
  ASSERT_FALSE(saved_tab_group_model_->Get(id_2_)->is_shared_tab_group());
  ASSERT_FALSE(saved_tab_group_model_->Get(id_3_)->is_shared_tab_group());

  EXPECT_THAT(saved_tab_group_model_->GetSavedTabGroupsOnly(),
              UnorderedElementsAre(Pointee(HasGroupId(id_1_)),
                                   Pointee(HasGroupId(id_2_)),
                                   Pointee(HasGroupId(id_3_))));
  EXPECT_THAT(
      saved_tab_group_model_->GetSharedTabGroupsOnly(),
      UnorderedElementsAre(Pointee(HasGroupId(shared_group->saved_guid()))));
}

TEST_P(SavedTabGroupModelTest, LoadStoredEntriesPopulatesModel) {
  std::unique_ptr<SavedTabGroup> group =
      std::make_unique<SavedTabGroup>(*saved_tab_group_model_->Get(id_3_));

  saved_tab_group_model_->Remove(id_3_);
  ASSERT_FALSE(saved_tab_group_model_->Contains(id_3_));

  saved_tab_group_model_->LoadStoredEntries({*group}, group->saved_tabs());

  EXPECT_TRUE(saved_tab_group_model_->Contains(id_3_));
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(id_3_), IsV2UIEnabled() ? 0 : 2);
  EXPECT_EQ(saved_tab_group_model_->Count(), 3);

  const SavedTabGroup* saved_group = saved_tab_group_model_->Get(id_3_);
  EXPECT_EQ(saved_group->saved_guid(), id_3_);
  EXPECT_EQ(saved_group->title(), group->title());
  EXPECT_EQ(saved_group->color(), group->color());

  // We can not use test::CompareSavedTabGroupTabs because the favicons are not
  // loaded until the tab is opened through the saved group button.
  EXPECT_EQ(saved_group->saved_tabs().size(), group->saved_tabs().size());
}

// Tests that merging a group with the same group_id changes the state of the
// object correctly.
TEST_P(SavedTabGroupModelTest, MergeGroupsFromModel) {
  const SavedTabGroup* group1 = saved_tab_group_model_->Get(id_1_);

  SavedTabGroup group2(*group1);
  group2.SetColor(tab_groups::TabGroupColorId::kPink);
  group2.SetTitle(u"Updated title");
  const SavedTabGroup* merged_group =
      saved_tab_group_model_->MergeRemoteGroupMetadata(
          group2.saved_guid(), group2.title(), group2.color(),
          group2.position(), group2.creator_cache_guid(),
          group2.last_updater_cache_guid(),
          group2.update_time_windows_epoch_micros());

  EXPECT_EQ(group2.title(), merged_group->title());
  EXPECT_EQ(group2.color(), merged_group->color());
  EXPECT_EQ(group2.saved_guid(), merged_group->saved_guid());
  EXPECT_EQ(group2.creation_time_windows_epoch_micros(),
            merged_group->creation_time_windows_epoch_micros());
  EXPECT_EQ(group2.update_time_windows_epoch_micros(),
            merged_group->update_time_windows_epoch_micros());
}

TEST_P(SavedTabGroupModelTest, MergePinnedGroupRetainPosition) {
  auto guid1 = base::Uuid::GenerateRandomV4();
  auto guid2 = base::Uuid::GenerateRandomV4();

  // Add group 1 at position 0.
  saved_tab_group_model_->Add(SavedTabGroup(
      u"Title 1", tab_groups::TabGroupColorId::kPink, {}, 0, guid1));

  // Add group 2 at position 0.
  saved_tab_group_model_->Add(SavedTabGroup(
      u"Title", tab_groups::TabGroupColorId::kPink, {}, 0, guid2));
  const SavedTabGroup* group2 = saved_tab_group_model_->Get(guid2);
  EXPECT_EQ(0, group2->position());

  // Verify group 2 should be the 1st one in the list.
  if (IsV2UIEnabled()) {
    ASSERT_THAT(GetSavedTabGroupIds(),
                testing::ElementsAre(guid2, guid1, id_3_, id_2_, id_1_));
  } else {
    ASSERT_THAT(GetSavedTabGroupIds(),
                testing::ElementsAre(guid2, guid1, id_1_, id_2_, id_3_));
  }

  // Change group 2 position from 0 to 1.
  SavedTabGroup updated_group2(*group2);
  EXPECT_EQ(0, updated_group2.position());
  updated_group2.SetPosition(1);
  EXPECT_EQ(1, updated_group2.position());

  // Merge the updated group 2 and verify the position is set to 1.
  const SavedTabGroup* merged_group =
      saved_tab_group_model_->MergeRemoteGroupMetadata(
          updated_group2.saved_guid(), updated_group2.title(),
          updated_group2.color(), updated_group2.position(),
          updated_group2.creator_cache_guid(),
          updated_group2.last_updater_cache_guid(),
          updated_group2.update_time_windows_epoch_micros());
  EXPECT_EQ(1, merged_group->position());

  // Verify group 2 should be the 2nd one in the list.
  if (IsV2UIEnabled()) {
    ASSERT_THAT(GetSavedTabGroupIds(),
                testing::ElementsAre(guid1, guid2, id_3_, id_2_, id_1_));
  } else {
    ASSERT_THAT(GetSavedTabGroupIds(),
                testing::ElementsAre(guid1, guid2, id_1_, id_2_, id_3_));
  }
}

TEST_P(SavedTabGroupModelTest, MergeUnpinnedGroupRetainUnpinned) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  auto guid1 = base::Uuid::GenerateRandomV4();
  auto guid2 = base::Uuid::GenerateRandomV4();

  // Add group 1 at position 0.
  saved_tab_group_model_->Add(SavedTabGroup(
      u"Title 1", tab_groups::TabGroupColorId::kPink, {}, 0, guid1));

  // Add group 2 at position 0.
  saved_tab_group_model_->Add(SavedTabGroup(
      u"Title", tab_groups::TabGroupColorId::kPink, {}, 0, guid2));
  const SavedTabGroup* group2 = saved_tab_group_model_->Get(guid2);
  EXPECT_EQ(0, group2->position());

  // Verify group 2 should be the 1st one in the list.
  ASSERT_THAT(GetSavedTabGroupIds(),
              testing::ElementsAre(guid2, guid1, id_3_, id_2_, id_1_));

  // Unpin group 2.
  SavedTabGroup updated_group2(*group2);
  EXPECT_EQ(0, updated_group2.position());
  updated_group2.SetPinned(false);
  EXPECT_EQ(std::nullopt, updated_group2.position());

  // Merge the updated group 2 and verify it's unpinned.
  const SavedTabGroup* merged_group =
      saved_tab_group_model_->MergeRemoteGroupMetadata(
          updated_group2.saved_guid(), updated_group2.title(),
          updated_group2.color(), updated_group2.position(),
          updated_group2.creator_cache_guid(),
          updated_group2.last_updater_cache_guid(),
          updated_group2.update_time_windows_epoch_micros());
  EXPECT_EQ(std::nullopt, merged_group->position());

  // Verify group 2 should place behind group 1.
  ASSERT_THAT(GetSavedTabGroupIds(),
              testing::ElementsAre(guid1, guid2, id_3_, id_2_, id_1_));
}

// Tests that merging a tab with the same tab_id changes the state of the object
// correctly.
TEST_P(SavedTabGroupModelTest, MergeTabsFromModel) {
  SavedTabGroupTab tab1 = saved_tab_group_model_->Get(id_1_)->saved_tabs()[0];
  SavedTabGroupTab tab2(tab1);
  tab2.SetTitle(u"Updated Title");
  tab2.SetURL(GURL("chrome://updated_url"));

  const SavedTabGroupTab* merged_tab =
      saved_tab_group_model_->MergeRemoteTab(tab2);

  EXPECT_EQ(tab2.url(), merged_tab->url());
  EXPECT_EQ(tab2.saved_tab_guid(), merged_tab->saved_tab_guid());
  EXPECT_EQ(tab2.saved_group_guid(), merged_tab->saved_group_guid());
  EXPECT_EQ(tab2.creation_time_windows_epoch_micros(),
            merged_tab->creation_time_windows_epoch_micros());
  EXPECT_EQ(tab2.update_time_windows_epoch_micros(),
            merged_tab->update_time_windows_epoch_micros());
}

// Tests that groups inserted in the model are in order stay inserted in sorted
// order.
TEST_P(SavedTabGroupModelTest, GroupsSortedWithInOrderPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups, with the positions the groups should
  // sit in the bookmarks bar.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {}, 0);
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {},
                        1);
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {},
                        2);
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {}, 3);
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {}, 4);
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {},
                        5);

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
    EXPECT_TRUE(test::CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model out of order are still inserted in
// sorted order.
TEST_P(SavedTabGroupModelTest, GroupsSortedWithOutOfOrderPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups, with the positions the groups should
  // sit in the bookmarks bar.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {}, 0);
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {},
                        1);
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {},
                        2);
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {}, 3);
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {}, 4);
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {},
                        5);

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
    EXPECT_TRUE(test::CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with gaps between the positions are
// still inserted in sorted order.
TEST_P(SavedTabGroupModelTest, GroupsSortedWithGapsInPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups, with the positions the groups should
  // sit in the bookmarks bar.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {}, 0);
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {},
                        3);
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {},
                        8);
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {},
                        19);
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {}, 21);
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {},
                        34);

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
    EXPECT_TRUE(test::CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with gaps and in decreasing order
// between the positions are still inserted in increasing sorted order.
TEST_P(SavedTabGroupModelTest, GroupsSortedWithDecreasingPositions) {
  RemoveTestData();

  // Create an arbitrary number of groups, with the positions the groups should
  // sit in the bookmarks bar.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {}, 0);
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {},
                        3);
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {},
                        8);
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {},
                        19);
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {}, 21);
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {},
                        34);

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
    EXPECT_TRUE(test::CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with a more recent update time take
// precedence over groups with the same position.
TEST_P(SavedTabGroupModelTest, GroupWithSamePositionSortedByUpdateTime) {
  RemoveTestData();

  // Create an arbitrary number of groups, with the positions the groups should
  // sit in the bookmarks bar.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {}, 0);
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {},
                        0);

  // This is the order we expect the groups in the model to be.
  std::vector<SavedTabGroup> groups = {group_2, group_1};

  // Add the groups into the model in an arbitrary order.
  saved_tab_group_model_->Add(group_1);
  saved_tab_group_model_->Add(group_2);

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(test::CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Tests that groups inserted in the model with no position are inserted at the
// back of the model and have their position set to the last index at the time
// they were inserted.
TEST_P(SavedTabGroupModelTest, GroupsWithNoPositionInsertedAtEnd) {
  RemoveTestData();

  // Create an arbitrary number of groups, with the positions the groups should
  // sit in the bookmarks bar.
  SavedTabGroup group_1(u"Group 1", tab_groups::TabGroupColorId::kRed, {}, 0);
  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kOrange, {},
                        1);
  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kYellow, {},
                        2);
  SavedTabGroup group_4(u"Group 4", tab_groups::TabGroupColorId::kGreen, {}, 3);
  SavedTabGroup group_5(u"Group 5", tab_groups::TabGroupColorId::kBlue, {}, 4);
  SavedTabGroup group_6(u"Group 6", tab_groups::TabGroupColorId::kPurple, {},
                        std::nullopt);

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

  if (!IsV2UIEnabled()) {
    groups[5].SetPosition(5);
  }

  EXPECT_EQ(saved_tab_group_model_->saved_tab_groups().size(), groups.size());

  // Expect the 6th group to have a position of 5 (0-based indexing).
  EXPECT_EQ(saved_tab_group_model_
                ->saved_tab_groups()
                    [saved_tab_group_model_->saved_tab_groups().size() - 1]
                .position(),
            groups[5].position());

  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_TRUE(test::CompareSavedTabGroups(
        groups[i], saved_tab_group_model_->saved_tab_groups()[i]));
  }
}

// Expect the pinned group is added to the front of the list.
TEST_P(SavedTabGroupModelTest, AddPinnedGroup) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(3u, saved_tab_group_model_->saved_tab_groups().size());
  SavedTabGroup group(u"Tab Group", tab_groups::TabGroupColorId::kRed, {},
                      std::nullopt);
  group.SetPinned(true);
  saved_tab_group_model_->Add(group);
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(group.saved_guid()));
  EXPECT_EQ(4u, saved_tab_group_model_->saved_tab_groups().size());
}

// Expect pin group to move the 2nd group to the front of the list.
TEST_P(SavedTabGroupModelTest, PinGroup) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(3u, saved_tab_group_model_->saved_tab_groups().size());
  SavedTabGroup group1(u"Tab Group 1", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  group1.SetPinned(true);
  saved_tab_group_model_->Add(group1);

  SavedTabGroup group2(u"Tab Group 2", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group2);

  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(group2.saved_guid()));
  EXPECT_EQ(5u, saved_tab_group_model_->saved_tab_groups().size());

  EXPECT_FALSE(
      saved_tab_group_model_->IsGroupPinned(group2.saved_guid()).value());
  saved_tab_group_model_->TogglePinState(group2.saved_guid());
  EXPECT_TRUE(
      saved_tab_group_model_->IsGroupPinned(group2.saved_guid()).value());

  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(group2.saved_guid()));
  EXPECT_EQ(5u, saved_tab_group_model_->saved_tab_groups().size());
}

// Expect unpin group to move the front group to the 2nd of the list.
TEST_P(SavedTabGroupModelTest, UnpinGroup) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(3u, saved_tab_group_model_->saved_tab_groups().size());
  SavedTabGroup group1(u"Tab Group 1", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  group1.SetPinned(true);
  saved_tab_group_model_->Add(group1);

  SavedTabGroup group2(u"Tab Group 2", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  group2.SetPinned(true);
  saved_tab_group_model_->Add(group2);

  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(group2.saved_guid()));
  EXPECT_EQ(5u, saved_tab_group_model_->saved_tab_groups().size());

  EXPECT_TRUE(
      saved_tab_group_model_->IsGroupPinned(group2.saved_guid()).value());
  saved_tab_group_model_->TogglePinState(group2.saved_guid());
  EXPECT_FALSE(
      saved_tab_group_model_->IsGroupPinned(group2.saved_guid()).value());

  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(group2.saved_guid()));
  EXPECT_EQ(5u, saved_tab_group_model_->saved_tab_groups().size());
}

TEST_P(SavedTabGroupModelTest, MigrateSavedTabGroup2FromV1) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  // Add 5 unpinned SavedTabGroups to the model.
  SavedTabGroup group4(u"Tab Group 4", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group4);

  SavedTabGroup group5(u"Tab Group 5", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group5);

  EXPECT_EQ(5u, saved_tab_group_model_->saved_tab_groups().size());

  // Verify orders of the added groups.
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(group5.saved_guid()));
  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(group4.saved_guid()));
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
  EXPECT_EQ(3, saved_tab_group_model_->GetIndexOf(id_2_));
  EXPECT_EQ(4, saved_tab_group_model_->GetIndexOf(id_1_));

  // Verify all groups are unpinned.
  EXPECT_EQ(false,
            saved_tab_group_model_->IsGroupPinned(group5.saved_guid()).value());
  EXPECT_EQ(false,
            saved_tab_group_model_->IsGroupPinned(group4.saved_guid()).value());
  EXPECT_EQ(false, saved_tab_group_model_->IsGroupPinned(id_3_).value());
  EXPECT_EQ(false, saved_tab_group_model_->IsGroupPinned(id_2_).value());
  EXPECT_EQ(false, saved_tab_group_model_->IsGroupPinned(id_1_).value());

  saved_tab_group_model_->MigrateTabGroupSavesUIUpdate();

  // Verify orders of the added groups don't change.
  EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(group5.saved_guid()));
  EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(group4.saved_guid()));
  EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(id_3_));
  EXPECT_EQ(3, saved_tab_group_model_->GetIndexOf(id_2_));
  EXPECT_EQ(4, saved_tab_group_model_->GetIndexOf(id_1_));

  // Verify the first 4 groups are pinned.
  EXPECT_EQ(true,
            saved_tab_group_model_->IsGroupPinned(group5.saved_guid()).value());
  EXPECT_EQ(true,
            saved_tab_group_model_->IsGroupPinned(group4.saved_guid()).value());
  EXPECT_EQ(true, saved_tab_group_model_->IsGroupPinned(id_3_).value());
  EXPECT_EQ(true, saved_tab_group_model_->IsGroupPinned(id_2_).value());
  EXPECT_EQ(false, saved_tab_group_model_->IsGroupPinned(id_1_).value());
}

// Tests that SavedTabGroupModelObserver::Added passes the correct element from
// the model.
TEST_P(SavedTabGroupModelObserverTest, AddElement) {
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(group_4.title(), received_group.title());
  EXPECT_EQ(group_4.color(), received_group.color());
  test::CompareSavedTabGroupTabs(group_4.saved_tabs(),
                                 received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Tests that SavedTabGroupModelObserver::Removed passes the correct
// element from the model.
TEST_P(SavedTabGroupModelObserverTest, RemovedElement) {
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->Remove(group_4.saved_guid());

  EXPECT_EQ(group_4.saved_guid(), retrieved_guid_);
  EXPECT_FALSE(saved_tab_group_model_->Contains(retrieved_guid_));

  // The model will have already removed and sent the index our element was at
  // before it was removed from the model. As such, we should get -1 when
  // checking the model and 0 for the retrieved index.
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(retrieved_guid_), std::nullopt);
}

// Tests that SavedTabGroupModelObserver::Updated passes the correct
// element from the model.
TEST_P(SavedTabGroupModelObserverTest, UpdatedElement) {
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  group_4.SetLocalGroupId(test::GenerateRandomTabGroupID());
  saved_tab_group_model_->Add(group_4);

  const std::u16string new_title = u"New Title";
  const tab_groups::TabGroupColorId& new_color =
      tab_groups::TabGroupColorId::kBlue;

  const tab_groups::TabGroupVisualData new_visual_data(new_title, new_color,
                                                       /*is_collapsed*/ false);
  saved_tab_group_model_->UpdateVisualData(group_4.local_group_id().value(),
                                           &new_visual_data);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(new_title, received_group.title());
  EXPECT_EQ(new_color, received_group.color());
  test::CompareSavedTabGroupTabs(group_4.saved_tabs(),
                                 received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Tests that SavedTabGroupModelObserver::AddedFromSync passes the correct
// element from the model.
TEST_P(SavedTabGroupModelObserverTest, AddElementFromSync) {
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  group_4.SetPosition(0);
  saved_tab_group_model_->AddedFromSync(group_4);

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_EQ(group_4.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(group_4.title(), received_group.title());
  EXPECT_EQ(group_4.color(), received_group.color());
  test::CompareSavedTabGroupTabs(group_4.saved_tabs(),
                                 received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Tests that SavedTabGroupModelObserver::RemovedFromSync passes the correct
// element from the model.
TEST_P(SavedTabGroupModelObserverTest, RemovedElementFromSync) {
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group_4);
  saved_tab_group_model_->RemovedFromSync(group_4.saved_guid());

  EXPECT_EQ(group_4.saved_guid(), retrieved_guid_);
  EXPECT_FALSE(saved_tab_group_model_->Contains(retrieved_guid_));

  // The model will have already removed and sent the index our element was at
  // before it was removed from the model. As such, we should get -1 when
  // checking the model and 0 for the retrieved index.
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(retrieved_guid_), std::nullopt);
}

// Tests that SavedTabGroupModelObserver::UpdatedFromSync passes the correct
// element from the model.
TEST_P(SavedTabGroupModelObserverTest, UpdatedElementFromSync) {
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
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
  test::CompareSavedTabGroupTabs(group_4.saved_tabs(),
                                 received_group.saved_tabs());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

// Verify that SavedTabGroupModel::OnGroupClosedInTabStrip passes the correct
// index.
TEST_P(SavedTabGroupModelObserverTest, OnGroupClosedInTabStrip) {
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
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
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(tab_group_id), std::nullopt);
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(group_4.saved_guid()), index);
}

// Tests that SavedTabGroupModelObserver::Moved passes the correct
// element from the model.
TEST_P(SavedTabGroupModelObserverTest, MoveElement) {
  SavedTabGroup stg_1(std::u16string(u"stg_1"),
                      tab_groups::TabGroupColorId::kGrey, {}, std::nullopt,
                      base::Uuid::GenerateRandomV4());
  SavedTabGroup stg_2(std::u16string(u"stg_2"),
                      tab_groups::TabGroupColorId::kGrey, {}, std::nullopt,
                      base::Uuid::GenerateRandomV4());
  SavedTabGroup stg_3(std::u16string(u"stg_3"),
                      tab_groups::TabGroupColorId::kGrey, {}, std::nullopt,
                      base::Uuid::GenerateRandomV4());

  saved_tab_group_model_->Add(stg_1);
  saved_tab_group_model_->Add(stg_2);
  saved_tab_group_model_->Add(stg_3);

  saved_tab_group_model_->ReorderGroupLocally(stg_2.saved_guid(), 2);

  EXPECT_TRUE(reordered_called_);
  if (IsV2UIEnabled()) {
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(stg_3.saved_guid()));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(stg_1.saved_guid()));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(stg_2.saved_guid()));
  } else {
    EXPECT_EQ(0, saved_tab_group_model_->GetIndexOf(stg_1.saved_guid()));
    EXPECT_EQ(1, saved_tab_group_model_->GetIndexOf(stg_3.saved_guid()));
    EXPECT_EQ(2, saved_tab_group_model_->GetIndexOf(stg_2.saved_guid()));
  }
}

TEST_P(SavedTabGroupModelObserverTest, ReordedTabsUpdatePositions) {
  SavedTabGroup group = test::CreateTestSavedTabGroup();
  base::Uuid group_id = group.saved_guid();
  base::Uuid tab1_id = group.saved_tabs()[0].saved_tab_guid();
  base::Uuid tab2_id = group.saved_tabs()[1].saved_tab_guid();
  saved_tab_group_model_->Add(group);

  // Move the first tab to the second position.
  saved_tab_group_model_->MoveTabInGroupTo(group_id, tab1_id, 1);

  EXPECT_TRUE(tabs_reodered_called_);
  EXPECT_EQ(0, saved_tab_group_model_->Get(group_id)->GetIndexOfTab(tab2_id));
  EXPECT_EQ(1, saved_tab_group_model_->Get(group_id)->GetIndexOfTab(tab1_id));
}

TEST_P(SavedTabGroupModelObserverTest, GetGroupContainingTab) {
  // Add a non matching SavedTabGroup.
  saved_tab_group_model_->Add(test::CreateTestSavedTabGroup());

  // Add a matching group/tab and save the ids used for GetGroupContainingTab.
  SavedTabGroup matching_group = test::CreateTestSavedTabGroup();
  base::Uuid matching_group_guid = matching_group.saved_guid();

  base::Uuid matching_tab_guid = base::Uuid::GenerateRandomV4();
  LocalTabID matching_local_tab_id = test::GenerateRandomTabID();

  SavedTabGroupTab tab(GURL(url::kAboutBlankURL), std::u16string(u"title"),
                       matching_group.saved_guid(), /*position=*/std::nullopt,
                       matching_tab_guid, matching_local_tab_id);
  matching_group.AddTabLocally(std::move(tab));
  saved_tab_group_model_->Add(std::move(matching_group));

  // Add another non matching SavedTabGroup.
  saved_tab_group_model_->Add(test::CreateTestSavedTabGroup());
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
            saved_tab_group_model_->GetGroupContainingTab(LocalTabID()));
}

// Toggle pin state should trigger SavedTabGroupUpdatedLocally.
TEST_P(SavedTabGroupModelObserverTest, TogglePinState) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  SavedTabGroup group(test::CreateTestSavedTabGroup());
  saved_tab_group_model_->Add(group);

  saved_tab_group_model_->TogglePinState(group.saved_guid());

  const int index = retrieved_group_.size() - 1;
  ASSERT_GE(index, 0);

  SavedTabGroup received_group = retrieved_group_[index];
  EXPECT_TRUE(received_group.is_pinned());
  EXPECT_EQ(group.local_group_id(), received_group.local_group_id());
  EXPECT_EQ(saved_tab_group_model_->GetIndexOf(received_group.saved_guid()),
            retrieved_index_);
}

TEST_P(SavedTabGroupModelObserverTest, MigrateSavedTabGroupsFromV1) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  // Add 5 unpinned SavedTabGroups to the model.
  SavedTabGroup group1(u"Tab Group 1", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group1);
  SavedTabGroup group2(u"Tab Group 2", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group2);
  SavedTabGroup group3(u"Tab Group 3", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group3);
  SavedTabGroup group4(u"Tab Group 4", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group4);
  SavedTabGroup group5(u"Tab Group 5", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt);
  saved_tab_group_model_->Add(group5);

  EXPECT_EQ(5u, saved_tab_group_model_->saved_tab_groups().size());

  ClearSignals();
  ASSERT_EQ(0u, retrieved_group_.size());

  // Verify 4 of them are updated.
  saved_tab_group_model_->MigrateTabGroupSavesUIUpdate();
  ASSERT_EQ(4u, retrieved_group_.size());
}

TEST_P(SavedTabGroupModelObserverTest, UpdateLocalCacheGuid) {
  base::Uuid group_1_id = base::Uuid::GenerateRandomV4();
  base::Uuid group_2_id = base::Uuid::GenerateRandomV4();
  base::Uuid group_3_id = base::Uuid::GenerateRandomV4();
  base::Uuid group_4_id = base::Uuid::GenerateRandomV4();

  const std::string edit_to_cache_guid = "edit_to_cache_guid";
  const std::string dont_edit_cache_guid = "dont_edit_cache_guid";
  const std::string second_edit_cache_guid = "second_edit_cache_guid";
  SavedTabGroup group1(u"Tab Group 1", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt /*position*/, group_1_id /*saved_guid*/,
                       std::nullopt /*local_group_id*/,
                       std::nullopt /*creator_cache_guid*/,
                       std::nullopt /*last_updater_cache_guid*/);
  saved_tab_group_model_->Add(std::move(group1));
  SavedTabGroup group2(u"Tab Group 2", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt /*position*/, group_2_id /*saved_guid*/,
                       std::nullopt /*local_group_id*/,
                       std::nullopt /*creator_cache_guid*/,
                       std::nullopt /*last_updater_cache_guid*/);
  saved_tab_group_model_->Add(group2);
  SavedTabGroup group3(u"Tab Group 3", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt /*position*/, group_3_id /*saved_guid*/,
                       std::nullopt /*local_group_id*/,
                       dont_edit_cache_guid /*creator_cache_guid*/,
                       std::nullopt /*last_updater_cache_guid*/);
  saved_tab_group_model_->Add(group3);
  SavedTabGroup group4(u"Tab Group 4", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt /*position*/, group_4_id /*saved_guid*/,
                       std::nullopt /*local_group_id*/,
                       second_edit_cache_guid /*creator_cache_guid*/,
                       std::nullopt /*last_updater_cache_guid*/);
  saved_tab_group_model_->Add(group4);

  saved_tab_group_model_->UpdateLocalCacheGuid(std::nullopt,
                                               edit_to_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_1_id)->creator_cache_guid(),
            edit_to_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_2_id)->creator_cache_guid(),
            edit_to_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_3_id)->creator_cache_guid(),
            dont_edit_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_4_id)->creator_cache_guid(),
            second_edit_cache_guid);

  saved_tab_group_model_->UpdateLocalCacheGuid(second_edit_cache_guid,
                                               edit_to_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_1_id)->creator_cache_guid(),
            edit_to_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_2_id)->creator_cache_guid(),
            edit_to_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_3_id)->creator_cache_guid(),
            dont_edit_cache_guid);
  EXPECT_EQ(saved_tab_group_model_->Get(group_4_id)->creator_cache_guid(),
            edit_to_cache_guid);
}

TEST_P(SavedTabGroupModelObserverTest, UpdateLocalCacheGuidForTabs) {
  const std::string cache_guid1 = "cache_guid1";
  const std::string cache_guid2 = "cache_guid2";
  const std::string cache_guid_tab2 = "cache_guid_tab2";

  SavedTabGroup group = test::CreateTestSavedTabGroup();
  base::Uuid group_id = group.saved_guid();
  SavedTabGroupTab tab1(GURL(url::kAboutBlankURL), std::u16string(u"title"),
                        group.saved_guid(), /*position=*/std::nullopt,
                        std::nullopt, std::nullopt);
  SavedTabGroupTab tab2(GURL(url::kAboutBlankURL), std::u16string(u"title"),
                        group.saved_guid(), /*position=*/std::nullopt,
                        std::nullopt, std::nullopt);
  tab2.SetCreatorCacheGuid(cache_guid_tab2);
  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  saved_tab_group_model_->Add(group);

  base::Uuid tab1_id = tab1.saved_tab_guid();
  base::Uuid tab2_id = tab2.saved_tab_guid();
  const SavedTabGroup* retrieved_group = saved_tab_group_model_->Get(group_id);
  const SavedTabGroupTab* retrieved_tab1 = retrieved_group->GetTab(tab1_id);
  const SavedTabGroupTab* retrieved_tab2 = retrieved_group->GetTab(tab2_id);

  saved_tab_group_model_->UpdateLocalCacheGuid(std::nullopt, cache_guid1);

  EXPECT_EQ(retrieved_group->creator_cache_guid(), cache_guid1);
  EXPECT_EQ(retrieved_tab1->creator_cache_guid(), cache_guid1);
  EXPECT_EQ(retrieved_tab2->creator_cache_guid(), cache_guid_tab2);

  saved_tab_group_model_->UpdateLocalCacheGuid(cache_guid1, cache_guid2);
  EXPECT_EQ(retrieved_group->creator_cache_guid(), cache_guid2);
  EXPECT_EQ(retrieved_tab1->creator_cache_guid(), cache_guid2);
  EXPECT_EQ(retrieved_tab2->creator_cache_guid(), cache_guid_tab2);

  saved_tab_group_model_->UpdateLocalCacheGuid(cache_guid_tab2, std::nullopt);
  EXPECT_EQ(retrieved_group->creator_cache_guid(), cache_guid2);
  EXPECT_EQ(retrieved_tab1->creator_cache_guid(), cache_guid2);
  EXPECT_EQ(retrieved_tab2->creator_cache_guid(), std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(SavedTabGroupModel,
                         SavedTabGroupModelTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(SavedTabGroupModel,
                         SavedTabGroupModelObserverTest,
                         testing::Bool());

}  // namespace

}  // namespace tab_groups
