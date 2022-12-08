// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/test_history_database.h"

#include "components/history/core/browser/history_database_params.h"
#include "components/version_info/channel.h"

namespace history {
const DownloadInterruptReason kTestDownloadInterruptReasonNone = 0;
const DownloadInterruptReason kTestDownloadInterruptReasonCrash = 1;

TestHistoryDatabase::TestHistoryDatabase()
    : HistoryDatabase(kTestDownloadInterruptReasonNone,
                      kTestDownloadInterruptReasonCrash) {
}

TestHistoryDatabase::~TestHistoryDatabase() {
}

HistoryDatabaseParams TestHistoryDatabaseParamsForPath(
    const base::FilePath& history_dir) {
  return HistoryDatabaseParams(history_dir, kTestDownloadInterruptReasonNone,
                               kTestDownloadInterruptReasonCrash,
                               version_info::Channel::UNKNOWN);
}

}  // namespace history
