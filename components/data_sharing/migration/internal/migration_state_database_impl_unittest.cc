// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/internal/migration_state_database_impl.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

class MigrationStateDatabaseImplTest : public testing::Test {
 public:
  MigrationStateDatabaseImplTest() = default;
  ~MigrationStateDatabaseImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MigrationStateDatabaseImplTest, ShouldDoNothing) {
  // TODO(haileywang): Implement this.
}

}  // namespace data_sharing
