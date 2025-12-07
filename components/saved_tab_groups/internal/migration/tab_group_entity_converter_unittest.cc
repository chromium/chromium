// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/migration/tab_group_entity_converter.h"

#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

namespace {
constexpr char kGroupGuid[] = "00000000-0000-0000-0000-000000000001";
constexpr char kTabGuid[] = "00000000-0000-0000-0000-000000000002";
constexpr char kTabUrl[] = "https://www.google.com/";
constexpr char kGroupTitle[] = "Test Group";
constexpr char kTabTitle[] = "Google";
constexpr int64_t kCreationTime = 1234567890;
constexpr int64_t kUpdateTime = 9876543210;
constexpr int kPosition = 1;

// Helper to create a private entity for a group.
syncer::EntityData CreatePrivateGroupEntity() {
  syncer::EntityData entity;
  entity.name = kGroupTitle;
  sync_pb::SavedTabGroupSpecifics* specifics =
      entity.specifics.mutable_saved_tab_group();
  specifics->set_guid(kGroupGuid);
  specifics->set_creation_time_windows_epoch_micros(kCreationTime);
  specifics->set_update_time_windows_epoch_micros(kUpdateTime);

  sync_pb::SavedTabGroup* group = specifics->mutable_group();
  group->set_title(kGroupTitle);
  group->set_color(sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE);
  group->set_position(kPosition);
  return entity;
}

// Helper to create a shared entity for a group.
syncer::EntityData CreateSharedGroupEntity() {
  syncer::EntityData entity;
  entity.name = kGroupTitle;
  sync_pb::SharedTabGroupDataSpecifics* specifics =
      entity.specifics.mutable_shared_tab_group_data();
  specifics->set_guid(kGroupGuid);
  specifics->set_update_time_windows_epoch_micros(kUpdateTime);

  sync_pb::SharedTabGroup* group = specifics->mutable_tab_group();
  group->set_title(kGroupTitle);
  group->set_color(sync_pb::SharedTabGroup::BLUE);
  group->set_originating_tab_group_guid(kGroupGuid);
  return entity;
}

// Helper to create a private entity for a tab.
syncer::EntityData CreatePrivateTabEntity() {
  syncer::EntityData entity;
  entity.name = kTabTitle;
  sync_pb::SavedTabGroupSpecifics* specifics =
      entity.specifics.mutable_saved_tab_group();
  specifics->set_guid(kTabGuid);
  specifics->set_creation_time_windows_epoch_micros(kCreationTime);
  specifics->set_update_time_windows_epoch_micros(kUpdateTime);

  sync_pb::SavedTabGroupTab* tab = specifics->mutable_tab();
  tab->set_url(kTabUrl);
  tab->set_title(kTabTitle);
  tab->set_group_guid(kGroupGuid);
  tab->set_position(kPosition);
  return entity;
}

// Helper to create a shared entity for a tab.
syncer::EntityData CreateSharedTabEntity() {
  syncer::EntityData entity;
  entity.name = kTabTitle;
  sync_pb::SharedTabGroupDataSpecifics* specifics =
      entity.specifics.mutable_shared_tab_group_data();
  specifics->set_guid(kTabGuid);
  specifics->set_update_time_windows_epoch_micros(kUpdateTime);

  sync_pb::SharedTab* tab = specifics->mutable_tab();
  tab->set_url(kTabUrl);
  tab->set_title(kTabTitle);
  tab->set_shared_tab_group_guid(kGroupGuid);
  return entity;
}

}  // namespace

class TabGroupEntityConverterTest : public testing::Test {
 protected:
  TabGroupEntityConverter converter_;
};

