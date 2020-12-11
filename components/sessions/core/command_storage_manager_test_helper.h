// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_TEST_HELPER_H_
#define COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_TEST_HELPER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"

namespace sessions {
class SessionCommand;
class CommandStorageManager;

class CommandStorageManagerTestHelper {
 public:
  explicit CommandStorageManagerTestHelper(
      CommandStorageManager* command_storage_manager_);
  ~CommandStorageManagerTestHelper() = default;

  // This posts the task to the SequencedWorkerPool, or run immediately
  // if the SequencedWorkerPool has been shutdown.
  void RunTaskOnBackendThread(const base::Location& from_here,
                              base::OnceClosure task);

  // Returns true if any commands got processed yet - saved or queued.
  bool ProcessedAnyCommands();

  // Read the last session commands directly from file.
  std::vector<std::unique_ptr<SessionCommand>> ReadLastSessionCommands();

  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner();

 private:
  CommandStorageManager* command_storage_manager_;

  DISALLOW_COPY_AND_ASSIGN(CommandStorageManagerTestHelper);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_TEST_HELPER_H_
