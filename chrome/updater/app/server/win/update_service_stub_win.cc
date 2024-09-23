// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/update_service_stub_win.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/registration_data.h"

namespace updater {

UpdateServiceStubWin::UpdateServiceStubWin(
    scoped_refptr<updater::UpdateService> impl,
    base::RepeatingClosure task_start_listener,
    base::RepeatingClosure task_end_listener)
    : impl_(impl),
      task_start_listener_(task_start_listener),
      task_end_listener_(task_end_listener) {}

UpdateServiceStubWin::~UpdateServiceStubWin() = default;

void UpdateServiceStubWin::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->GetVersion(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::FetchPolicies(
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->FetchPolicies(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::RegisterApp(const RegistrationRequest& request,
                                       base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->RegisterApp(request, std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->GetAppStates(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->RunPeriodicTasks(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::CheckForUpdate(
    const std::string& app_id,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->CheckForUpdate(app_id, priority, policy_same_version_update,
                        state_update,
                        std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->Update(app_id, install_data_index, priority,
                policy_same_version_update, state_update,
                std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::UpdateAll(
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->UpdateAll(std::move(state_update),
                   std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::Install(
    const RegistrationRequest& registration,
    const std::string& client_install_data,
    const std::string& install_data_index,
    Priority priority,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->Install(registration, client_install_data, install_data_index,
                 priority, std::move(state_update),
                 std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStubWin::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->CancelInstalls(app_id);
  task_end_listener_.Run();
}

void UpdateServiceStubWin::RunInstaller(
    const std::string& app_id,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data,
    const std::string& install_settings,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->RunInstaller(app_id, installer_path, install_args, install_data,
                      install_settings, std::move(state_update),
                      std::move(callback).Then(task_end_listener_));
}

}  // namespace updater
