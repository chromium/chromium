// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/remove_uninstalled_apps_task.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_impl_impl.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

UpdateServiceImpl::UpdateServiceImpl(UpdaterScope scope,
                                     scoped_refptr<Configurator> config)
    : scope_(scope),
      config_(config),
      delegate_(base::MakeRefCounted<UpdateServiceImplImpl>(config)) {}

void UpdateServiceImpl::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->GetVersion(std::move(callback));
}

void UpdateServiceImpl::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->FetchPolicies(std::move(callback));
}

void UpdateServiceImpl::RegisterApp(const RegistrationRequest& request,
                                    base::OnceCallback<void(int)> callback) {
  // Registering apps with the updater is always allowed.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->RegisterApp(request, std::move(callback));
}

void UpdateServiceImpl::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  // Asking the updater for app status is always allowed.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->GetAppStates(std::move(callback));
}

void UpdateServiceImpl::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEulaAccepted() || IsOemMode()) {
    VLOG(1) << __func__ << " rejected (EULA required or OEM mode).";
    base::MakeRefCounted<RemoveUninstalledAppsTask>(config_, scope_)
        ->Run(std::move(callback));
    return;
  }
  delegate_->RunPeriodicTasks(std::move(callback));
}

void UpdateServiceImpl::CheckForUpdate(
    const std::string& app_id,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEulaAccepted() || IsOemMode()) {
    VLOG(1) << __func__ << " rejected (EULA required or OEM mode).";
    std::move(callback).Run(Result::kEulaRequiredOrOemMode);
    return;
  }
  delegate_->FetchPolicies(base::BindPostTaskToCurrentDefault(base::BindOnce(
      [](scoped_refptr<UpdateServiceImplImpl> delegate,
         const std::string& app_id, Priority priority,
         PolicySameVersionUpdate policy_same_version_update,
         base::RepeatingCallback<void(const UpdateState&)> state_update,
         base::OnceCallback<void(Result)> callback, int policy_fetch_result) {
        VLOG(1) << "Policy fetch result: " << policy_fetch_result;
        delegate->CheckForUpdate(app_id, priority, policy_same_version_update,
                                 state_update, std::move(callback));
      },
      delegate_, app_id, priority, policy_same_version_update, state_update,
      std::move(callback))));
}

void UpdateServiceImpl::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEulaAccepted() || IsOemMode()) {
    VLOG(1) << __func__ << " rejected (EULA required or OEM mode).";
    std::move(callback).Run(Result::kEulaRequiredOrOemMode);
    return;
  }
  delegate_->Update(app_id, install_data_index, priority,
                    policy_same_version_update, state_update,
                    std::move(callback));
}

void UpdateServiceImpl::UpdateAll(
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEulaAccepted() || IsOemMode()) {
    VLOG(1) << __func__ << " rejected (EULA required or OEM mode).";
    std::move(callback).Run(Result::kEulaRequiredOrOemMode);
    return;
  }
  delegate_->UpdateAll(state_update, std::move(callback));
}

void UpdateServiceImpl::Install(
    const RegistrationRequest& registration,
    const std::string& client_install_data,
    const std::string& install_data_index,
    Priority priority,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  // Online installers can only be downloaded after ToS acceptance.
  AcceptEula();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->Install(registration, client_install_data, install_data_index,
                     priority, state_update, std::move(callback));
}

void UpdateServiceImpl::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->CancelInstalls(app_id);
}

void UpdateServiceImpl::RunInstaller(
    const std::string& app_id,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data,
    const std::string& install_settings,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  // Offline installs are always permitted, to support OEM cases.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->RunInstaller(app_id, installer_path, install_args, install_data,
                          install_settings, state_update, std::move(callback));
}

void UpdateServiceImpl::AcceptEula() {
  base::MakeRefCounted<PersistedData>(scope_, config_->GetPrefService(),
                                      nullptr)
      ->SetEulaRequired(false);
}

bool UpdateServiceImpl::IsEulaAccepted() {
  scoped_refptr<PersistedData> persisted_data =
      base::MakeRefCounted<PersistedData>(scope_, config_->GetPrefService(),
                                          nullptr);
  if (!persisted_data->GetEulaRequired()) {
    return true;
  }
  if (EulaAccepted(persisted_data->GetAppIds())) {
    // Mark the acceptance for future queries.
    AcceptEula();
    return true;
  }
  return false;
}

bool UpdateServiceImpl::IsOemMode() {
#if BUILDFLAG(IS_WIN)
  return IsSystemInstall() && IsOemInstalling();
#else
  return false;
#endif
}

UpdateServiceImpl::~UpdateServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
