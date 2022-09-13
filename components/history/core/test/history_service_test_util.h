// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_HISTORY_SERVICE_TEST_UTIL_H_
#define COMPONENTS_HISTORY_CORE_TEST_HISTORY_SERVICE_TEST_UTIL_H_

#include <memory>

namespace base {
class FilePath;
}

namespace history {
class HistoryService;

// Creates a new HistoryService that stores its data in `history_dir`.  If
// `create_db` is false, the HistoryService will fail to initialize its
// database; this is useful for testing error conditions.  This method spins the
// runloop before returning to ensure that any initialization-related tasks are
// run.
std::unique_ptr<HistoryService> CreateHistoryService(
    const base::FilePath& history_dir,
    bool create_db);

// Schedules a task on the history backend and runs a nested loop until the task
// is processed.  This blocks the caller until the history service processes all
// pending requests.
void BlockUntilHistoryProcessesPendingRequests(
    HistoryService* history_service);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_HISTORY_SERVICE_TEST_UTIL_H_
