// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/test_support/test_sqlite_utils.h"

#include <array>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

constexpr size_t kSqliteDatabaseHeaderSize = 100;

}  // namespace

void CorruptDatabaseHeaderForTesting(const base::FilePath& database_path) {
  // The SQLite database file format defines a 100-byte header. Overwrite it
  // with zeros in place so the file is no longer recognizable as SQLite.
  std::array<uint8_t, kSqliteDatabaseHeaderSize> zeros = {};

  base::File file(database_path,
                  base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.WriteAndCheck(/*offset=*/0, zeros));
}

void DropMetaTableForTesting(const base::FilePath& database_path) {
  sql::Database database(sql::DatabaseOptions().set_wal_mode(true),
                         sql::test::kTestTag);
  ASSERT_TRUE(database.Open(database_path));
  ASSERT_TRUE(database.Execute("DROP TABLE meta"));
}

}  // namespace storage
