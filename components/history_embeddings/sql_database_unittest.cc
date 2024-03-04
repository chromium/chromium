// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

class HistoryEmbeddingsSqlDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    OSCryptMocker::SetUp();
    CHECK(history_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    CHECK(history_dir_.Delete());
    OSCryptMocker::TearDown();
  }

 protected:
  base::ScopedTempDir history_dir_;
};

TEST_F(HistoryEmbeddingsSqlDatabaseTest, WriteCloseAndThenRead) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  proto::PassagesValue original_proto;
  original_proto.add_passages("fake passage 1");
  original_proto.add_passages("fake passage 2");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(1, 1, base::Time::Now(),
                                                    original_proto));

  sql_database.reset();
  sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  auto read_proto = sql_database->GetPassages(1);
  ASSERT_TRUE(read_proto);
  ASSERT_EQ(read_proto->passages_size(), 2);
  EXPECT_EQ(read_proto->passages()[0], "fake passage 1");
  EXPECT_EQ(read_proto->passages()[1], "fake passage 2");

  sql_database.reset();
  EXPECT_TRUE(
      base::PathExists(history_dir_.GetPath().Append(kHistoryEmbeddingsName)))
      << "DB file is still there after destruction.";
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, InsertOrReplace) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  proto::PassagesValue original_proto;
  original_proto.add_passages("fake passage 1");
  original_proto.add_passages("fake passage 2");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(1, 1, base::Time::Now(),
                                                    original_proto));
  original_proto.add_passages("fake passage 3");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(1, 2, base::Time::Now(),
                                                    original_proto));

  // Verify that the new one has replaced the old one.
  auto read_proto = sql_database->GetPassages(1);
  ASSERT_TRUE(read_proto);
  ASSERT_EQ(read_proto->passages_size(), 3);
  EXPECT_EQ(read_proto->passages()[0], "fake passage 1");
  EXPECT_EQ(read_proto->passages()[1], "fake passage 2");
  EXPECT_EQ(read_proto->passages()[2], "fake passage 3");

  EXPECT_FALSE(sql_database->GetPassages(2));
}

}  // namespace history_embeddings
