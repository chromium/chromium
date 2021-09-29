// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_SQL_CALLBACK_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_SQL_CALLBACK_TASK_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace sql {
class Database;
}

namespace offline_pages {
class SqlStoreBase;

// A simple task that calls store->Execute() with the provided |exec_callback|
// and completes. |done_callback|, if provided, is called with the result. This
// class can be used if there are no UI thread actions that need done.
class SqlCallbackTask : public Task {
 public:
  typedef base::OnceCallback<bool(sql::Database* db)> ExecuteCallback;
  typedef base::OnceCallback<void(bool)> DoneCallback;

  SqlCallbackTask(SqlStoreBase* store,
                  ExecuteCallback exec_callback,
                  DoneCallback done_callback = {});
  ~SqlCallbackTask() override;
  void Run() override;

 private:
  base::WeakPtr<SqlCallbackTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void Done(bool result);

  SqlStoreBase* store_;
  ExecuteCallback exec_callback_;
  DoneCallback done_callback_;
  base::WeakPtrFactory<SqlCallbackTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_SQL_CALLBACK_TASK_H_
