// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/update_client/task_send_uninstall_ping.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"

namespace update_client {

TaskSendUninstallPing::TaskSendUninstallPing(
    scoped_refptr<UpdateEngine> update_engine,
    const std::string& id,
    const base::Version& version,
    int reason,
    bool requires_network_encryption,
    Callback callback)
    : update_engine_(update_engine),
      id_(id),
      version_(version),
      reason_(reason),
      requires_network_encryption_(requires_network_encryption),
      callback_(std::move(callback)) {}

TaskSendUninstallPing::~TaskSendUninstallPing() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TaskSendUninstallPing::Run() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (id_.empty()) {
    TaskComplete(Error::INVALID_ARGUMENT);
    return;
  }

  update_engine_->SendUninstallPing(
      id_, version_, reason_, requires_network_encryption_,
      base::BindOnce(&TaskSendUninstallPing::TaskComplete, this));
}

void TaskSendUninstallPing::Cancel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  TaskComplete(Error::UPDATE_CANCELED);
}

std::vector<std::string> TaskSendUninstallPing::GetIds() const {
  return std::vector<std::string>{id_};
}

void TaskSendUninstallPing::TaskComplete(Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), scoped_refptr<Task>(this), error));
}

}  // namespace update_client
