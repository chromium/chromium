// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group.h"

#include <memory>

#include "base/guid.h"
#include "base/time/time.h"
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

  void CompareTabs(SavedTabGroupTab group1, SavedTabGroupTab group2) {
    EXPECT_EQ(group1.url(), group2.url());
    EXPECT_EQ(group1.guid(), group2.guid());
    EXPECT_EQ(group1.group_guid(), group2.group_guid());
    EXPECT_EQ(group1.creation_time_windows_epoch_micros(),
              group2.creation_time_windows_epoch_micros());
    EXPECT_EQ(group1.update_time_windows_epoch_micros(),
              group2.update_time_windows_epoch_micros());
  }

  base::Time time_;
};

TEST_F(SavedTabGroupConversionTest, GroupToSpecificRetainsData) {
  // Create a group.
  const std::u16string& title = u"Test title";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;
  absl::optional<base::GUID> saved_guid = base::GUID::GenerateRandomV4();
  absl::optional<base::Time> creation_time_windows_epoch_micros = time_;
  absl::optional<base::Time> update_time_windows_epoch_micros = time_;
  SavedTabGroup group(title, color, {}, saved_guid, absl::nullopt,
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
  SavedTabGroupTab tab(GURL("chrome://hidden_link"),
                       base::GUID::GenerateRandomV4(), nullptr,
                       base::GUID::GenerateRandomV4(), time_, time_);

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
  pb_specific->set_guid(base::GUID::GenerateRandomV4().AsLowercaseString());

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
  pb_specific->set_guid(base::GUID::GenerateRandomV4().AsLowercaseString());

  int64_t time_in_micros = time_.ToDeltaSinceWindowsEpoch().InMicroseconds();
  pb_specific->set_creation_time_windows_epoch_micros(time_in_micros);
  pb_specific->set_update_time_windows_epoch_micros(time_in_micros);

  sync_pb::SavedTabGroupTab* pb_tab = pb_specific->mutable_tab();
  pb_tab->set_url("chrome://newtab/");
  pb_tab->set_group_guid(base::GUID::GenerateRandomV4().AsLowercaseString());

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
