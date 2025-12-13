// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_IMPL_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_IMPL_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace updater {

// Implementations of `UpdateServiceProxyImpl` connect to the active updater
// instance server and run its implementation of UpdateService methods. All
// functions and callbacks must be called on the same sequence.
class UpdateServiceProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImpl> {
 public:
  virtual void GetVersion(
      base::OnceCallback<void(base::expected<base::Version, RpcError>)>
          callback) = 0;
  virtual void FetchPolicies(
      policy::PolicyFetchReason reason,
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) = 0;
  virtual void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) = 0;
  virtual void GetAppStates(
      base::OnceCallback<void(
          base::expected<std::vector<UpdateService::AppState>, RpcError>)>) = 0;
  virtual void RunPeriodicTasks(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) = 0;
  virtual void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) = 0;
  virtual void Update(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) = 0;
  virtual void UpdateAll(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) = 0;
  virtual void Install(
      const RegistrationRequest& registration,
      const std::string& client_install_data,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) = 0;
  virtual void CancelInstalls(const std::string& app_id) = 0;
  virtual void RunInstaller(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdateServiceProxyImpl>;

  virtual ~UpdateServiceProxyImpl() = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_IMPL_H_
