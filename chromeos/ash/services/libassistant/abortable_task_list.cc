// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/abortable_task_list.h"

#include <algorithm>

namespace ash::libassistant {

AbortableTaskList::AbortableTaskList() = default;
AbortableTaskList::~AbortableTaskList() {
  AbortAll();
}

void AbortableTaskList::AbortAll() {
  // Cancel all tasks that are not finished yet.
  for (auto& task : tasks_) {
    if (!task->IsFinished())
      task->Abort();
  }

  tasks_.clear();
}

AbortableTask* AbortableTaskList::GetFirstTaskForTesting() {
  return tasks_[0].get();
}

void AbortableTaskList::AddInternal(std::unique_ptr<AbortableTask> task) {
  // We cleanup finished tasks when a new task is added.
  RemoveFinishedTasks();
  tasks_.push_back(std::move(task));
}

void AbortableTaskList::RemoveFinishedTasks() {
  tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                              [](const std::unique_ptr<AbortableTask>& task) {
                                return task->IsFinished();
                              }),
               tasks_.end());
}

}  // namespace ash::libassistant
