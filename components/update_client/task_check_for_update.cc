// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/task_check_for_update.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"

namespace update_client {

TaskCheckForUpdate::TaskCheckForUpdate(
    scoped_refptr<UpdateEngine> update_engine,
    const std::string& id,
    UpdateClient::CrxDataCallback crx_data_callback,
    UpdateClient::CrxStateChangeCallback crx_state_change_callback,
    bool is_foreground,
    base::OnceCallback<void(scoped_refptr<Task> task, Error error)> callback)
    : update_engine_(update_engine),
      id_(id),
      crx_data_callback_(std::move(crx_data_callback)),
      crx_state_change_callback_(crx_state_change_callback),
      is_foreground_(is_foreground),
      callback_(std::move(callback)) {}

TaskCheckForUpdate::~TaskCheckForUpdate() = default;

void TaskCheckForUpdate::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_.is_null()) {
    return;
  }
  update_engine_->CheckForUpdate(
      is_foreground_, id_, std::move(crx_data_callback_),
      std::move(crx_state_change_callback_),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(std::move(callback_), base::WrapRefCounted(this))));
}

void TaskCheckForUpdate::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_.is_null()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), base::WrapRefCounted(this),
                     Error::UPDATE_CANCELED));
}

std::vector<std::string> TaskCheckForUpdate::GetIds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {id_};
}

}  // namespace update_client
