// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/history_service_test_util.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/test/test_history_database.h"


namespace history {

std::unique_ptr<HistoryService> CreateHistoryService(
    const base::FilePath& history_dir,
    bool create_db) {
  std::unique_ptr<HistoryService> history_service(new HistoryService());
  if (!history_service->Init(
          !create_db, history::TestHistoryDatabaseParamsForPath(history_dir))) {
    return nullptr;
  }

  if (create_db)
    BlockUntilHistoryProcessesPendingRequests(history_service.get());
  return history_service;
}

void BlockUntilHistoryProcessesPendingRequests(
    HistoryService* history_service) {
  base::RunLoop run_loop;
  history_service->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace history
