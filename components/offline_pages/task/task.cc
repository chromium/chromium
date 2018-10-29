// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task.h"

#include <utility>

namespace offline_pages {

Task::Task() {}

Task::~Task() {}

void Task::SetTaskCompletionCallbackForTesting(
    TaskCompletionCallback task_completion_callback) {
  SetTaskCompletionCallback(std::move(task_completion_callback));
}

void Task::SetTaskCompletionCallback(
    TaskCompletionCallback task_completion_callback) {
  // Attempts to enforce that SetTaskCompletionCallback is at most called once.
  DCHECK(task_completion_callback_.is_null());
  DCHECK(!task_completion_callback.is_null());
  task_completion_callback_ = std::move(task_completion_callback);
}

void Task::TaskComplete() {
  if (!task_completion_callback_.is_null())
    std::move(task_completion_callback_).Run(this);
}

}  // namespace offline_pages
