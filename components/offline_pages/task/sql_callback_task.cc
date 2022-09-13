// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/offline_pages/task/sql_callback_task.h"

#include "components/offline_pages/task/sql_store_base.h"

namespace offline_pages {

using ExecuteCallback = SqlCallbackTask::ExecuteCallback;

SqlCallbackTask::SqlCallbackTask(SqlStoreBase* store,
                                 ExecuteCallback exec_callback,
                                 DoneCallback done_callback)
    : store_(store),
      exec_callback_(std::move(exec_callback)),
      done_callback_(std::move(done_callback)) {}

SqlCallbackTask::~SqlCallbackTask() = default;

void SqlCallbackTask::Run() {
  // Execute() requires that the callback returns a value, so we use
  // ExecuteAndReturn to return a dummy boolean which is ignored.
  store_->Execute(std::move(exec_callback_),
                  base::BindOnce(&SqlCallbackTask::Done, GetWeakPtr()), false);
}

void SqlCallbackTask::Done(bool result) {
  TaskComplete();
  if (done_callback_)
    std::move(done_callback_).Run(result);
}

}  // namespace offline_pages
