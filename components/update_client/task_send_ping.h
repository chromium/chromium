// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_SEND_PING_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_SEND_PING_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/update_client/task.h"
#include "components/update_client/update_client.h"

namespace update_client {

class UpdateEngine;
enum class Error;

// Defines a specialized task for sending a one-off ping.
class TaskSendPing : public Task {
 public:
  using Callback =
      base::OnceCallback<void(scoped_refptr<Task> task, Error error)>;

  // `update_engine` is injected here to handle the task.
  // `crx_component` represents the CRX to send the ping for.
  // `ping_params` contains the parameters of the ping.
  // `callback` is called to return the execution flow back to creator of this
  //    task when the task is done.
  TaskSendPing(scoped_refptr<UpdateEngine> update_engine,
               const CrxComponent& crx_component,
               UpdateClient::PingParams ping_params,
               Callback callback);

  TaskSendPing(const TaskSendPing&) = delete;
  TaskSendPing& operator=(const TaskSendPing&) = delete;

  void Run() override;

  void Cancel() override;

  std::vector<std::string> GetIds() const override;

 private:
  ~TaskSendPing() override;

  // Called when the task has completed either because the task has run or
  // it has been canceled.
  void TaskComplete(Error error);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateEngine> update_engine_;
  const CrxComponent crx_component_;
  const UpdateClient::PingParams ping_params_;
  Callback callback_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_SEND_PING_H_
