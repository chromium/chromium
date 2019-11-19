// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_data.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/base_node.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace syncer {

namespace {

const char kSyncTag[] = "3984729834";
const ModelType kDatatype = PREFERENCES;
const char kNonUniqueTitle[] = "my preference";
const int64_t kId = 439829;

class SyncDataTest : public testing::Test {
 protected:
  SyncDataTest() = default;
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_pb::EntitySpecifics specifics;
};

TEST_F(SyncDataTest, NoArgCtor) {
  SyncData data;
  EXPECT_FALSE(data.IsValid());
}

TEST_F(SyncDataTest, CreateLocalDelete) {
  SyncData data = SyncData::CreateLocalDelete(kSyncTag, kDatatype);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, SyncDataLocal(data).GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
}

TEST_F(SyncDataTest, CreateLocalData) {
  specifics.mutable_preference();
  SyncData data =
      SyncData::CreateLocalData(kSyncTag, kNonUniqueTitle, specifics);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, SyncDataLocal(data).GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
  EXPECT_EQ(kNonUniqueTitle, data.GetTitle());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
}

TEST_F(SyncDataTest, CreateRemoteData) {
  specifics.mutable_preference();
  SyncData data = SyncData::CreateRemoteData(kId, specifics);
  EXPECT_TRUE(data.IsValid());
  EXPECT_FALSE(data.IsLocal());
  EXPECT_EQ(kId, SyncDataRemote(data).GetId());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
}

TEST_F(SyncDataTest, CreateRemoteDataWithInvalidId) {
  specifics.mutable_preference();
  SyncData data = SyncData::CreateRemoteData(kInvalidId, specifics);
  EXPECT_TRUE(data.IsValid());
  EXPECT_FALSE(data.IsLocal());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
}

}  // namespace

}  // namespace syncer
