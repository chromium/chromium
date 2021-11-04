// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_SEND_REGISTRATION_PING_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_SEND_REGISTRATION_PING_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/update_client/task.h"
#include "components/update_client/update_client.h"

namespace update_client {

class UpdateEngine;
enum class Error;

// Defines a specialized task for sending the registration ping.
class TaskSendRegistrationPing : public Task {
 public:
  using Callback =
      base::OnceCallback<void(scoped_refptr<Task> task, Error error)>;

  TaskSendRegistrationPing(const TaskSendRegistrationPing&) = delete;
  TaskSendRegistrationPing& operator=(const TaskSendRegistrationPing&) = delete;

  // |update_engine| is injected here to handle the task.
  // |id| represents the CRX to send the ping for.
  // |callback| is posted when the task is done.
  TaskSendRegistrationPing(scoped_refptr<UpdateEngine> update_engine,
                           const CrxComponent& crx_component,
                           Callback callback);

  void Run() override;

  void Cancel() override;

  std::vector<std::string> GetIds() const override;

 private:
  ~TaskSendRegistrationPing() override;

  // Called when the task has completed either because the task has run or
  // it has been canceled.
  void TaskComplete(Error error);

  // Runs the task registration ping callback
  void RunCallback(Error error);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateEngine> update_engine_;
  const CrxComponent crx_component_;
  Callback callback_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_SEND_REGISTRATION_PING_H_
