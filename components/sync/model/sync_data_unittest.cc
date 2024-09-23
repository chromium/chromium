// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_data.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace syncer {

namespace {

const char kSyncTag[] = "3984729834";
const DataType kDatatype = PREFERENCES;
const char kNonUniqueTitle[] = "my preference";

TEST(SyncDataTest, NoArgCtor) {
  SyncData data;
  EXPECT_FALSE(data.IsValid());
}

TEST(SyncDataTest, CreateLocalDelete) {
  SyncData data = SyncData::CreateLocalDelete(kSyncTag, kDatatype);
  EXPECT_TRUE(data.IsValid());
  EXPECT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, kSyncTag),
            data.GetClientTagHash());
  EXPECT_EQ(kDatatype, data.GetDataType());
}

TEST(SyncDataTest, CreateLocalData) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference();
  SyncData data =
      SyncData::CreateLocalData(kSyncTag, kNonUniqueTitle, specifics);
  EXPECT_TRUE(data.IsValid());
  EXPECT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, kSyncTag),
            data.GetClientTagHash());
  EXPECT_EQ(kDatatype, data.GetDataType());
  EXPECT_EQ(kNonUniqueTitle, data.GetTitle());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
  EXPECT_FALSE(data.ToString().empty());
}

TEST(SyncDataTest, CreateRemoteData) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference();
  SyncData data = SyncData::CreateRemoteData(
      specifics, ClientTagHash::FromUnhashed(PREFERENCES, kSyncTag));
  EXPECT_TRUE(data.IsValid());
  EXPECT_EQ(ClientTagHash::FromUnhashed(PREFERENCES, kSyncTag),
            data.GetClientTagHash());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
  EXPECT_FALSE(data.ToString().empty());
}

}  // namespace

}  // namespace syncer
