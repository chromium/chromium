// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_UPDATE_SERVICE_STUB_WIN_H_
#define CHROME_UPDATER_APP_SERVER_WIN_UPDATE_SERVICE_STUB_WIN_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service.h"

namespace updater {

// Receives calls from the client and delegates them to an UpdateService
// implementation. Before each call to the UpdateService implementation is
// invoked, `task_start_listener` is called. And after each call to the
// UpdateService implementation has completed, `task_end_listener` is called.
class UpdateServiceStubWin : public UpdateService {
 public:
  // Creates an `UpdateServiceStubWin` which forwards calls to `impl`.
  UpdateServiceStubWin(scoped_refptr<updater::UpdateService> impl,
                       base::RepeatingClosure task_start_listener,
                       base::RepeatingClosure task_end_listener);
  UpdateServiceStubWin(const UpdateServiceStubWin&) = delete;
  UpdateServiceStubWin& operator=(const UpdateServiceStubWin&) = delete;

  // updater::UpdateService overrides.
  void GetVersion(base::OnceCallback<void(const base::Version&)>) override;
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
  ~UpdateServiceStubWin() override;

  scoped_refptr<updater::UpdateService> impl_;
  base::RepeatingClosure task_start_listener_;
  base::RepeatingClosure task_end_listener_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_UPDATE_SERVICE_STUB_WIN_H_
