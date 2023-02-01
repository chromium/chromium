// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_UPDATE_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_UPDATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/update_client/task.h"
#include "components/update_client/update_client.h"

namespace update_client {

class UpdateEngine;
enum class Error;

// Defines a specialized task for updating a group of CRXs.
class TaskUpdate : public Task {
 public:
  using Callback =
      base::OnceCallback<void(scoped_refptr<Task> task, Error error)>;

  // |update_engine| is injected here to handle the task.
  // |is_foreground| is true when the update task is initiated by the user.
  // |is_install| is true when the task is initiated in an install flow.
  // |ids| represents the CRXs to be updated by this task.
  // |crx_data_callback| is called to get update data for the these CRXs.
  // |callback| is called to return the execution flow back to creator of
  //    this task when the task is done.
  TaskUpdate(scoped_refptr<UpdateEngine> update_engine,
             bool is_foreground,
             bool is_install,
             const std::vector<std::string>& ids,
             UpdateClient::CrxDataCallback crx_data_callback,
             UpdateClient::CrxStateChangeCallback crx_state_change_callback,
             Callback callback);
  TaskUpdate(const TaskUpdate&) = delete;
  TaskUpdate& operator=(const TaskUpdate&) = delete;

  // Overrides for Task.
  void Run() override;
  void Cancel() override;
  std::vector<std::string> GetIds() const override;

 private:
  ~TaskUpdate() override;

  // Called when the task has completed either because the task has run or
  // it has been canceled.
  void TaskComplete(Error error);

  // Runs the task update callback.
  void RunCallback(Error error);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateEngine> update_engine_;
  const bool is_foreground_;
  const bool is_install_;
  const std::vector<std::string> ids_;
  UpdateClient::CrxDataCallback crx_data_callback_;
  UpdateClient::CrxStateChangeCallback crx_state_change_callback_;
  Callback callback_;
  bool cancelled_ = false;
  base::RepeatingClosure cancel_callback_ = base::DoNothing();
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_UPDATE_H_
