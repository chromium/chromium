// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
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

BrowserUpdaterClient::~BrowserUpdaterClient() {
  // Weak pointers must be invalidated on the app's main sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

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
          base::BindPostTaskToCurrentDefault(
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
  update_service_->GetVersion(base::BindPostTaskToCurrentDefault(
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
    base::RepeatingCallback<void(const updater::UpdateService::UpdateState&)>
        version_updater_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  updater::UpdateService::UpdateState update_state;
  update_state.state =
      updater::UpdateService::UpdateState::State::kCheckingForUpdates;
  version_updater_callback.Run(update_state);
  update_service_->Update(
      GetAppId(), {}, updater::UpdateService::Priority::kForeground,
      updater::UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindPostTaskToCurrentDefault(version_updater_callback),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&BrowserUpdaterClient::UpdateCompleted, this,
                         version_updater_callback)));
}

void BrowserUpdaterClient::UpdateCompleted(
    base::RepeatingCallback<void(const updater::UpdateService::UpdateState&)>
        callback,
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
  update_service_->RunPeriodicTasks(base::BindPostTaskToCurrentDefault(
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
  update_service_->GetAppStates(base::BindPostTaskToCurrentDefault(
      base::BindOnce(&BrowserUpdaterClient::IsBrowserRegisteredCompleted, this,
                     std::move(callback))));
}

void BrowserUpdaterClient::IsBrowserRegisteredCompleted(
    base::OnceCallback<void(bool)> callback,
    const std::vector<updater::UpdateService::AppState>& apps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(std::find_if(apps.begin(), apps.end(),
                                       &BrowserUpdaterClient::AppMatches) !=
                          apps.end());
}

// User and System BrowserUpdaterClients must be kept separate - the template
// function causes there to be two static variables instead of one.
template <updater::UpdaterScope scope>
scoped_refptr<BrowserUpdaterClient> BrowserUpdaterClient::GetClient(
    base::RepeatingCallback<scoped_refptr<updater::UpdateService>()>
        proxy_provider) {
  // Multiple UpdateServiceProxies interfere with each other. Reuse a current
  // BrowserUpdaterClient if possible. BrowserUpdaterClients are refcounted, but
  // bad to keep around indefinitely since they can hold the RPC server open.
  // Using a static NoDestruct weak pointer keeps the objects' lifetime
  // controlled by the refcounting, but allows the function to reuse them when
  // they're alive.
  struct WeakPointerHolder {
    base::WeakPtr<BrowserUpdaterClient> client;
  };

  static base::NoDestructor<WeakPointerHolder> existing;
  if (existing->client) {
    return base::WrapRefCounted(existing->client.get());
  }

  // Else, make a new one:
  auto new_client =
      base::MakeRefCounted<BrowserUpdaterClient>(proxy_provider.Run());
  existing->client = new_client->weak_ptr_factory_.GetWeakPtr();
  return new_client;
}

scoped_refptr<BrowserUpdaterClient> BrowserUpdaterClient::Create(
    updater::UpdaterScope scope) {
  return Create(base::BindRepeating(&updater::CreateUpdateServiceProxy, scope,
                                    base::Seconds(15)),
                scope);
}

scoped_refptr<BrowserUpdaterClient> BrowserUpdaterClient::Create(
    base::RepeatingCallback<scoped_refptr<updater::UpdateService>()>
        proxy_provider,
    updater::UpdaterScope scope) {
  return scope == updater::UpdaterScope::kSystem
             ? GetClient<updater::UpdaterScope::kSystem>(proxy_provider)
             : GetClient<updater::UpdaterScope::kUser>(proxy_provider);
}
