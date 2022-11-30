// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_RUNNER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_RUNNER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"

namespace password_manager {

// This class is responsible of running credential clean-ups sequentially in the
// order they are added. The runner is informed by the the clean-up tasks that
// the clean-up is finished when a clean-up task calls CleaningCompleted. This
// class will keep the clean-up object alive until the runner is notified that
// the clean-up is finished, or until BrowserContext shutdown.
//
// Usage:
// (1) Add cleaning tasks in the order the have to be executed.
// (2) After all cleaning task are added call StartCleaning().
//
// Use CredentialsCleanerRunnerFactory to create this object.
class CredentialsCleanerRunner : public CredentialsCleaner::Observer,
                                 public KeyedService {
 public:
  CredentialsCleanerRunner();
  CredentialsCleanerRunner(const CredentialsCleanerRunner&) = delete;
  CredentialsCleanerRunner& operator=(const CredentialsCleanerRunner&) = delete;
  ~CredentialsCleanerRunner() override;

  // Adds |cleaning_task| to the |cleaning_task_queue_| if the corresponding
  // cleaning task still needs to be done.
  void MaybeAddCleaningTask(std::unique_ptr<CredentialsCleaner> cleaning_task);

  bool HasPendingTasks() const;

  void StartCleaning();

  // CredentialsCleaner::Observer:
  void CleaningCompleted() override;

  base::WeakPtr<CredentialsCleanerRunner> GetWeakPtr();

 private:
  void StartCleaningTask();

  bool cleaning_in_progress_ = false;

  base::queue<std::unique_ptr<CredentialsCleaner>> cleaning_tasks_queue_;

  base::WeakPtrFactory<CredentialsCleanerRunner> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_RUNNER_H_
