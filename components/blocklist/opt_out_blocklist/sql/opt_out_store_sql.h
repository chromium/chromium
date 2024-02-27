// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_SQL_OPT_OUT_STORE_SQL_H_
#define COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_SQL_OPT_OUT_STORE_SQL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace blocklist {

// OptOutStoreSQL is an instance of OptOutStore
// which is implemented using a SQLite database.
class OptOutStoreSQL : public OptOutStore {
 public:
  OptOutStoreSQL(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);

  OptOutStoreSQL(const OptOutStoreSQL&) = delete;
  OptOutStoreSQL& operator=(const OptOutStoreSQL&) = delete;

  ~OptOutStoreSQL() override;

  // OptOutStore implementation:
  void AddEntry(bool opt_out,
                const std::string& host_name,
                int type,
                base::Time now) override;
  void ClearBlockList(base::Time begin_time, base::Time end_time) override;
  void LoadBlockList(std::unique_ptr<BlocklistData> blocklist_data,
                     LoadBlockListCallback callback) override;

 private:
  // Thread this object is accessed on.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Path to the database on disk.
  const base::FilePath db_file_path_;

  // SQL connection to the SQLite database.
  std::unique_ptr<sql::Database> db_;
};

}  // namespace blocklist

#endif  // COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_SQL_OPT_OUT_STORE_SQL_H_
