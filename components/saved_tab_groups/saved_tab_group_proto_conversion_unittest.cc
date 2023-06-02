// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group.h"

#include <memory>

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SavedTabGroupConversionTest : public testing::Test {
 public:
  SavedTabGroupConversionTest() : time_(base::Time::Now()) {}

  // Compare Specifics
  void CompareGroupSpecifics(sync_pb::SavedTabGroupSpecifics* sp1,
                             sync_pb::SavedTabGroupSpecifics* sp2) {
    EXPECT_EQ(sp1->guid(), sp2->guid());
    EXPECT_EQ(sp1->group().title(), sp2->group().title());
    EXPECT_EQ(sp1->group().color(), sp2->group().color());
    EXPECT_EQ(sp1->creation_time_windows_epoch_micros(),
              sp2->creation_time_windows_epoch_micros());
    EXPECT_EQ(sp1->update_time_windows_epoch_micros(),
              sp2->update_time_windows_epoch_micros());
  }

  void CompareTabSpecifics(sync_pb::SavedTabGroupSpecifics* sp1,
                           sync_pb::SavedTabGroupSpecifics* sp2) {
    EXPECT_EQ(sp1->guid(), sp2->guid());
    EXPECT_EQ(sp1->tab().url(), sp2->tab().url());
    EXPECT_EQ(sp1->tab().title(), sp2->tab().title());
    EXPECT_EQ(sp1->tab().group_guid(), sp2->tab().group_guid());
    EXPECT_EQ(sp1->creation_time_windows_epoch_micros(),
              sp2->creation_time_windows_epoch_micros());
    EXPECT_EQ(sp1->update_time_windows_epoch_micros(),
              sp2->update_time_windows_epoch_micros());
  }

  // Compare SavedTabGroups
  void CompareGroups(SavedTabGroup group1, SavedTabGroup group2) {
    EXPECT_EQ(group1.title(), group2.title());
    EXPECT_EQ(group1.color(), group2.color());
    EXPECT_EQ(group1.saved_guid(), group2.saved_guid());
    EXPECT_EQ(group1.creation_time_windows_epoch_micros(),
              group2.creation_time_windows_epoch_micros());
    EXPECT_EQ(group1.update_time_windows_epoch_micros(),
              group2.update_time_windows_epoch_micros());
  }

  void CompareTabs(SavedTabGroupTab tab1, SavedTabGroupTab tab2) {
    EXPECT_EQ(tab1.url(), tab2.url());
    EXPECT_EQ(tab1.saved_tab_guid(), tab2.saved_tab_guid());
    EXPECT_EQ(tab1.title(), tab2.title());
    EXPECT_EQ(tab1.saved_group_guid(), tab2.saved_group_guid());
    EXPECT_EQ(tab1.creation_time_windows_epoch_micros(),
              tab2.creation_time_windows_epoch_micros());
    EXPECT_EQ(tab1.update_time_windows_epoch_micros(),
              tab2.update_time_windows_epoch_micros());
  }

  base::Time time_;
};

TEST_F(SavedTabGroupConversionTest, GroupToSpecificRetainsData) {
  // Create a group.
  const std::u16string& title = u"Test title";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;
  absl::optional<base::Uuid> saved_guid = base::Uuid::GenerateRandomV4();
  absl::optional<base::Time> creation_time_windows_epoch_micros = time_;
  absl::optional<base::Time> update_time_windows_epoch_micros = time_;
  SavedTabGroup group(title, color, {}, 0, saved_guid, absl::nullopt,
                      creation_time_windows_epoch_micros,
                      update_time_windows_epoch_micros);

  // Use the group to create a STGSpecific.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific =
      group.ToSpecifics();

  // Turn Specific into a group.
  SavedTabGroup group2 = SavedTabGroup::FromSpecifics(*specific);

  // Verify the 2 groups hold the same data.
  CompareGroups(group, group2);

  specific->clear_entity();
}

TEST_F(SavedTabGroupConversionTest, TabToSpecificRetainsData) {
  // Create a tab.
  SavedTabGroupTab tab(GURL("chrome://hidden_link"), u"Hidden Title",
                       base::Uuid::GenerateRandomV4(), /*position=*/0,
                       base::Uuid::GenerateRandomV4(), absl::nullopt, time_,
                       time_);

  // Create a STGSpecific using `tab`.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific = tab.ToSpecifics();

  // Turn Specific into a tab.
  SavedTabGroupTab tab2 = SavedTabGroupTab::FromSpecifics(*specific);

  // Verify the 2 tabs hold the same data.
  CompareTabs(tab, tab2);

  specific->clear_entity();
}

