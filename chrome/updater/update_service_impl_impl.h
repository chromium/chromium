// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_IMPL_IMPL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_IMPL_IMPL_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/updater/update_service.h"

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
class PolicyService;
struct RegistrationRequest;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceImplImpl : public UpdateService {
 public:
  explicit UpdateServiceImplImpl(scoped_refptr<Configurator> config);

  // Overrides for updater::UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override;
  void FetchPolicies(base::OnceCallback<void(int)> callback) override;
  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) override;
  void GetAppStates(
      base::OnceCallback<void(const std::vector<AppState>&)>) override;
  void RunPeriodicTasks(base::OnceClosure callback) override;
  void CheckForUpdate(
      const std::string& app_id,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) override;
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              Priority priority,
              PolicySameVersionUpdate policy_same_version_update,
              base::RepeatingCallback<void(const UpdateState&)> state_update,
              base::OnceCallback<void(Result)> callback) override;
  void UpdateAll(base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback) override;
  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               Priority priority,
               base::RepeatingCallback<void(const UpdateState&)> state_update,
               base::OnceCallback<void(Result)> callback) override;
  void CancelInstalls(const std::string& app_id) override;
  void RunInstaller(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) override;

 private:
  ~UpdateServiceImplImpl() override;

  // Runs the task at the head of `tasks_`, if any.
  void TaskStart();

  // Pops `tasks_`, and calls TaskStart.
  void TaskDone();

  // Installs applications in the wake task based on the ForceInstalls policy.
  void ForceInstall(
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback);

  bool IsUpdateDisabledByPolicy(const std::string& app_id,
                                Priority priority,
                                bool is_install,
                                int& policy);
  void HandleUpdateDisabledByPolicy(
      const std::string& app_id,
      int policy,
      bool is_install,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback);

  void OnShouldBlockCheckForUpdateForMeteredNetwork(
      const std::string& app_id,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback,
      bool update_blocked);

  void OnShouldBlockUpdateForMeteredNetwork(
      const std::vector<std::string>& app_ids,
      const base::flat_map<std::string, std::string>& app_client_install_data,
      const base::flat_map<std::string, std::string>& app_install_data_index,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback,
      bool update_blocked);

  void OnShouldBlockForceInstallForMeteredNetwork(
      const std::vector<std::string>& app_ids,
      const base::flat_map<std::string, std::string>& app_client_install_data,
      const base::flat_map<std::string, std::string>& app_install_data_index,
      PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback,
      bool update_blocked);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<Configurator> config_;
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
    scoped_refptr<PolicyService> policy_service,
    crx_file::VerifierFormat verifier_format,
    scoped_refptr<PersistedData> persisted_data,
    const base::flat_map<std::string, std::string>& app_client_install_data,
    const base::flat_map<std::string, std::string>& app_install_data_index,
    const std::string& install_source,
    UpdateService::Priority priority,
    bool update_blocked,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    const std::vector<std::string>& ids,
    base::OnceCallback<
        void(const std::vector<std::optional<update_client::CrxComponent>>&)>
        callback);

#if BUILDFLAG(IS_WIN)
std::string GetInstallerText(UpdateService::ErrorCategory error_category,
                             int error_code,
                             int extra_code);
#endif  // BUILDFLAG(IS_WIN)
}  // namespace internal

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_IMPL_IMPL_H_
