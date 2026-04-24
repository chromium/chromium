// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/intent_table.h"

#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class IntentTableTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(database_.OpenInMemory()); }

 protected:
  sql::Database database_{sql::DatabaseOptions{}, sql::test::kTestTag};
  IntentTable intent_table_;
};

// Tests that initialization fails if the input database is null.
TEST_F(IntentTableTest, InitFalseIfNullDatabase) {
  EXPECT_FALSE(intent_table_.Init(nullptr));
}

// Tests that migration from clean state to version 1 immediately fails if the
// input database is null.
TEST_F(IntentTableTest, DoNotMigrateFromCleanStateToVersion1IfNullDatabase) {
  // Do not call Init() since the database is by default nullptr
  EXPECT_FALSE(intent_table_.MigrateFromCleanStateToVersion1());
}

// Tests that migration from clean state to version 1 succeeds if the input
// database is valid.
TEST_F(IntentTableTest, MigrateFromCleanStateToVersion1Success) {
  ASSERT_TRUE(intent_table_.Init(&database_));
  EXPECT_TRUE(intent_table_.MigrateFromCleanStateToVersion1());

  // Verify that the tables exist.
  EXPECT_TRUE(database_.DoesTableExist("task_intent_provenance"));
  EXPECT_TRUE(database_.DoesTableExist("task_intent"));
}

}  // namespace accessibility_annotator
