// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_DATABASE_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CORE_TEST_DATABASE_TEST_UTILS_H_

namespace base {
class FilePath;
}

namespace history {

// Sets `dir` to the path of the history data directory. Returns true on success
// or false, in which case `dir` is undefined.
[[nodiscard]] bool GetTestDataHistoryDir(base::FilePath* dir);

// Create the test database at `db_path` from the golden file at `ascii_path` in
// the "history" subdir of the components test data dir.
[[nodiscard]] bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                                         const char* ascii_path);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_DATABASE_TEST_UTILS_H_
