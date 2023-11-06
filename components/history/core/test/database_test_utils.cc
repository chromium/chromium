// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/database_test_utils.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "sql/test/test_helpers.h"

namespace history {

[[nodiscard]] bool GetTestDataHistoryDir(base::FilePath* dir) {
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, dir)) {
    return false;
  }
  *dir = dir->AppendASCII("components");
  *dir = dir->AppendASCII("test");
  *dir = dir->AppendASCII("data");
  *dir = dir->AppendASCII("history");
  return true;
}

[[nodiscard]] bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                                         const char* ascii_path) {
  base::FilePath dir;
  if (!GetTestDataHistoryDir(&dir))
    return false;
  return sql::test::CreateDatabaseFromSQL(db_path, dir.AppendASCII(ascii_path));
}

}  // namespace history
