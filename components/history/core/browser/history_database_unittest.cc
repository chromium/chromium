// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/init_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

TEST(HistoryDatabaseTest, DropBookmarks) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;

  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.GetPath().AppendASCII("DropBookmarks.db");
  sql::Database::Delete(db_file);

  // Copy db file over that contains starred URLs.
  base::FilePath old_history_path;
  EXPECT_TRUE(GetTestDataHistoryDir(&old_history_path));
  old_history_path =
      old_history_path.Append(FILE_PATH_LITERAL("History_with_starred"));
  base::CopyFile(old_history_path, db_file);

  // Load the DB twice. The first time it should migrate. Make sure that the
  // migration leaves it in a state fit to load again later.
  for (int i = 0; i < 2; ++i) {
    TestHistoryDatabase history_db;
    ASSERT_EQ(sql::INIT_OK, history_db.Init(db_file));
    HistoryDatabase::URLEnumerator url_enumerator;
    ASSERT_TRUE(history_db.InitURLEnumeratorForEverything(&url_enumerator));
    int num_urls = 0;
    URLRow url_row;
    while (url_enumerator.GetNextURL(&url_row)) {
      ++num_urls;
    }
    ASSERT_EQ(5, num_urls);
  }
}

}  // namespace history
