// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_HISTORY_BACKEND_DB_BASE_TEST_H_
#define COMPONENTS_HISTORY_CORE_TEST_HISTORY_BACKEND_DB_BASE_TEST_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "components/history/core/test/history_unittest_base.h"
#include "sql/init_status.h"

namespace base {
class Time;
}

namespace history {

class BackendDelegate;
class HistoryBackend;
class HistoryDatabase;
class InMemoryHistoryBackend;
enum class DownloadState;

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class HistoryBackendDBBaseTest : public HistoryUnitTestBase {
 public:
  HistoryBackendDBBaseTest();
  ~HistoryBackendDBBaseTest() override;

 protected:
  friend class BackendDelegate;

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  // Creates the HistoryBackend and HistoryDatabase on the current thread,
  // assigning the values to backend_ and db_.
  void CreateBackendAndDatabase();
  void CreateBackendAndDatabaseAllowFail();

  void CreateDBVersion(int version);

  void DeleteBackend();

  bool AddDownload(uint32_t id,
                   const std::string& guid,
                   DownloadState state,
                   base::Time time);

  base::ScopedTempDir temp_dir_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  // names of the database files
  base::FilePath history_dir_;

  // Created via CreateBackendAndDatabase.
  scoped_refptr<HistoryBackend> backend_;
  std::unique_ptr<InMemoryHistoryBackend> in_mem_backend_;
  raw_ptr<HistoryDatabase> db_;  // Cached reference to the backend's database.
  sql::InitStatus last_profile_error_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_HISTORY_BACKEND_DB_BASE_TEST_H_