TEST_F(TabGroupEntityConverterTest, ConvertsPrivateGroupToShared) {
  syncer::EntityData private_entity = CreatePrivateGroupEntity();
  std::unique_ptr<syncer::EntityData> shared_entity =
      converter_.CreateSharedEntityFromPrivate(private_entity);

  ASSERT_TRUE(shared_entity);
  EXPECT_EQ(shared_entity->name, kGroupTitle);

  const sync_pb::SharedTabGroupDataSpecifics& shared_specifics =
      shared_entity->specifics.shared_tab_group_data();
  EXPECT_EQ(shared_specifics.guid(), kGroupGuid);
  EXPECT_EQ(shared_specifics.update_time_windows_epoch_micros(), kUpdateTime);
  ASSERT_TRUE(shared_specifics.has_tab_group());

  const sync_pb::SharedTabGroup& group = shared_specifics.tab_group();
  EXPECT_EQ(group.title(), kGroupTitle);
  EXPECT_EQ(group.color(), sync_pb::SharedTabGroup::BLUE);
  EXPECT_EQ(group.originating_tab_group_guid(), kGroupGuid);
}

TEST_F(TabGroupEntityConverterTest, ConvertsSharedGroupToPrivate) {
  syncer::EntityData shared_entity = CreateSharedGroupEntity();
  std::unique_ptr<syncer::EntityData> private_entity =
      converter_.CreatePrivateEntityFromShared(shared_entity);

  ASSERT_TRUE(private_entity);
  EXPECT_EQ(private_entity->name, kGroupTitle);

  const sync_pb::SavedTabGroupSpecifics& private_specifics =
      private_entity->specifics.saved_tab_group();
  EXPECT_EQ(private_specifics.guid(), kGroupGuid);
  EXPECT_EQ(private_specifics.creation_time_windows_epoch_micros(),
            kUpdateTime);
  EXPECT_EQ(private_specifics.update_time_windows_epoch_micros(), kUpdateTime);
  ASSERT_TRUE(private_specifics.has_group());

  const sync_pb::SavedTabGroup& group = private_specifics.group();
  EXPECT_EQ(group.title(), kGroupTitle);
  EXPECT_EQ(group.color(), sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE);
}

TEST_F(TabGroupEntityConverterTest, ConvertsPrivateTabToShared) {
  syncer::EntityData private_entity = CreatePrivateTabEntity();
  std::unique_ptr<syncer::EntityData> shared_entity =
      converter_.CreateSharedEntityFromPrivate(private_entity);

  ASSERT_TRUE(shared_entity);
  EXPECT_EQ(shared_entity->name, kTabTitle);

  const sync_pb::SharedTabGroupDataSpecifics& shared_specifics =
      shared_entity->specifics.shared_tab_group_data();
  EXPECT_EQ(shared_specifics.guid(), kTabGuid);
  EXPECT_EQ(shared_specifics.update_time_windows_epoch_micros(), kUpdateTime);
  ASSERT_TRUE(shared_specifics.has_tab());

  const sync_pb::SharedTab& tab = shared_specifics.tab();
  EXPECT_EQ(tab.url(), kTabUrl);
  EXPECT_EQ(tab.title(), kTabTitle);
  EXPECT_EQ(tab.shared_tab_group_guid(), kGroupGuid);
}

TEST_F(TabGroupEntityConverterTest, ConvertsSharedTabToPrivate) {
  syncer::EntityData shared_entity = CreateSharedTabEntity();
  std::unique_ptr<syncer::EntityData> private_entity =
      converter_.CreatePrivateEntityFromShared(shared_entity);

  ASSERT_TRUE(private_entity);
  EXPECT_EQ(private_entity->name, kTabTitle);

  const sync_pb::SavedTabGroupSpecifics& private_specifics =
      private_entity->specifics.saved_tab_group();
  EXPECT_EQ(private_specifics.guid(), kTabGuid);
  EXPECT_EQ(private_specifics.creation_time_windows_epoch_micros(),
            kUpdateTime);
  EXPECT_EQ(private_specifics.update_time_windows_epoch_micros(), kUpdateTime);
  ASSERT_TRUE(private_specifics.has_tab());

  const sync_pb::SavedTabGroupTab& tab = private_specifics.tab();
  EXPECT_EQ(tab.url(), kTabUrl);
  EXPECT_EQ(tab.title(), kTabTitle);
  EXPECT_EQ(tab.group_guid(), kGroupGuid);
}

}  // namespace tab_groups
