// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credentials_cleaner_runner.h"

#include <utility>

namespace password_manager {

CredentialsCleanerRunner::CredentialsCleanerRunner() = default;

CredentialsCleanerRunner::~CredentialsCleanerRunner() = default;

void CredentialsCleanerRunner::MaybeAddCleaningTask(
    std::unique_ptr<CredentialsCleaner> cleaning_task) {
  if (cleaning_task->NeedsCleaning()) {
    cleaning_tasks_queue_.push(std::move(cleaning_task));
  }
}

bool CredentialsCleanerRunner::HasPendingTasks() const {
  return !cleaning_tasks_queue_.empty();
}

void CredentialsCleanerRunner::StartCleaning() {
  if (cleaning_in_progress_) {
    return;
  }
  StartCleaningTask();
}

void CredentialsCleanerRunner::CleaningCompleted() {
  // Delete the cleaner object, because the cleaning is finished.
  DCHECK(!cleaning_tasks_queue_.empty());
  cleaning_tasks_queue_.pop();
  StartCleaningTask();
}

void CredentialsCleanerRunner::StartCleaningTask() {
  cleaning_in_progress_ = HasPendingTasks();
  if (!cleaning_in_progress_) {
    return;
  }

  cleaning_tasks_queue_.front()->StartCleaning(this);
}

base::WeakPtr<CredentialsCleanerRunner> CredentialsCleanerRunner::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace password_manager
