// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_SEND_UNINSTALL_PING_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_SEND_UNINSTALL_PING_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/task.h"
#include "components/update_client/update_client.h"

namespace update_client {

class UpdateEngine;
enum class Error;

// Defines a specialized task for sending the uninstall ping.
class TaskSendUninstallPing : public Task {
 public:
  using Callback =
      base::OnceCallback<void(scoped_refptr<Task> task, Error error)>;

  // `update_engine` is injected here to handle the task.
  // `crx_component` represents the CRX to send the ping for.
  // `reason` is the reason for the uninstall ping
  // `callback` is called to return the execution flow back to creator of
  //    this task when the task is done.
  TaskSendUninstallPing(scoped_refptr<UpdateEngine> update_engine,
                        const CrxComponent& crx_component,
                        int reason,
                        Callback callback);

  TaskSendUninstallPing(const TaskSendUninstallPing&) = delete;
  TaskSendUninstallPing& operator=(const TaskSendUninstallPing&) = delete;

  void Run() override;

  void Cancel() override;

  std::vector<std::string> GetIds() const override;

 private:
  ~TaskSendUninstallPing() override;

  // Called when the task has completed either because the task has run or
  // it has been canceled.
  void TaskComplete(Error error);

  base::ThreadChecker thread_checker_;
  scoped_refptr<UpdateEngine> update_engine_;
  const CrxComponent crx_component_;
  const int reason_;
  Callback callback_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_SEND_UNINSTALL_PING_H_
