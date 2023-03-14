// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_CHECK_FOR_UPDATE_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_CHECK_FOR_UPDATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/update_client/task.h"
#include "components/update_client/update_client.h"

namespace update_client {

class UpdateEngine;
enum class Error;

// Defines a specialized task for checking an id for updates. This is an
// update discovery feature. No updates are applied.
class TaskCheckForUpdate : public Task {
 public:
  TaskCheckForUpdate(
      scoped_refptr<UpdateEngine> update_engine,
      const std::string& id,
      UpdateClient::CrxDataCallback crx_data_callback,
      UpdateClient::CrxStateChangeCallback crx_state_change_callback,
      bool is_foreground,
      base::OnceCallback<void(scoped_refptr<Task> task, Error error)> callback);
  TaskCheckForUpdate(const TaskCheckForUpdate&) = delete;
  TaskCheckForUpdate& operator=(const TaskCheckForUpdate&) = delete;

  // Overrides for Task.
  void Run() override;
  void Cancel() override;
  std::vector<std::string> GetIds() const override;

 private:
  ~TaskCheckForUpdate() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateEngine> update_engine_;
  const std::string id_;
  UpdateClient::CrxDataCallback crx_data_callback_;
  UpdateClient::CrxStateChangeCallback crx_state_change_callback_;
  const bool is_foreground_ = false;
  base::OnceCallback<void(scoped_refptr<Task> task, Error error)> callback_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_CHECK_FOR_UPDATE_H_
