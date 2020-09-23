// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/update_client.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/test/test_app/test_app_version.h"
#include "chrome/updater/util.h"

namespace updater {

UpdateClient::UpdateClient()
    : callback_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

UpdateClient::~UpdateClient() = default;

bool UpdateClient::CanCheckForUpdate() {
  return CanDialIPC();
}

void UpdateClient::Register(base::OnceCallback<void(int)> callback) {
  BeginRegister({}, {}, TEST_APP_VERSION_STRING,
                base::BindOnce(&UpdateClient::RegistrationCompleted, this,
                               std::move(callback)));
}

void UpdateClient::CheckForUpdate(StatusCallback callback) {
  callback_ = std::move(callback);

  callback_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(callback_, UpdateStatus::INIT, 0, false,
                                std::string(), 0, base::string16()));

  if (CanCheckForUpdate()) {
    BeginUpdateCheck(
        base::BindRepeating(&UpdateClient::HandleStatusUpdate, this),
        base::BindOnce(&UpdateClient::UpdateCompleted, this));
  } else {
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(callback_, UpdateStatus::FAILED, 0, false,
                                  std::string(), 0, base::string16()));
  }
}

void UpdateClient::HandleStatusUpdate(UpdateService::UpdateState update_state) {
  UpdateStatus status = UpdateStatus::INIT;
  switch (update_state.state) {
    case UpdateService::UpdateState::State::kNotStarted:
      status = UpdateStatus::INIT;
      break;
    case UpdateService::UpdateState::State::kCheckingForUpdates:
      status = UpdateStatus::CHECKING;
      break;
    case UpdateService::UpdateState::State::kDownloading:
      status = UpdateStatus::CHECKING;
      break;
    case UpdateService::UpdateState::State::kInstalling:
      status = UpdateStatus::UPDATING;
      break;
    case UpdateService::UpdateState::State::kUpdated:
      status = UpdateStatus::NEARLY_UPDATED;
      break;
    case UpdateService::UpdateState::State::kNoUpdate:
      status = UpdateStatus::UPDATED;
      break;
    default:
      return;
  }

  callback_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(callback_, status, 0, false, std::string(), 0,
                                base::string16()));
}

void UpdateClient::RegistrationCompleted(base::OnceCallback<void(int)> callback,
                                         UpdateService::Result result) {
  if (result != UpdateService::Result::kSuccess) {
    LOG(ERROR) << "Updater registration error: "
               << base::NumberToString(static_cast<int>(result));
  }

  callback_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), static_cast<int>(result)));
}

void UpdateClient::UpdateCompleted(UpdateService::Result result) {
  if (result == UpdateService::Result::kSuccess) {
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(callback_, UpdateStatus::NEARLY_UPDATED, 0,
                                  false, std::string(), 0, base::string16()));
  } else {
    const base::string16 error_message(base::ASCIIToUTF16(base::StrCat(
        {"Error code: ", base::NumberToString(static_cast<int>(result))})));
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(callback_, UpdateStatus::FAILED, 0, false,
                                  std::string(), 0, error_message));
  }
}

}  // namespace updater
