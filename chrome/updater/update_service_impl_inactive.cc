// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl_inactive.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

namespace updater {

namespace {

class UpdateServiceImplInactive : public UpdateService {
 public:
  UpdateServiceImplInactive() = default;

  // Overrides for updater::UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::Version()));
  }

  void FetchPolicies(base::OnceCallback<void(int)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    std::move(callback).Run(-1);
  }

  void RegisterApp(const RegistrationRequest& /*request*/,
                   base::OnceCallback<void(int)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), -1));
  }

  void GetAppStates(base::OnceCallback<void(const std::vector<AppState>&)>
                        callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<AppState>()));
  }

  void RunPeriodicTasks(base::OnceClosure callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    std::move(callback).Run();
  }

  void CheckForUpdate(
      const std::string& /*app_id*/,
      Priority /*priority*/,
      PolicySameVersionUpdate /*policy_same_version_update*/,
      base::RepeatingCallback<void(const UpdateState&)> /*state_update*/,
      base::OnceCallback<void(Result)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UpdateService::Result::kInactive));
  }

  void Update(
      const std::string& /*app_id*/,
      const std::string& /*install_data_index*/,
      Priority /*priority*/,
      PolicySameVersionUpdate /*policy_same_version_update*/,
      base::RepeatingCallback<void(const UpdateState&)> /*state_update*/,
      base::OnceCallback<void(Result)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UpdateService::Result::kInactive));
  }

  void UpdateAll(
      base::RepeatingCallback<void(const UpdateState&)> /*state_update*/,
      base::OnceCallback<void(Result)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UpdateService::Result::kInactive));
  }

  void Install(
      const RegistrationRequest& /*registration*/,
      const std::string& /*client_install_data*/,
      const std::string& /*install_data_index*/,
      Priority /*priority*/,
      base::RepeatingCallback<void(const UpdateState&)> /*state_update*/,
      base::OnceCallback<void(Result)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UpdateService::Result::kInactive));
  }

  void CancelInstalls(const std::string& /*app_id*/) override {
    VLOG(1) << __func__ << " (Inactive)";
  }

  void RunInstaller(
      const std::string& /*app_id*/,
      const base::FilePath& /*installer_path*/,
      const std::string& /*install_args*/,
      const std::string& /*install_data*/,
      const std::string& /*install_settings*/,
      base::RepeatingCallback<void(const UpdateState&)> /*state_update*/,
      base::OnceCallback<void(Result)> callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UpdateService::Result::kInactive));
  }

 private:
  ~UpdateServiceImplInactive() override = default;
};

}  // namespace

scoped_refptr<UpdateService> MakeInactiveUpdateService() {
  return base::MakeRefCounted<UpdateServiceImplInactive>();
}

}  // namespace updater
