// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_TEST_HISTORY_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_TEST_TEST_HISTORY_DATABASE_H_

#include "components/history/core/browser/download_types.h"
#include "components/history/core/browser/history_database.h"

namespace base {
class FilePath;
}

namespace history {

struct HistoryDatabaseParams;

// The download successfully completed. Value for tests.
extern const DownloadInterruptReason kTestDownloadInterruptReasonNone;

// The download was interrupted by a browser crash. Internal use only.
// Resume pending downloads if possible. Value for tests.
extern const DownloadInterruptReason kTestDownloadInterruptReasonCrash;

// TestHistoryDatabase is a simple wrapper around HistoryDatabase that provides
// default values to the constructor.
class TestHistoryDatabase : public HistoryDatabase {
 public:
  TestHistoryDatabase();
  ~TestHistoryDatabase() override;
};

// Returns a HistoryDatabaseParams for unit tests.
HistoryDatabaseParams TestHistoryDatabaseParamsForPath(
    const base::FilePath& history_dir);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_TEST_HISTORY_DATABASE_H_
