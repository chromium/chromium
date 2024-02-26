// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

class HistoryEmbeddingsSqlDatabaseTest : public testing::Test {
 public:
  void SetUp() override { CHECK(history_dir_.CreateUniqueTempDir()); }

  void TearDown() override { CHECK(history_dir_.Delete()); }

 protected:
  base::ScopedTempDir history_dir_;
};

TEST_F(HistoryEmbeddingsSqlDatabaseTest, InitializeFromEmpty) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  EXPECT_TRUE(
      base::PathExists(history_dir_.GetPath().Append(kHistoryEmbeddingsName)))
      << "Initialization creates the DB file.";
  sql_database.reset();
  EXPECT_TRUE(
      base::PathExists(history_dir_.GetPath().Append(kHistoryEmbeddingsName)))
      << "It's still there after destruction.";
}

}  // namespace history_embeddings
