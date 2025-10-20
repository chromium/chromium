// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/internal/migration_state_database_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
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

TEST_F(MigrationStateDatabaseImplTest, TestConstructionAndDestruction) {
  auto migration_state_db = std::make_unique<MigrationStateDatabaseImpl>(
      temp_dir_.GetPath(),
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
}

TEST_F(MigrationStateDatabaseImplTest, TestInitialization) {
  auto migration_state_db = std::make_unique<MigrationStateDatabaseImpl>(
      temp_dir_.GetPath(),
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  base::test::TestFuture<bool> future;
  migration_state_db->Init(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace data_sharing
