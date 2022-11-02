// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "components/version_info/version_info.h"

BrowserUpdaterClient::BrowserUpdaterClient(
    scoped_refptr<updater::UpdateService> update_service)
    : update_service_(update_service) {}

BrowserUpdaterClient::~BrowserUpdaterClient() = default;

void BrowserUpdaterClient::Register(base::OnceClosure complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BrowserUpdaterClient::GetRegistrationRequest, this),
      base::BindOnce(
          [](base::OnceCallback<void(int)> callback,
             scoped_refptr<updater::UpdateService> update_service,
             const updater::RegistrationRequest& request) {
            update_service->RegisterApp(request, std::move(callback));
          },
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindOnce(&BrowserUpdaterClient::RegistrationCompleted, this,
                             std::move(complete))),
          update_service_));
}

void BrowserUpdaterClient::RegistrationCompleted(base::OnceClosure complete,
                                                 int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != updater::kRegistrationSuccess) {
    VLOG(1) << "Updater registration error: " << result;
  }
  std::move(complete).Run();
}

void BrowserUpdaterClient::GetUpdaterVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_service_->GetVersion(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BrowserUpdaterClient::GetUpdaterVersionCompleted, this,
                     std::move(callback))));
}

void BrowserUpdaterClient::GetUpdaterVersionCompleted(
    base::OnceCallback<void(const base::Version&)> callback,
    const base::Version& version) {
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
  update_service_->Update(
      GetAppId(), {}, updater::UpdateService::Priority::kForeground,
      updater::UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         version_updater_callback),
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&BrowserUpdaterClient::UpdateCompleted,
                                        this, version_updater_callback)));
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

void BrowserUpdaterClient::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_service_->RunPeriodicTasks(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BrowserUpdaterClient::RunPeriodicTasksCompleted, this,
                     std::move(callback))));
}

void BrowserUpdaterClient::RunPeriodicTasksCompleted(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void BrowserUpdaterClient::IsBrowserRegistered(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_service_->GetAppStates(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BrowserUpdaterClient::IsBrowserRegisteredCompleted, this,
                     std::move(callback))));
}

void BrowserUpdaterClient::IsBrowserRegisteredCompleted(
    base::OnceCallback<void(bool)> callback,
    const std::vector<updater::UpdateService::AppState>& apps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string app_id = GetAppId();
  std::move(callback).Run(
      std::find_if(apps.begin(), apps.end(),
                   [&](const updater::UpdateService::AppState& app) {
                     return app.app_id == app_id;
                   }) != apps.end());
}

scoped_refptr<BrowserUpdaterClient> BrowserUpdaterClient::Create(
    updater::UpdaterScope scope) {
  return base::MakeRefCounted<BrowserUpdaterClient>(
      CreateUpdateServiceProxy(scope, base::Seconds(15)));
}
