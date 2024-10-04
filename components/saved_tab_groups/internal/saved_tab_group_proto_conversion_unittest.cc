// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

class SavedTabGroupConversionTest : public testing::Test {
 public:
  SavedTabGroupConversionTest() : time_(base::Time::Now()) {}

  // Compare Specifics
  void CompareGroupSpecifics(const sync_pb::SavedTabGroupSpecifics& sp1,
                             const sync_pb::SavedTabGroupSpecifics& sp2) {
    EXPECT_EQ(sp1.guid(), sp2.guid());
    EXPECT_EQ(sp1.group().title(), sp2.group().title());
    EXPECT_EQ(sp1.group().color(), sp2.group().color());
    EXPECT_EQ(sp1.attribution_metadata().created().device_info().cache_guid(),
              sp2.attribution_metadata().created().device_info().cache_guid());
    EXPECT_EQ(sp1.attribution_metadata().updated().device_info().cache_guid(),
              sp2.attribution_metadata().updated().device_info().cache_guid());
    EXPECT_EQ(sp1.creation_time_windows_epoch_micros(),
              sp2.creation_time_windows_epoch_micros());
    EXPECT_EQ(sp1.update_time_windows_epoch_micros(),
              sp2.update_time_windows_epoch_micros());
  }

  void CompareTabSpecifics(const sync_pb::SavedTabGroupSpecifics& sp1,
                           const sync_pb::SavedTabGroupSpecifics& sp2) {
    EXPECT_EQ(sp1.guid(), sp2.guid());
    EXPECT_EQ(sp1.tab().url(), sp2.tab().url());
    EXPECT_EQ(sp1.tab().title(), sp2.tab().title());
    EXPECT_EQ(sp1.tab().group_guid(), sp2.tab().group_guid());
    EXPECT_EQ(sp1.attribution_metadata().created().device_info().cache_guid(),
              sp2.attribution_metadata().created().device_info().cache_guid());
    EXPECT_EQ(sp1.attribution_metadata().updated().device_info().cache_guid(),
              sp2.attribution_metadata().updated().device_info().cache_guid());
    EXPECT_EQ(sp1.creation_time_windows_epoch_micros(),
              sp2.creation_time_windows_epoch_micros());
    EXPECT_EQ(sp1.update_time_windows_epoch_micros(),
              sp2.update_time_windows_epoch_micros());
  }

  void CompareProtoGroupLocalData(const proto::SavedTabGroupData& sp1,
                                  const proto::SavedTabGroupData& sp2) {
    EXPECT_EQ(sp1.local_tab_group_data().local_group_id(),
              sp2.local_tab_group_data().local_group_id());
    EXPECT_EQ(sp1.local_tab_group_data().created_before_syncing_tab_groups(),
              sp2.local_tab_group_data().created_before_syncing_tab_groups());
    EXPECT_EQ(sp1.local_tab_group_data().close_and_delete_on_next_restore(),
              sp2.local_tab_group_data().close_and_delete_on_next_restore());
    EXPECT_EQ(sp1.local_tab_group_data()
                  .last_user_interaction_time_windows_epoch_micros(),
              sp2.local_tab_group_data()
                  .last_user_interaction_time_windows_epoch_micros());
  }

  // Compare SavedTabGroups
  void CompareGroups(const SavedTabGroup& group1, const SavedTabGroup& group2) {
    EXPECT_EQ(group1.title(), group2.title());
    EXPECT_EQ(group1.color(), group2.color());
    EXPECT_EQ(group1.saved_guid(), group2.saved_guid());
    EXPECT_EQ(group1.position(), group2.position());
    EXPECT_EQ(group1.creation_time_windows_epoch_micros(),
              group2.creation_time_windows_epoch_micros());
    EXPECT_EQ(group1.update_time_windows_epoch_micros(),
              group2.update_time_windows_epoch_micros());
    EXPECT_EQ(group1.last_user_interaction_time(),
              group2.last_user_interaction_time());
    EXPECT_EQ(group1.creator_cache_guid(), group2.creator_cache_guid());
    EXPECT_EQ(group1.last_updater_cache_guid(),
              group2.last_updater_cache_guid());
    EXPECT_EQ(group1.created_before_syncing_tab_groups(),
              group2.created_before_syncing_tab_groups());
  }

