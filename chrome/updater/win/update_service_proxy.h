// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UPDATE_SERVICE_PROXY_H_
#define CHROME_UPDATER_WIN_UPDATE_SERVICE_PROXY_H_

#include <windows.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class Version;
}  // namespace base

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {

struct RegistrationRequest;
struct RegistrationResponse;

// There are two threads running the code in this module. The main sequence is
// bound to one thread, all the COM calls, inbound and outbound occur on the
// second thread which serializes the tasks and the invocations originating
// in the COM RPC runtime, which arrive sequentially but they are not sequenced
// through the task runner.

// All public functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  explicit UpdateServiceProxy(UpdaterScope updater_scope);

  // Overrides for updater::UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override;
  void FetchPolicies(base::OnceCallback<void(int)> callback) override;
  void RegisterApp(const RegistrationRequest& request,
                   RegisterAppCallback callback) override;
  void GetAppStates(
      base::OnceCallback<void(const std::vector<AppState>&)>) override;
  void RunPeriodicTasks(base::OnceClosure callback) override;
  void UpdateAll(StateChangeCallback state_update, Callback callback) override;
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              Priority priority,
              PolicySameVersionUpdate policy_same_version_update,
              StateChangeCallback state_update,
              Callback callback) override;
  void Install(const RegistrationRequest& registration,
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
  void Uninitialize() override;

 private:
  ~UpdateServiceProxy() override;

  // These functions run on the `com_task_runner_`. `prev_hr` contains the
  // result of the previous callback invocation in a `Then` chain.
  HRESULT InitializeSTA();
  void UninitializeOnSTA();
  void GetVersionOnSTA(base::OnceCallback<void(const base::Version&)> callback,
                       HRESULT prev_hr);
  void FetchPoliciesOnSTA(base::OnceCallback<void(int)> callback,
                          HRESULT prev_hr);
  void RegisterAppOnSTA(const RegistrationRequest& request,
                        RegisterAppCallback callback,
                        HRESULT prev_hr);
  void GetAppStatesSTA(base::OnceCallback<void(const std::vector<AppState>&)>,
                       HRESULT prev_hr);
  void RunPeriodicTasksOnSTA(base::OnceClosure callback, HRESULT prev_hr);
  void UpdateAllOnSTA(StateChangeCallback state_update,
                      Callback callback,
                      HRESULT prev_hr);
  void UpdateOnSTA(const std::string& app_id,
                   const std::string& install_data_index,
                   UpdateService::Priority priority,
                   PolicySameVersionUpdate policy_same_version_update,
                   StateChangeCallback state_update,
                   Callback callback,
                   HRESULT prev_hr);

  void InstallOnSTA(const RegistrationRequest& registration,
                    const std::string& install_data_index,
                    Priority priority,
                    StateChangeCallback state_update,
                    Callback callback,
                    HRESULT prev_hr);

  void CancelInstallsOnSTA(const std::string& app_id, HRESULT prev_hr);

  void RunInstallerOnSTA(const std::string& app_id,
                         const base::FilePath& installer_path,
                         const std::string& install_args,
                         const std::string& install_data,
                         const std::string& install_settings,
                         StateChangeCallback state_update,
                         Callback callback,
                         HRESULT prev_hr);

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_main_);

  UpdaterScope scope_;

  // Bound to the main sequence.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Runs the tasks which involve outbound COM calls and inbound COM callbacks.
  // This task runner is thread-affine with the COM STA.
  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;

  // Updater COM server instance owned by the STA. That means the instance must
  // be created and destroyed on the com_task_runner_.
  Microsoft::WRL::ComPtr<IUpdater> updater_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UPDATE_SERVICE_PROXY_H_
