// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/syncer_util.h"

#include <memory>

#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine_impl/test_entry_factory.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class GetUpdatePositionTest : public ::testing::Test {
 public:
  void SetUp() override {
    dir_maker_.SetUp();
    entry_factory_ = std::make_unique<TestEntryFactory>(directory());
  }

  void TearDown() override { dir_maker_.TearDown(); }

  syncable::Directory* directory() { return dir_maker_.directory(); }

  TestEntryFactory* entry_factory() { return entry_factory_.get(); }

  GetUpdatePositionTest() {
    InitUpdate();

    // Init test_position to some valid position value, but don't assign
    // it to the update just yet.
    std::string pos_suffix = UniquePosition::RandomSuffix();
    test_position = UniquePosition::InitialPosition(pos_suffix);
  }

  void InitUpdate() {
    update.set_id_string("I");
    update.set_parent_id_string("P");
    update.set_version(10);
    update.set_mtime(100);
    update.set_ctime(100);
    update.set_deleted(false);
    update.mutable_specifics()->mutable_bookmark()->set_title("Chrome");
    update.mutable_specifics()->mutable_bookmark()->set_url(
        "https://www.chrome.com");
  }

  void InitSuffixIngredients() {
    update.set_originator_cache_guid("CacheGUID");
    update.set_originator_client_item_id("OrigID");
  }

  void InitProtoPosition() {
    update.mutable_unique_position()->CopyFrom(test_position.ToProto());
  }

  void InitInt64Position(int64_t pos_value) {
    update.set_position_in_parent(pos_value);
  }

  sync_pb::SyncEntity update;
  UniquePosition test_position;
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestDirectorySetterUpper dir_maker_;
  std::unique_ptr<TestEntryFactory> entry_factory_;
};

// Generate a suffix from originator client GUID and client-assigned ID.  These
// values should always be present in updates sent down to the client, and
// combine to create a globally unique value.
TEST_F(GetUpdatePositionTest, SuffixFromUpdate) {
  InitSuffixIngredients();

  // Expect suffix is valid and consistent.
  std::string suffix1 = GetUniqueBookmarkTagFromUpdate(update);
  std::string suffix2 = GetUniqueBookmarkTagFromUpdate(update);

  EXPECT_EQ(suffix1, suffix2);
  EXPECT_TRUE(UniquePosition::IsValidSuffix(suffix1));
}

// Receive an update without the ingredients used to make a consistent suffix.
//
// The server should never send us an update like this.  If it does,
// that's a bug and it needs to be fixed.  Still, we'd like to not
// crash and have fairly reasonable results in this scenario.
TEST_F(GetUpdatePositionTest, SuffixFromRandom) {
  // Intentonally do not call InitSuffixIngredients()

  // Expect suffix is valid but inconsistent.
  std::string suffix1 = GetUniqueBookmarkTagFromUpdate(update);
  std::string suffix2 = GetUniqueBookmarkTagFromUpdate(update);

  EXPECT_NE(suffix1, suffix2);
  EXPECT_TRUE(UniquePosition::IsValidSuffix(suffix1));
  EXPECT_TRUE(UniquePosition::IsValidSuffix(suffix2));
}

TEST_F(GetUpdatePositionTest, FromInt64) {
  InitSuffixIngredients();
  InitInt64Position(10);

  std::string suffix = GetUniqueBookmarkTagFromUpdate(update);

  // Expect the result is valid.
  UniquePosition pos = GetUpdatePosition(update, suffix);
  EXPECT_TRUE(pos.IsValid());

  // Expect the position had some effect on ordering.
  EXPECT_TRUE(pos.LessThan(
      UniquePosition::FromInt64(11, UniquePosition::RandomSuffix())));
}

TEST_F(GetUpdatePositionTest, FromProto) {
  InitSuffixIngredients();
  InitInt64Position(10);

  std::string suffix = GetUniqueBookmarkTagFromUpdate(update);

  // The proto position is not set, so we should get one based on the int64_t.
  // It should not match the proto we defined in the test harness.
  UniquePosition int64_pos = GetUpdatePosition(update, suffix);
  EXPECT_FALSE(int64_pos.Equals(test_position));

  // Move the test harness' position value into the update proto.
  // Expect that it takes precedence over the int64_t-based position.
  InitProtoPosition();
  UniquePosition pos = GetUpdatePosition(update, suffix);
  EXPECT_TRUE(pos.Equals(test_position));
}

