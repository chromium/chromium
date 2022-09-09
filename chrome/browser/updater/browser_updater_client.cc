// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/update_service.h"
#include "components/version_info/version_info.h"

BrowserUpdaterClient::BrowserUpdaterClient() {}

BrowserUpdaterClient::~BrowserUpdaterClient() = default;

void BrowserUpdaterClient::Register() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &BrowserUpdaterClient::BeginRegister, this,
          version_info::GetVersionNumber(),
          base::BindPostTask(
              base::SequencedTaskRunnerHandle::Get(),
              base::BindOnce(&BrowserUpdaterClient::RegistrationCompleted,
                             this))));
}

void BrowserUpdaterClient::RegistrationCompleted(
    updater::UpdateService::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != updater::UpdateService::Result::kSuccess) {
    VLOG(1) << "Updater registration error: " << result;
  }
}

void BrowserUpdaterClient::GetUpdaterVersion(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &BrowserUpdaterClient::BeginGetUpdaterVersion, this,
          base::BindPostTask(
              base::SequencedTaskRunnerHandle::Get(),
              base::BindOnce(&BrowserUpdaterClient::GetUpdaterVersionCompleted,
                             this, std::move(callback)))));
}

void BrowserUpdaterClient::GetUpdaterVersionCompleted(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Detected updater version: " << version;
  std::move(callback).Run(version);
}

void BrowserUpdaterClient::CheckForUpdate(
    updater::UpdateService::StateChangeCallback version_updater_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  updater::UpdateService::UpdateState update_state;
  update_state.state =
      updater::UpdateService::UpdateState::State::kCheckingForUpdates;
  version_updater_callback.Run(update_state);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BrowserUpdaterClient::BeginUpdateCheck, this,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        version_updater_callback),
                     base::BindPostTask(
                         base::SequencedTaskRunnerHandle::Get(),
                         base::BindOnce(&BrowserUpdaterClient::UpdateCompleted,
                                        this, version_updater_callback))));
}

void BrowserUpdaterClient::UpdateCompleted(
    updater::UpdateService::StateChangeCallback callback,
    updater::UpdateService::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Result of update was: " << result;

  if (result != updater::UpdateService::Result::kSuccess) {
    updater::UpdateService::UpdateState update_state;
    update_state.state =
        updater::UpdateService::UpdateState::State::kUpdateError;
    update_state.error_category =
        updater::UpdateService::ErrorCategory::kUpdateCheck;
    update_state.error_code = static_cast<int>(result);

    callback.Run(update_state);
  }
}
