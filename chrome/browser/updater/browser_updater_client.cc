// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/update_service.h"
#include "components/version_info/version_info.h"

BrowserUpdaterClient::BrowserUpdaterClient()
    : callback_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

BrowserUpdaterClient::~BrowserUpdaterClient() = default;

void BrowserUpdaterClient::Register() {
  BeginRegister(
      {}, {}, version_info::GetVersionNumber(),
      base::BindOnce(&BrowserUpdaterClient::RegistrationCompleted, this));
}

void BrowserUpdaterClient::RegistrationCompleted(
    updater::UpdateService::Result result) {
  if (result != updater::UpdateService::Result::kSuccess) {
    VLOG(1) << "Updater registration error: " << result;
  }
}

void BrowserUpdaterClient::CheckForUpdate(
    base::RepeatingCallback<void(updater::UpdateService::UpdateState)>
        version_updater_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  updater::UpdateService::UpdateState update_state;
  update_state.state =
      updater::UpdateService::UpdateState::State::kCheckingForUpdates;
  callback_task_runner_->PostTask(
      FROM_HERE, base::BindRepeating(version_updater_callback, update_state));
  BeginUpdateCheck(
      base::BindRepeating(&BrowserUpdaterClient::HandleStatusUpdate, this,
                          std::move(version_updater_callback)),
      base::BindOnce(&BrowserUpdaterClient::UpdateCompleted, this,
                     std::move(version_updater_callback)));
}

void BrowserUpdaterClient::HandleStatusUpdate(
    base::RepeatingCallback<void(updater::UpdateService::UpdateState)> callback,
    updater::UpdateService::UpdateState update_state) {
  callback_task_runner_->PostTask(FROM_HERE,
                                  base::BindRepeating(callback, update_state));
}

void BrowserUpdaterClient::UpdateCompleted(
    base::RepeatingCallback<void(updater::UpdateService::UpdateState)> callback,
    updater::UpdateService::Result result) {
  VLOG(1) << "Result of update was: " << result;

  if (result != updater::UpdateService::Result::kSuccess) {
    updater::UpdateService::UpdateState update_state;
    update_state.state =
        updater::UpdateService::UpdateState::State::kUpdateError;
    update_state.error_category =
        updater::UpdateService::ErrorCategory::kUpdateCheck;
    update_state.error_code = static_cast<int>(result);

    callback_task_runner_->PostTask(
        FROM_HERE, base::BindRepeating(callback, update_state));
  }
}
