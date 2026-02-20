// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<AccessibilityAnnotatorDatabase>();
    ASSERT_TRUE(db_->Init(GetDbPath()));
  }

 protected:
  base::FilePath GetDbPath() const {
    return temp_dir_.GetPath().AppendASCII("TestDB");
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AccessibilityAnnotatorDatabase> db_;
};

TEST_F(AccessibilityAnnotatorDatabaseTest, DatabaseCorruptionIsRecovered) {
  // Corrupt the database file.
  db_.reset();
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(GetDbPath()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Try to initialize the database again.
  AccessibilityAnnotatorDatabase recovered_db;
  ASSERT_TRUE(recovered_db.Init(GetDbPath()));
  EXPECT_TRUE(expecter.SawExpectedErrors());
}

}  // namespace accessibility_annotator
