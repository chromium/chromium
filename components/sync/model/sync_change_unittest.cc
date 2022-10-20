// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_change.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(SyncChangeTest, LocalDelete) {
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_DELETE;
  std::string tag = "client_tag";
  SyncChange e(FROM_HERE, change_type,
               SyncData::CreateLocalDelete(tag, PREFERENCES));
  EXPECT_EQ(change_type, e.change_type());
  EXPECT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, tag),
            e.sync_data().GetClientTagHash());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
}

TEST(SyncChangeTest, LocalUpdate) {
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_UPDATE;
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name("test");
  std::string tag = "client_tag";
  std::string title = "client_title";
  SyncChange e(FROM_HERE, change_type,
               SyncData::CreateLocalData(tag, title, specifics));
  EXPECT_EQ(change_type, e.change_type());
  EXPECT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, tag),
            e.sync_data().GetClientTagHash());
  EXPECT_EQ(title, e.sync_data().GetTitle());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  base::Value ref_spec = EntitySpecificsToValue(specifics);
  base::Value e_spec = EntitySpecificsToValue(e.sync_data().GetSpecifics());
  EXPECT_EQ(ref_spec, e_spec);
}

TEST(SyncChangeTest, LocalAdd) {
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_ADD;
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name("test");
  std::string tag = "client_tag";
  std::string title = "client_title";
  SyncChange e(FROM_HERE, change_type,
               SyncData::CreateLocalData(tag, title, specifics));
  EXPECT_EQ(change_type, e.change_type());
  EXPECT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, tag),
            e.sync_data().GetClientTagHash());
  EXPECT_EQ(title, e.sync_data().GetTitle());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  base::Value ref_spec = EntitySpecificsToValue(specifics);
  base::Value e_spec = EntitySpecificsToValue(e.sync_data().GetSpecifics());
  EXPECT_EQ(ref_spec, e_spec);
}

TEST(SyncChangeTest, SyncerChanges) {
  SyncChangeList change_list;

  // Create an update.
  sync_pb::EntitySpecifics update_specifics;
  sync_pb::PreferenceSpecifics* pref_specifics =
      update_specifics.mutable_preference();
  pref_specifics->set_name("update");
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE,
                 SyncData::CreateRemoteData(
                     update_specifics, ClientTagHash::FromHashed("unused"))));

  // Create an add.
  sync_pb::EntitySpecifics add_specifics;
  pref_specifics = add_specifics.mutable_preference();
  pref_specifics->set_name("add");
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                 SyncData::CreateRemoteData(
                     add_specifics, ClientTagHash::FromHashed("unused"))));

  // Create a delete.
  sync_pb::EntitySpecifics delete_specifics;
  pref_specifics = delete_specifics.mutable_preference();
  pref_specifics->set_name("add");
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_DELETE,
                 SyncData::CreateRemoteData(
                     delete_specifics, ClientTagHash::FromHashed("unused"))));

  ASSERT_EQ(3U, change_list.size());

  // Verify update.
  SyncChange e = change_list[0];
  EXPECT_EQ(SyncChange::ACTION_UPDATE, e.change_type());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  base::Value ref_spec = EntitySpecificsToValue(update_specifics);
  base::Value e_spec = EntitySpecificsToValue(e.sync_data().GetSpecifics());
  EXPECT_EQ(ref_spec, e_spec);

  // Verify add.
  e = change_list[1];
  EXPECT_EQ(SyncChange::ACTION_ADD, e.change_type());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  ref_spec = EntitySpecificsToValue(add_specifics);
  e_spec = EntitySpecificsToValue(e.sync_data().GetSpecifics());
  EXPECT_EQ(ref_spec, e_spec);

  // Verify delete.
  e = change_list[2];
  EXPECT_EQ(SyncChange::ACTION_DELETE, e.change_type());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  ref_spec = EntitySpecificsToValue(delete_specifics);
  e_spec = EntitySpecificsToValue(e.sync_data().GetSpecifics());
  EXPECT_EQ(ref_spec, e_spec);
}

TEST(SyncChangeTest, MoveIsCopy) {
  const std::string kTag = "client_tag";
  const std::string kTitle = "client_title";
  const std::string kPrefName = "test_name";
  const SyncChange::SyncChangeType kChangeType = SyncChange::ACTION_UPDATE;

  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name(kPrefName);
  SyncChange original(FROM_HERE, kChangeType,
                      SyncData::CreateLocalData(kTag, kTitle, specifics));

  SyncChange other = std::move(original);

  ASSERT_EQ(kChangeType, other.change_type());
  ASSERT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, kTag),
            other.sync_data().GetClientTagHash());
  ASSERT_EQ(PREFERENCES, other.sync_data().GetDataType());
  ASSERT_EQ(kPrefName, other.sync_data().GetSpecifics().preference().name());

  // The original instance should remain valid, unmodified.
  EXPECT_EQ(kChangeType, original.change_type());
  EXPECT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, kTag),
            original.sync_data().GetClientTagHash());
  EXPECT_EQ(PREFERENCES, original.sync_data().GetDataType());
  EXPECT_EQ(kPrefName, original.sync_data().GetSpecifics().preference().name());
}

}  // namespace

}  // namespace syncer
