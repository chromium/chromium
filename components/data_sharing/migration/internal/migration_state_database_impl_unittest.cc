// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/internal/migration_state_database_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "components/data_sharing/migration/internal/protocol/migration_state.pb.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

class MigrationStateDatabaseImplTest : public testing::Test {
 public:
  MigrationStateDatabaseImplTest() = default;
  ~MigrationStateDatabaseImplTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(MigrationStateDatabaseImplTest, TestInitialization) {
  auto migration_state_db = std::make_unique<MigrationStateDatabaseImpl>(
      temp_dir_.GetPath());

  base::test::TestFuture<bool> future;
  migration_state_db->Init(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(MigrationStateDatabaseImplTest, TestUpdateGetDelete) {
  auto migration_state_db = std::make_unique<MigrationStateDatabaseImpl>(
      temp_dir_.GetPath());

  base::test::TestFuture<bool> future;
  migration_state_db->Init(future.GetCallback());
  ASSERT_TRUE(future.Get());

  const ContextId kContextId("test_context_id");
  data_sharing_pb::MigrationState state;
  state.set_state(data_sharing_pb::MigrationState::UNSPECIFIED);

  // Test Get on an empty DB.
  EXPECT_EQ(migration_state_db->GetMigrationState(kContextId), std::nullopt);

  // Test Update and Get.
  migration_state_db->UpdateMigrationState(kContextId, state);
  std::optional<data_sharing_pb::MigrationState> result =
      migration_state_db->GetMigrationState(kContextId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->state(), state.state());

  // Test Delete.
  migration_state_db->DeleteMigrationState(kContextId);
  EXPECT_EQ(migration_state_db->GetMigrationState(kContextId), std::nullopt);
}

}  // namespace data_sharing
