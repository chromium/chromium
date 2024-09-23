// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/history_unittest_base.h"

#include <stdint.h>

#include <vector>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "sql/database.h"

namespace history {

HistoryUnitTestBase::~HistoryUnitTestBase() {
}

void HistoryUnitTestBase::ExecuteSQLScript(const base::FilePath& sql_path,
                                           const base::FilePath& db_path) {
  std::string sql;
  ASSERT_TRUE(base::ReadFileToString(sql_path, &sql))
      << "Path: " << sql_path << "\nContent read before failure: " << sql;

  // Replace the 'last_visit_time', 'visit_time', 'time_slot' values in this
  // SQL with the current time.
  int64_t now = base::Time::Now().ToInternalValue();
  std::vector<std::string> sql_time;
  sql_time.push_back(base::StringPrintf("%" PRId64, now));  // last_visit_time
  sql_time.push_back(base::StringPrintf("%" PRId64, now));  // visit_time
  sql_time.push_back(base::StringPrintf("%" PRId64, now));  // time_slot
  sql = base::ReplaceStringPlaceholders(sql, sql_time, nullptr);

  sql::Database connection;
  ASSERT_TRUE(connection.Open(db_path));
  ASSERT_TRUE(connection.Execute(sql));
}

HistoryUnitTestBase::HistoryUnitTestBase() {
}

}  // namespace history
