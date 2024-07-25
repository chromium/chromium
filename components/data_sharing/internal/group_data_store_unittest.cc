// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_store.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {
namespace {

using testing::Eq;
using testing::UnorderedElementsAre;

class GroupDataStoreTest : public testing::Test {
 public:
  GroupDataStoreTest() = default;
  ~GroupDataStoreTest() override = default;

  GroupDataStore& store() { return store_; }

 private:
  GroupDataStore store_;
};

TEST_F(GroupDataStoreTest, ShouldStoreAndGetGroupData) {
  const GroupId group_id("test_group_id");
  GroupData group_data;
  group_data.group_token.group_id = group_id;
  group_data.display_name = "Test group";

  const VersionToken version_token("test_version_token");

  store().StoreGroupData(version_token, group_data);

  auto stored_group_data = store().GetGroupData(group_id);
  ASSERT_TRUE(stored_group_data.has_value());
  EXPECT_THAT(stored_group_data->group_token.group_id, Eq(group_id));
  EXPECT_THAT(stored_group_data->display_name, Eq(group_data.display_name));

  auto stored_version_token = store().GetGroupVersionToken(group_id);
  ASSERT_TRUE(stored_version_token.has_value());
  EXPECT_THAT(*stored_version_token, Eq(version_token));
}

TEST_F(GroupDataStoreTest, ShouldDeleteGroupData) {
  const GroupId group_id("test_group_id");
  GroupData group_data;
  group_data.group_token.group_id = group_id;

  const VersionToken version_token("test_version_token");

  store().StoreGroupData(version_token, group_data);
  ASSERT_TRUE(store().GetGroupData(group_id).has_value());
  ASSERT_TRUE(store().GetGroupVersionToken(group_id).has_value());

  store().DeleteGroupData(group_id);

  EXPECT_FALSE(store().GetGroupData(group_id).has_value());
  EXPECT_FALSE(store().GetGroupVersionToken(group_id).has_value());
}

TEST_F(GroupDataStoreTest, ShouldGetAllGroupsIds) {
  const VersionToken version_token("test_version_token");

  const GroupId group_id1("test_group_id1");
  GroupData group_data1;
  group_data1.group_token.group_id = group_id1;

  const GroupId group_id2("test_group_id2");
  GroupData group_data2;
  group_data2.group_token.group_id = group_id2;

  store().StoreGroupData(version_token, group_data1);
  store().StoreGroupData(version_token, group_data2);

  std::vector<GroupId> stored_group_ids = store().GetAllGroupsIds();
  EXPECT_THAT(stored_group_ids, UnorderedElementsAre(group_id1, group_id2));
}

}  // namespace
}  // namespace data_sharing
