// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/task_send_registration_ping.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"

namespace update_client {

TaskSendRegistrationPing::TaskSendRegistrationPing(
    scoped_refptr<UpdateEngine> update_engine,
    const CrxComponent& crx_component,
    Callback callback)
    : update_engine_(update_engine),
      crx_component_(crx_component),
      callback_(std::move(callback)) {}

TaskSendRegistrationPing::~TaskSendRegistrationPing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TaskSendRegistrationPing::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (crx_component_.app_id.empty()) {
    TaskComplete(Error::INVALID_ARGUMENT);
    return;
  }

  update_engine_->SendRegistrationPing(
      crx_component_,
      base::BindOnce(&TaskSendRegistrationPing::TaskComplete, this));
}

void TaskSendRegistrationPing::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TaskComplete(Error::UPDATE_CANCELED);
}

std::vector<std::string> TaskSendRegistrationPing::GetIds() const {
  return std::vector<std::string>{crx_component_.app_id};
}

void TaskSendRegistrationPing::TaskComplete(Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&TaskSendRegistrationPing::RunCallback, this, error));
}

void TaskSendRegistrationPing::RunCallback(Error error) {
  std::move(callback_).Run(this, error);
}

}  // namespace update_client
