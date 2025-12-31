// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_

#include <windows.h>

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/updater/ipc/update_service_proxy_impl.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

namespace base {
class Version;
}

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace updater {

enum class UpdaterScope;
class UpdateServiceProxyImplImpl;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceProxyWinImpl : public UpdateServiceProxyImpl {
 public:
  explicit UpdateServiceProxyWinImpl(UpdaterScope updater_scope);

  // UpdateServiceProxyWinImpl will not be destroyed while these calls are
  // outstanding; the caller need not retain a ref.
  void GetVersion(
      base::OnceCallback<void(base::expected<base::Version, RpcError>)>
          callback) override;
  void FetchPolicies(policy::PolicyFetchReason reason,
                     base::OnceCallback<void(base::expected<int, RpcError>)>
                         callback) override;
  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(base::expected<int, RpcError>)>
                       callback) override;
  void GetAppStates(
      base::OnceCallback<
          void(base::expected<std::vector<UpdateService::AppState>, RpcError>)>)
      override;
  void RunPeriodicTasks(base::OnceCallback<void(base::expected<int, RpcError>)>
                            callback) override;
  void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) override;
  void Update(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) override;
  void UpdateAll(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) override;
  void Install(
      const RegistrationRequest& registration,
      const std::string& client_install_data,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) override;
  void CancelInstalls(const std::string& app_id) override;
  void RunInstaller(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) override;

 private:
  ~UpdateServiceProxyWinImpl() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceProxyImplImpl> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_
