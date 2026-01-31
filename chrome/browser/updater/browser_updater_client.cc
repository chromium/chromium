// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <algorithm>
#include <optional>
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
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "components/version_info/version_info.h"

namespace updater {

std::optional<mojom::AppState>& GetLastKnownBrowserRegistrationStorage() {
  static base::NoDestructor<std::optional<mojom::AppState>> storage;
  return *storage;
}

std::optional<mojom::AppState>& GetLastKnownUpdaterRegistrationStorage() {
  static base::NoDestructor<std::optional<mojom::AppState>> storage;
  return *storage;
}

std::optional<mojom::UpdateState>& GetLastOnDemandUpdateStateStorage() {
  static base::NoDestructor<std::optional<mojom::UpdateState>> storage;
  return *storage;
}

BrowserUpdaterClient::BrowserUpdaterClient(
    scoped_refptr<UpdateService> update_service)
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
             scoped_refptr<UpdateService> update_service,
             const RegistrationRequest& request) {
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
  if (result != kRegistrationSuccess) {
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
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        version_updater_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateService::UpdateState update_state;
  update_state.state = UpdateService::UpdateState::State::kCheckingForUpdates;
  version_updater_callback.Run(update_state);
  update_service_->Update(
      GetAppId(), {}, UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      /*language=*/{},
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating([](const UpdateService::UpdateState& state) {
            GetLastOnDemandUpdateStateStorage() = state;
            return state;
          }).Then(version_updater_callback)),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&BrowserUpdaterClient::UpdateCompleted, this,
                         version_updater_callback)));
}

void BrowserUpdaterClient::UpdateCompleted(
    base::RepeatingCallback<void(const UpdateService::UpdateState&)> callback,
    UpdateService::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Result of update was: " << result;

  if (result == UpdateService::Result::kSuccess ||
      result == UpdateService::Result::kUpdateCheckFailed) {
    // These statuses will have sent more descriptive information in the status
    // callback, don't overwrite it.
    return;
  }
  UpdateService::UpdateState update_state;
  update_state.state = UpdateService::UpdateState::State::kUpdateError;
  update_state.error_category = UpdateService::ErrorCategory::kUpdateCheck;
  update_state.error_code = static_cast<int>(result);
  callback.Run(update_state);
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

void BrowserUpdaterClient::GetUpdaterState(
    base::OnceCallback<void(const UpdateService::UpdaterState&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_service_->GetUpdaterState(
      base::BindOnce(&BrowserUpdaterClient::GetUpdaterStateCompleted, this,
                     std::move(callback)));
}

void BrowserUpdaterClient::GetPoliciesJson(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_service_->GetPoliciesJson(
      base::BindOnce(&BrowserUpdaterClient::GetPoliciesJsonCompleted, this,
                     std::move(callback)));
}

void BrowserUpdaterClient::GetAppStates(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_service_->GetAppStates(base::BindOnce(
      &BrowserUpdaterClient::GetAppStatesCompleted, this, std::move(callback)));
}

void BrowserUpdaterClient::IsBrowserRegisteredCompleted(
    base::OnceCallback<void(bool)> callback,
    const std::vector<UpdateService::AppState>& apps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto updater =
      std::ranges::find_if(apps, [](const UpdateService::AppState& state) {
        return base::EqualsCaseInsensitiveASCII(state.app_id, kUpdaterAppId);
      });
  if (updater != apps.end()) {
    GetLastKnownUpdaterRegistrationStorage() = *updater;
  }
  const auto app =
      std::ranges::find_if(apps, &BrowserUpdaterClient::AppMatches);
  if (app != apps.end()) {
    GetLastKnownBrowserRegistrationStorage() = *app;
  }
  std::move(callback).Run(app != apps.end());
}

void BrowserUpdaterClient::GetUpdaterStateCompleted(
    base::OnceCallback<void(const UpdateService::UpdaterState&)> callback,
    const UpdateService::UpdaterState& updater_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), updater_state));
}

void BrowserUpdaterClient::GetPoliciesJsonCompleted(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& policies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), policies));
}

void BrowserUpdaterClient::GetAppStatesCompleted(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback,
    const std::vector<mojom::AppState>& app_states) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), app_states));
}

// User and System BrowserUpdaterClients must be kept separate - the template
// function causes there to be two static variables instead of one.
template <UpdaterScope scope>
scoped_refptr<BrowserUpdaterClient> BrowserUpdaterClient::GetClient(
    base::RepeatingCallback<scoped_refptr<UpdateService>()> proxy_provider) {
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
    UpdaterScope scope) {
  return Create(base::BindRepeating(
#if BUILDFLAG(IS_WIN)
                    &CreateUpdateServiceProxyMojo,
#else   // BUILDFLAG(IS_WIN)
                    &CreateUpdateServiceProxy,
#endif  // BUILDFLAG(IS_WIN)
                    scope, base::Seconds(15)),
                scope);
}

scoped_refptr<BrowserUpdaterClient> BrowserUpdaterClient::Create(
    base::RepeatingCallback<scoped_refptr<UpdateService>()> proxy_provider,
    UpdaterScope scope) {
  return scope == UpdaterScope::kSystem
             ? GetClient<UpdaterScope::kSystem>(proxy_provider)
             : GetClient<UpdaterScope::kUser>(proxy_provider);
}

}  // namespace updater