  void CompareTabs(const SavedTabGroupTab& tab1, const SavedTabGroupTab& tab2) {
    EXPECT_EQ(tab1.url(), tab2.url());
    EXPECT_EQ(tab1.saved_tab_guid(), tab2.saved_tab_guid());
    EXPECT_EQ(tab1.title(), tab2.title());
    EXPECT_EQ(tab1.saved_group_guid(), tab2.saved_group_guid());
    EXPECT_EQ(tab1.creator_cache_guid(), tab2.creator_cache_guid());
    EXPECT_EQ(tab1.last_updater_cache_guid(), tab2.last_updater_cache_guid());
    EXPECT_EQ(tab1.creation_time_windows_epoch_micros(),
              tab2.creation_time_windows_epoch_micros());
    EXPECT_EQ(tab1.update_time_windows_epoch_micros(),
              tab2.update_time_windows_epoch_micros());
  }

  base::Time time_;
};

TEST_F(SavedTabGroupConversionTest, GroupToDataRetainsData) {
  const std::u16string& title = u"Test title";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;
  std::optional<base::Uuid> saved_guid = base::Uuid::GenerateRandomV4();
  std::optional<base::Time> creation_time_windows_epoch_micros = time_;
  std::optional<base::Time> update_time_windows_epoch_micros = time_;
  SavedTabGroup group(
      title, color, {}, 0, saved_guid, test::GenerateRandomTabGroupID(),
      "creator_cache_guid_1",       // creator_cache_guid
      "last_updater_cache_guid_1",  // last_updater_cache_guid
      /*created_before_syncing_tab_groups=*/true,
      creation_time_windows_epoch_micros, update_time_windows_epoch_micros);
  group.SetLastUserInteractionTime(time_);

  proto::SavedTabGroupData proto =
      SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(group);
  EXPECT_EQ(1, proto.version());

  CompareGroups(group,
                SavedTabGroupSyncBridge::DataToSavedTabGroupForTest(proto));
}

TEST_F(SavedTabGroupConversionTest, TabToDataRetainsData) {
  SavedTabGroupTab tab(GURL("chrome://hidden_link"), u"Hidden Title",
                       base::Uuid::GenerateRandomV4(), /*position=*/0,
                       base::Uuid::GenerateRandomV4(), std::nullopt,
                       std::nullopt, std::nullopt, time_, time_);

  proto::SavedTabGroupData proto =
      SavedTabGroupSyncBridge::SavedTabGroupTabToDataForTest(tab);
  EXPECT_EQ(1, proto.version());

  CompareTabs(tab,
              SavedTabGroupSyncBridge::DataToSavedTabGroupTabForTest(proto));
}

TEST_F(SavedTabGroupConversionTest, DataToGroupRetainsData) {
  proto::SavedTabGroupData pb_data;
  sync_pb::SavedTabGroupSpecifics* pb_specific = pb_data.mutable_specifics();
  pb_specific->set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  int64_t time_in_micros = time_.ToDeltaSinceWindowsEpoch().InMicroseconds();
  pb_specific->set_creation_time_windows_epoch_micros(time_in_micros);
  pb_specific->set_update_time_windows_epoch_micros(time_in_micros);

  sync_pb::SavedTabGroup* pb_group = pb_specific->mutable_group();
  pb_group->set_color(sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE);
  pb_group->set_title("Another test title");

  // Turn a data into a group and back into data.
  CompareGroupSpecifics(
      pb_data.specifics(),
      SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(
          SavedTabGroupSyncBridge::DataToSavedTabGroupForTest(pb_data))
          .specifics());
}

TEST_F(SavedTabGroupConversionTest, DataToTabRetainsData) {
  proto::SavedTabGroupData pb_data;
  sync_pb::SavedTabGroupSpecifics* pb_specific = pb_data.mutable_specifics();
  pb_specific->set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  int64_t time_in_micros = time_.ToDeltaSinceWindowsEpoch().InMicroseconds();
  pb_specific->set_creation_time_windows_epoch_micros(time_in_micros);
  pb_specific->set_update_time_windows_epoch_micros(time_in_micros);

  sync_pb::SavedTabGroupTab* pb_tab = pb_specific->mutable_tab();
  pb_tab->set_url("chrome://newtab/");
  pb_tab->set_group_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  pb_tab->set_title("New Tab Title");

  // Verify the 2 specifics hold the same data.
  CompareTabSpecifics(
      pb_data.specifics(),
      SavedTabGroupSyncBridge::SavedTabGroupTabToDataForTest(
          SavedTabGroupSyncBridge::DataToSavedTabGroupTabForTest(pb_data))
          .specifics());
}

