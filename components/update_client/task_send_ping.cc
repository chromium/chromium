// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/update_client/task_send_ping.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"

namespace update_client {

TaskSendPing::TaskSendPing(scoped_refptr<UpdateEngine> update_engine,
                           const CrxComponent& crx_component,
                           UpdateClient::PingParams ping_params,
                           Callback callback)
    : update_engine_(update_engine),
      crx_component_(crx_component),
      ping_params_(ping_params),
      callback_(std::move(callback)) {}

TaskSendPing::~TaskSendPing() = default;

void TaskSendPing::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (crx_component_.app_id.empty()) {
    TaskComplete(Error::INVALID_ARGUMENT);
    return;
  }

  update_engine_->SendPing(crx_component_, ping_params_,
                           base::BindOnce(&TaskSendPing::TaskComplete, this));
}

void TaskSendPing::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TaskComplete(Error::UPDATE_CANCELED);
}

std::vector<std::string> TaskSendPing::GetIds() const {
  return std::vector<std::string>{crx_component_.app_id};
}

void TaskSendPing::TaskComplete(Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), scoped_refptr<Task>(this), error));
}

}  // namespace update_client
