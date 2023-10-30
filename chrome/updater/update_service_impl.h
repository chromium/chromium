// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/updater/update_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class Version;
}  // namespace base

namespace update_client {
class UpdateClient;
}  // namespace update_client

namespace updater {
class Configurator;
class PersistedData;
struct RegistrationRequest;

using AppClientInstallData = base::flat_map<std::string, std::string>;
using AppInstallDataIndex = base::flat_map<std::string, std::string>;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceImpl : public UpdateService {
 public:
  explicit UpdateServiceImpl(scoped_refptr<Configurator> config);

  // Overrides for updater::UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override;
  void FetchPolicies(base::OnceCallback<void(int)> callback) override;
  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) override;
  void GetAppStates(
      base::OnceCallback<void(const std::vector<AppState>&)>) override;
  void RunPeriodicTasks(base::OnceClosure callback) override;
  void CheckForUpdate(const std::string& app_id,
                      Priority priority,
                      PolicySameVersionUpdate policy_same_version_update,
                      StateChangeCallback state_update,
                      Callback callback) override;
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              Priority priority,
              PolicySameVersionUpdate policy_same_version_update,
              StateChangeCallback state_update,
              Callback callback) override;
  void UpdateAll(StateChangeCallback state_update, Callback callback) override;
  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               Priority priority,
               StateChangeCallback state_update,
               Callback callback) override;
  void CancelInstalls(const std::string& app_id) override;
  void RunInstaller(const std::string& app_id,
                    const base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    StateChangeCallback state_update,
                    Callback callback) override;

 private:
  ~UpdateServiceImpl() override;

  // Runs the task at the head of `tasks_`, if any.
  void TaskStart();

  // Pops `tasks_`, and calls TaskStart.
  void TaskDone();

  // Installs applications in the wake task based on the ForceInstalls policy.
  void ForceInstall(StateChangeCallback state_update, Callback callback);

  bool IsUpdateDisabledByPolicy(const std::string& app_id,
                                Priority priority,
                                bool is_install,
                                int& policy);
  void HandleUpdateDisabledByPolicy(const std::string& app_id,
                                    int policy,
                                    bool is_install,
                                    StateChangeCallback state_update,
                                    Callback callback);

  void OnShouldBlockCheckForUpdateForMeteredNetwork(
      const std::string& app_id,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      StateChangeCallback state_update,
      Callback callback,
      bool update_blocked);

  void OnShouldBlockUpdateForMeteredNetwork(
      const std::vector<std::string>& app_ids,
      const AppClientInstallData& app_client_install_data,
      const AppInstallDataIndex& app_install_data_index,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      StateChangeCallback state_update,
      Callback callback,
      bool update_blocked);

  void OnShouldBlockForceInstallForMeteredNetwork(
      const std::vector<std::string>& app_ids,
      const AppClientInstallData& app_client_install_data,
      const AppInstallDataIndex& app_install_data_index,
      PolicySameVersionUpdate policy_same_version_update,
      StateChangeCallback state_update,
      Callback callback,
      bool update_blocked);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<Configurator> config_;
  scoped_refptr<PersistedData> persisted_data_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<update_client::UpdateClient> update_client_;

  // The queue serializes periodic task execution.
  base::queue<base::OnceClosure> tasks_;

  // Cancellation callbacks, keyed by appid.
  std::multimap<std::string, base::RepeatingClosure> cancellation_callbacks_;
};

namespace internal {
UpdateService::Result ToResult(update_client::Error error);

void GetComponents(
    scoped_refptr<Configurator> config,
    scoped_refptr<PersistedData> persisted_data,
    const AppClientInstallData& app_client_install_data,
    const AppInstallDataIndex& app_install_data_index,
    UpdateService::Priority priority,
    bool update_blocked,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    const std::vector<std::string>& ids,
    base::OnceCallback<
        void(const std::vector<absl::optional<update_client::CrxComponent>>&)>
        callback);

#if BUILDFLAG(IS_WIN)
std::string GetInstallerText(UpdateService::ErrorCategory error_category,
                             int error_code,
                             bool is_installer_error = false);
#endif  // BUILDFLAG(IS_WIN)
}  // namespace internal

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_
