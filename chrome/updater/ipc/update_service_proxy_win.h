// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_

#include <windows.h>

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/updater/update_service.h"

namespace base {
class Version;
}

namespace updater {

using RpcError = HRESULT;

struct RegistrationRequest;
enum class UpdaterScope;
class UpdateServiceProxyImplImpl;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImpl> {
 public:
  explicit UpdateServiceProxyImpl(UpdaterScope updater_scope);

  // UpdateServiceProxyImpl will not be destroyed while these calls are
  // outstanding; the caller need not retain a ref.
  void GetVersion(
      base::OnceCallback<void(base::expected<base::Version, RpcError>)>
          callback);
  void FetchPolicies(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback);
  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(base::expected<int, RpcError>)> callback);
  void GetAppStates(
      base::OnceCallback<void(
          base::expected<std::vector<UpdateService::AppState>, RpcError>)>);
  void RunPeriodicTasks(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback);
  void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback);
  void Update(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback);
  void UpdateAll(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback);
  void Install(
      const RegistrationRequest& registration,
      const std::string& client_install_data,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback);
  void CancelInstalls(const std::string& app_id);
  void RunInstaller(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback);

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceProxyImpl>;
  ~UpdateServiceProxyImpl();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceProxyImplImpl> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_