TEST_F(SavedTabGroupConversionTest, VerifyLocalFieldsOnProtoToGroupConversion) {
  proto::SavedTabGroupData pb_data;
  sync_pb::SavedTabGroupSpecifics* pb_specific = pb_data.mutable_specifics();
  pb_specific->set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  int64_t time_in_micros = time_.ToDeltaSinceWindowsEpoch().InMicroseconds();
  pb_specific->set_creation_time_windows_epoch_micros(time_in_micros);
  pb_specific->set_update_time_windows_epoch_micros(time_in_micros);

  sync_pb::SavedTabGroup* pb_group = pb_specific->mutable_group();
  pb_group->set_color(sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE);
  pb_group->set_title("Another test title");

  proto::LocalTabGroupData* pb_local_group_data =
      pb_data.mutable_local_tab_group_data();
  DCHECK(pb_local_group_data);
  pb_local_group_data->set_created_before_syncing_tab_groups(true);
  pb_local_group_data->set_last_user_interaction_time_windows_epoch_micros(
      time_in_micros);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  std::string serialized_local_id = base::Token::CreateRandom().ToString();
  pb_local_group_data->set_local_group_id(serialized_local_id);
#endif

  CompareProtoGroupLocalData(
      pb_data,
      SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(
          SavedTabGroupSyncBridge::DataToSavedTabGroupForTest(pb_data)));

  CompareGroupSpecifics(
      pb_data.specifics(),
      SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(
          SavedTabGroupSyncBridge::DataToSavedTabGroupForTest(pb_data))
          .specifics());
}

// Verifies that merging 2 group objects (1 Sync, 1 SavedTabGroup) merges the
// most recently updated object correctly.
TEST_F(SavedTabGroupConversionTest, MergedGroupHoldsCorrectData) {
  // Create a group.
  const base::Time old_time = base::Time::Now();
  const std::u16string& title = u"Test title";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;
  std::optional<base::Uuid> saved_guid = base::Uuid::GenerateRandomV4();
  std::optional<base::Time> creation_time_windows_epoch_micros = time_;
  std::optional<base::Time> update_time_windows_epoch_micros = time_;
  SavedTabGroup group1(title, color, {}, 0, saved_guid, std::nullopt,
                       "creator_cache_guid", "last_updater_cache_guid",
                       /*created_before_syncing_tab_groups=*/false,
                       creation_time_windows_epoch_micros,
                       update_time_windows_epoch_micros);

  // Create a new group with the same data and update it. Calling set functions
  // should internally update update_time_windows_epoch_micros.
  SavedTabGroup group2(group1);
  group2.SetColor(tab_groups::TabGroupColorId::kGreen);
  group2.SetTitle(u"New Title");

  // Expect that group2 is a valid group to merge with and that group1 hold the
  // same data after the merge.
  EXPECT_TRUE(group1.RemoteGroupHasMoreRecentUpdates(
      group2.update_time_windows_epoch_micros()));
  group1.MergeRemoteGroupMetadata(
      group2.title(), group2.color(), group2.position(),
      group2.creator_cache_guid(), group2.last_updater_cache_guid(),
      group2.update_time_windows_epoch_micros());
  CompareGroups(group1, group2);

  // Expect that group2 is not a valid group to merge. No merging should be
  // done.
  group1.SetColor(tab_groups::TabGroupColorId::kOrange);
  group1.SetTitle(u"Another title");
  group2.SetUpdateTimeWindowsEpochMicros(old_time);
  EXPECT_TRUE(group1.RemoteGroupHasMoreRecentUpdates(
      group2.update_time_windows_epoch_micros()));
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
  SavedTabGroupTab tab2(tab1);
  tab2.SetURL(GURL("new url"));
  tab2.SetTitle(u"New Title");
  tab2.SetCreatorCacheGuid("creator_cache_guid");
  tab2.SetLastUpdaterCacheGuid("last_updater_cache_guid");

  // Expect that tab2 is a valid group to merge with and that the tab1 holds the
  // same data after the merge.
  EXPECT_TRUE(tab1.ShouldMergeTab(tab2));
  tab1.MergeRemoteTab(tab2);
  CompareTabs(tab1, tab2);

  // Expect that tab2 is not a valid group to merge. No merging should be done.
  tab1.SetTitle(u"A title");
  tab1.SetURL(GURL("Another url"));
  tab2.SetUpdateTimeWindowsEpochMicros(old_time);
  EXPECT_TRUE(tab1.ShouldMergeTab(tab2));
}

}  // namespace tab_groups
