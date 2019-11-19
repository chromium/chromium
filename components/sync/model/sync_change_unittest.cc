// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_change.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// Ordered list of SyncChange's.
using SyncChangeList = std::vector<SyncChange>;

namespace {

class SyncChangeTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SyncChangeTest, LocalDelete) {
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_DELETE;
  std::string tag = "client_tag";
  SyncChange e(FROM_HERE, change_type,
               SyncData::CreateLocalDelete(tag, PREFERENCES));
  EXPECT_EQ(change_type, e.change_type());
  EXPECT_EQ(tag, SyncDataLocal(e.sync_data()).GetTag());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
}

TEST_F(SyncChangeTest, LocalUpdate) {
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_UPDATE;
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name("test");
  std::string tag = "client_tag";
  std::string title = "client_title";
  SyncChange e(FROM_HERE, change_type,
               SyncData::CreateLocalData(tag, title, specifics));
  EXPECT_EQ(change_type, e.change_type());
  EXPECT_EQ(tag, SyncDataLocal(e.sync_data()).GetTag());
  EXPECT_EQ(title, e.sync_data().GetTitle());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  std::unique_ptr<base::DictionaryValue> ref_spec(
      EntitySpecificsToValue(specifics));
  std::unique_ptr<base::DictionaryValue> e_spec(
      EntitySpecificsToValue(e.sync_data().GetSpecifics()));
  EXPECT_TRUE(ref_spec->Equals(e_spec.get()));
}

TEST_F(SyncChangeTest, LocalAdd) {
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_ADD;
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name("test");
  std::string tag = "client_tag";
  std::string title = "client_title";
  SyncChange e(FROM_HERE, change_type,
               SyncData::CreateLocalData(tag, title, specifics));
  EXPECT_EQ(change_type, e.change_type());
  EXPECT_EQ(tag, SyncDataLocal(e.sync_data()).GetTag());
  EXPECT_EQ(title, e.sync_data().GetTitle());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  std::unique_ptr<base::DictionaryValue> ref_spec(
      EntitySpecificsToValue(specifics));
  std::unique_ptr<base::DictionaryValue> e_spec(
      EntitySpecificsToValue(e.sync_data().GetSpecifics()));
  EXPECT_TRUE(ref_spec->Equals(e_spec.get()));
}

TEST_F(SyncChangeTest, SyncerChanges) {
  SyncChangeList change_list;

  // Create an update.
  sync_pb::EntitySpecifics update_specifics;
  sync_pb::PreferenceSpecifics* pref_specifics =
      update_specifics.mutable_preference();
  pref_specifics->set_name("update");
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE,
                 SyncData::CreateRemoteData(1, update_specifics)));

  // Create an add.
  sync_pb::EntitySpecifics add_specifics;
  pref_specifics = add_specifics.mutable_preference();
  pref_specifics->set_name("add");
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                 SyncData::CreateRemoteData(2, add_specifics)));

  // Create a delete.
  sync_pb::EntitySpecifics delete_specifics;
  pref_specifics = delete_specifics.mutable_preference();
  pref_specifics->set_name("add");
  change_list.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_DELETE,
                 SyncData::CreateRemoteData(3, delete_specifics)));

  ASSERT_EQ(3U, change_list.size());

  // Verify update.
  SyncChange e = change_list[0];
  EXPECT_EQ(SyncChange::ACTION_UPDATE, e.change_type());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  std::unique_ptr<base::DictionaryValue> ref_spec(
      EntitySpecificsToValue(update_specifics));
  std::unique_ptr<base::DictionaryValue> e_spec(
      EntitySpecificsToValue(e.sync_data().GetSpecifics()));
  EXPECT_TRUE(ref_spec->Equals(e_spec.get()));

  // Verify add.
  e = change_list[1];
  EXPECT_EQ(SyncChange::ACTION_ADD, e.change_type());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  ref_spec = EntitySpecificsToValue(add_specifics);
  e_spec = EntitySpecificsToValue(e.sync_data().GetSpecifics());
  EXPECT_TRUE(ref_spec->Equals(e_spec.get()));

  // Verify delete.
  e = change_list[2];
  EXPECT_EQ(SyncChange::ACTION_DELETE, e.change_type());
  EXPECT_EQ(PREFERENCES, e.sync_data().GetDataType());
  ref_spec = EntitySpecificsToValue(delete_specifics);
  e_spec = EntitySpecificsToValue(e.sync_data().GetSpecifics());
  EXPECT_TRUE(ref_spec->Equals(e_spec.get()));
}

}  // namespace

}  // namespace syncer