TEST_F(GetUpdatePositionTest, FromNothing) {
  // Init none of the ingredients necessary to make a position.
  // Verify we still generate a valid position locally.

  std::string suffix = GetUniqueBookmarkTagFromUpdate(update);
  UniquePosition pos = GetUpdatePosition(update, suffix);
  EXPECT_TRUE(pos.IsValid());
}

namespace {

sync_pb::EntitySpecifics DefaultBookmarkSpecifics() {
  sync_pb::EntitySpecifics result;
  AddDefaultFieldValue(BOOKMARKS, &result);
  return result;
}

}  // namespace

// Checks that whole cycle of unique_position updating from
// server works fine and does not browser crash.
TEST_F(GetUpdatePositionTest, UpdateServerFieldsFromUpdateTest) {
  InitSuffixIngredients();  // Initialize update with valid data.

  std::string root_server_id = syncable::Id::GetRoot().GetServerId();
  int64_t handle = entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "I", DefaultBookmarkSpecifics(), root_server_id);

  syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, directory());
  syncable::MutableEntry target(&trans, syncable::GET_BY_HANDLE, handle);

  // Before update, target has invalid bookmark tag and unique position.
  EXPECT_FALSE(UniquePosition::IsValidSuffix(target.GetUniqueBookmarkTag()));
  EXPECT_FALSE(target.GetServerUniquePosition().IsValid());
  UpdateServerFieldsFromUpdate(&target, update, "name");

  // After update, target has valid bookmark tag and unique position.
  EXPECT_TRUE(UniquePosition::IsValidSuffix(target.GetUniqueBookmarkTag()));
  EXPECT_TRUE(target.GetServerUniquePosition().IsValid());
}

// Checks that whole cycle of unique_position updating does not
// browser crash even data from server is invalid.
// It looks like server bug, but browser should not crash and work further.
TEST_F(GetUpdatePositionTest, UpdateServerFieldsFromInvalidUpdateTest) {
  // Do not initialize data in update, update is invalid.

  std::string root_server_id = syncable::Id::GetRoot().GetServerId();
  int64_t handle = entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "I", DefaultBookmarkSpecifics(), root_server_id);

  syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, directory());
  syncable::MutableEntry target(&trans, syncable::GET_BY_HANDLE, handle);

  // Before update, target has invalid bookmark tag and unique position.
  EXPECT_FALSE(UniquePosition::IsValidSuffix(target.GetUniqueBookmarkTag()));
  EXPECT_FALSE(target.GetServerUniquePosition().IsValid());
  UpdateServerFieldsFromUpdate(&target, update, "name");

  // After update, target has valid bookmark tag and unique position.
  EXPECT_TRUE(UniquePosition::IsValidSuffix(target.GetUniqueBookmarkTag()));
  EXPECT_TRUE(target.GetServerUniquePosition().IsValid());
}

TEST_F(GetUpdatePositionTest, UpdateServerFieldsFromInvalidUniquePositionTest) {
  InitSuffixIngredients();  // Initialize update with valid data.
  sync_pb::SyncEntity invalid_update(update);

  // Create and Setup an invalid position
  sync_pb::UniquePosition* invalid_position = new sync_pb::UniquePosition();
  invalid_position->set_value("");
  invalid_update.set_allocated_unique_position(invalid_position);

  std::string root_server_id = syncable::Id::GetRoot().GetServerId();
  int64_t handle = entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "I", DefaultBookmarkSpecifics(), root_server_id);

  syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, directory());
  syncable::MutableEntry target(&trans, syncable::GET_BY_HANDLE, handle);

  // Before update, target has invalid bookmark tag and unique position.
  EXPECT_FALSE(UniquePosition::IsValidSuffix(target.GetUniqueBookmarkTag()));
  EXPECT_FALSE(target.GetServerUniquePosition().IsValid());
  UpdateServerFieldsFromUpdate(&target, invalid_update, "name");

  // After update, target has valid bookmark tag and unique position.
  EXPECT_TRUE(UniquePosition::IsValidSuffix(target.GetUniqueBookmarkTag()));
  EXPECT_TRUE(target.GetServerUniquePosition().IsValid());
}

}  // namespace syncer