TEST_F(SavedTabGroupConversionTest, SpecificToGroupRetainsData) {
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> pb_specific =
      std::make_unique<sync_pb::SavedTabGroupSpecifics>();
  pb_specific->set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  int64_t time_in_micros = time_.ToDeltaSinceWindowsEpoch().InMicroseconds();
  pb_specific->set_creation_time_windows_epoch_micros(time_in_micros);
  pb_specific->set_update_time_windows_epoch_micros(time_in_micros);

  sync_pb::SavedTabGroup* pb_group = pb_specific->mutable_group();
  pb_group->set_color(sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE);
  pb_group->set_title("Another test title");

  // Turn a specific into a group.
  SavedTabGroup group = SavedTabGroup::FromSpecifics(*pb_specific);

  // Turn the group back into a specific.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> pb_specific_2 =
      group.ToSpecifics();

  // Verify the 2 specifics hold the same data.
  CompareGroupSpecifics(pb_specific.get(), pb_specific_2.get());

  pb_specific->clear_group();
  pb_specific_2->clear_group();
}

TEST_F(SavedTabGroupConversionTest, SpecificToTabRetainsData) {
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> pb_specific =
      std::make_unique<sync_pb::SavedTabGroupSpecifics>();
  pb_specific->set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  int64_t time_in_micros = time_.ToDeltaSinceWindowsEpoch().InMicroseconds();
  pb_specific->set_creation_time_windows_epoch_micros(time_in_micros);
  pb_specific->set_update_time_windows_epoch_micros(time_in_micros);

  sync_pb::SavedTabGroupTab* pb_tab = pb_specific->mutable_tab();
  pb_tab->set_url("chrome://newtab/");
  pb_tab->set_group_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  pb_tab->set_title("New Tab Title");

  // Turn a specific into a tab.
  SavedTabGroupTab tab = SavedTabGroupTab::FromSpecifics(*pb_specific);

  // Turn the tab back into a specific.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> pb_specific_2 =
      tab.ToSpecifics();

  // Verify the 2 specifics hold the same data.
  CompareTabSpecifics(pb_specific.get(), pb_specific_2.get());

  pb_specific->clear_tab();
  pb_specific_2->clear_tab();
}

// Verifies that merging 2 group objects (1 Sync, 1 SavedTabGroup) merges the
// most recently updated object correctly.
TEST_F(SavedTabGroupConversionTest, MergedGroupHoldsCorrectData) {
  // Create a group.
  const base::Time old_time = base::Time::Now();
  const std::u16string& title = u"Test title";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;
  absl::optional<base::Uuid> saved_guid = base::Uuid::GenerateRandomV4();
  absl::optional<base::Time> creation_time_windows_epoch_micros = time_;
  absl::optional<base::Time> update_time_windows_epoch_micros = time_;
  SavedTabGroup group1(title, color, {}, 0, saved_guid, absl::nullopt,
                       creation_time_windows_epoch_micros,
                       update_time_windows_epoch_micros);

  // Create a new group with the same data and update it. Calling set functions
  // should internally update update_time_windows_epoch_micros.
  SavedTabGroup group2 = SavedTabGroup::FromSpecifics(*group1.ToSpecifics());
  group2.SetColor(tab_groups::TabGroupColorId::kGreen);
  group2.SetTitle(u"New Title");

  // Expect that group2 is a valid group to merge with and that group1 hold the
  // same data after the merge.
  EXPECT_TRUE(group1.ShouldMergeGroup(*group2.ToSpecifics()));
  group1.MergeGroup(*group2.ToSpecifics());
  CompareGroups(group1, group2);

  // Expect that group2 is not a valid group to merge. No merging should be
  // done.
  group1.SetColor(tab_groups::TabGroupColorId::kOrange);
  group1.SetTitle(u"Another title");
  group2.SetUpdateTimeWindowsEpochMicros(old_time);
  EXPECT_FALSE(group1.ShouldMergeGroup(*group2.ToSpecifics()));
}

// Verifies that merging 2 tab objects (1 Sync, 1 SavedTabGroupTab)
TEST_F(SavedTabGroupConversionTest, MergedTabHoldsCorrectData) {
  // Create a tab.
  const base::Time old_time = base::Time::Now();
  base::Uuid saved_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroupTab tab1(GURL("Test url"), u"Test Title", saved_guid,
                        /*position=*/0);

  // Create a new group with the same data and update it. Calling set functions
  // should internally update update_time_windows_epoch_micros.
  SavedTabGroupTab tab2 = SavedTabGroupTab::FromSpecifics(*tab1.ToSpecifics());
  tab2.SetURL(GURL("new url"));
  tab2.SetTitle(u"New Title");

  // Expect that tab2 is a valid group to merge with and that the tab1 holds the
  // same data after the merge.
  EXPECT_TRUE(tab1.ShouldMergeTab(*tab2.ToSpecifics()));
  tab1.MergeTab(*tab2.ToSpecifics());
  CompareTabs(tab1, tab2);

  // Expect that tab2 is not a valid group to merge. No merging should be done.
  tab1.SetTitle(u"A title");
  tab1.SetURL(GURL("Another url"));
  tab2.SetUpdateTimeWindowsEpochMicros(old_time);
  EXPECT_FALSE(tab1.ShouldMergeTab(*tab2.ToSpecifics()));
}
