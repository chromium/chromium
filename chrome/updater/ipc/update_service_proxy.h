// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/updater/update_service.h"

#if BUILDFLAG(IS_POSIX)
#include "chrome/updater/ipc/update_service_proxy_posix.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/updater/ipc/update_service_proxy_win.h"
#endif

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace updater {

struct RegistrationRequest;

// UpdateServiceProxy is an UpdateService that connects to the active updater
// instance server and runs its implementation of UpdateService methods. All
// functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  explicit UpdateServiceProxy(scoped_refptr<UpdateServiceProxyImpl> proxy);

  // Overrides for updater::UpdateService.
  // UpdateServiceProxy will not be destroyed while these calls are
  // outstanding; the caller need not retain a ref.
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
  ~UpdateServiceProxy() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceProxyImpl> proxy_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_H_
