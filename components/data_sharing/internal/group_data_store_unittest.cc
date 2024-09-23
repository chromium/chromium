// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_store.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {
namespace {

using testing::Eq;
using testing::UnorderedElementsAre;

class GroupDataStoreTest : public testing::Test {
 public:
  GroupDataStoreTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    InitStoreAndWaitForDBLoading();
  }

  ~GroupDataStoreTest() override = default;

  void TearDown() override {
    // This is needed to ensure that `temp_dir_` outlives any write tasks on DB
    // sequence.
    base::RunLoop run_loop;
    store_->SetShutdownCallbackForTesting(run_loop.QuitClosure());
    store_ = nullptr;
    run_loop.Run();
  }

  void MimicRestart() {
    base::RunLoop run_loop;
    store_->SetShutdownCallbackForTesting(run_loop.QuitClosure());

    store_ = nullptr;
    run_loop.Run();

    InitStoreAndWaitForDBLoading();
  }

  GroupDataStore& store() { return *store_; }

 private:
  void InitStoreAndWaitForDBLoading() {
    base::RunLoop run_loop;
    store_ = std::make_unique<GroupDataStore>(
        GetDBPath(), base::BindLambdaForTesting(
                         [&run_loop](GroupDataStore::DBInitStatus status) {
                           EXPECT_EQ(status,
                                     GroupDataStore::DBInitStatus::kSuccess);
                           run_loop.Quit();
                         }));
    run_loop.Run();
  }

  base::FilePath GetDBPath() const {
    return temp_dir_.GetPath().Append(
        base::FilePath(FILE_PATH_LITERAL("db_file")));
  }

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<GroupDataStore> store_;
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

TEST_F(GroupDataStoreTest, ShouldDeleteSingleGroup) {
  const GroupId group_id1("test_group_id1");
  GroupData group_data1;
  group_data1.group_token.group_id = group_id1;

  const GroupId group_id2("test_group_id2");
  GroupData group_data2;
  group_data2.group_token.group_id = group_id2;

  const VersionToken version_token("test_version_token");

  store().StoreGroupData(version_token, group_data1);
  store().StoreGroupData(version_token, group_data2);
  ASSERT_TRUE(store().GetGroupData(group_id1).has_value());
  ASSERT_TRUE(store().GetGroupVersionToken(group_id1).has_value());
  ASSERT_TRUE(store().GetGroupData(group_id2).has_value());
  ASSERT_TRUE(store().GetGroupVersionToken(group_id2).has_value());

  store().DeleteGroups({group_id1});

  EXPECT_FALSE(store().GetGroupData(group_id1).has_value());
  EXPECT_FALSE(store().GetGroupVersionToken(group_id1).has_value());
  // Second group should stay intact.
  EXPECT_TRUE(store().GetGroupData(group_id2).has_value());
  EXPECT_TRUE(store().GetGroupVersionToken(group_id2).has_value());
}

TEST_F(GroupDataStoreTest, ShouldDeleteMultipleGroups) {
  const GroupId group_id1("test_group_id1");
  GroupData group_data1;
  group_data1.group_token.group_id = group_id1;

  const GroupId group_id2("test_group_id2");
  GroupData group_data2;
  group_data2.group_token.group_id = group_id2;

  const VersionToken version_token("test_version_token");

  store().StoreGroupData(version_token, group_data1);
  store().StoreGroupData(version_token, group_data2);
  ASSERT_TRUE(store().GetGroupData(group_id1).has_value());
  ASSERT_TRUE(store().GetGroupVersionToken(group_id1).has_value());
  ASSERT_TRUE(store().GetGroupData(group_id2).has_value());
  ASSERT_TRUE(store().GetGroupVersionToken(group_id2).has_value());

  store().DeleteGroups({group_id1, group_id2});

  EXPECT_FALSE(store().GetGroupData(group_id1).has_value());
  EXPECT_FALSE(store().GetGroupVersionToken(group_id1).has_value());
  EXPECT_FALSE(store().GetGroupData(group_id2).has_value());
  EXPECT_FALSE(store().GetGroupVersionToken(group_id2).has_value());
}

TEST_F(GroupDataStoreTest, ShouldGetAllGroupIds) {
  const VersionToken version_token("test_version_token");

  const GroupId group_id1("test_group_id1");
  GroupData group_data1;
  group_data1.group_token.group_id = group_id1;

  const GroupId group_id2("test_group_id2");
  GroupData group_data2;
  group_data2.group_token.group_id = group_id2;

  store().StoreGroupData(version_token, group_data1);
  store().StoreGroupData(version_token, group_data2);

  std::vector<GroupId> stored_group_ids = store().GetAllGroupIds();
  EXPECT_THAT(stored_group_ids, UnorderedElementsAre(group_id1, group_id2));
}

TEST_F(GroupDataStoreTest, ShouldPersistChanges) {
  const GroupId group_id("test_group_id");
  GroupData group_data;
  group_data.group_token.group_id = group_id;
  group_data.display_name = "Test group";

  const VersionToken version_token("test_version_token");

  // Store some group data first.
  store().StoreGroupData(version_token, group_data);
  ASSERT_TRUE(store().GetGroupData(group_id).has_value());
  ASSERT_TRUE(store().GetGroupVersionToken(group_id).has_value());

  // Ensure that data is still around after restart.
  MimicRestart();
  auto stored_group_data = store().GetGroupData(group_id);
  ASSERT_TRUE(stored_group_data.has_value());
  EXPECT_THAT(stored_group_data->group_token.group_id, Eq(group_id));
  EXPECT_THAT(stored_group_data->display_name, Eq(group_data.display_name));

  auto stored_version_token = store().GetGroupVersionToken(group_id);
  ASSERT_TRUE(stored_version_token.has_value());
  EXPECT_THAT(*stored_version_token, Eq(version_token));

  // Now delete the group data.
  store().DeleteGroups({group_id});
  ASSERT_FALSE(store().GetGroupData(group_id).has_value());
  ASSERT_FALSE(store().GetGroupVersionToken(group_id).has_value());

  // Ensure that data was actually deleted from DB.
  MimicRestart();
  EXPECT_FALSE(store().GetGroupData(group_id).has_value());
  EXPECT_FALSE(store().GetGroupVersionToken(group_id).has_value());
}

}  // namespace
}  // namespace data_sharing
