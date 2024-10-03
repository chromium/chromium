// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_POSIX_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_POSIX_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace mojo {
class IsolatedConnection;
class PlatformChannelEndpoint;
}  // namespace mojo

namespace updater {

using RpcError = int;

struct RegistrationRequest;

// UpdateServiceProxyImpl connects to the active updater instance server and
// runs its implementation of UpdateService methods. All functions and
// callbacks must be called on the same sequence.
class UpdateServiceProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImpl> {
 public:
  // Create an UpdateServiceProxyImpl which is not bound to a remote. It will
  // search for and establish a connection in a background sequence.
  UpdateServiceProxyImpl(UpdaterScope scope, base::TimeDelta timeout);

  // Create an UpdateServiceProxyImpl bound to the provided Mojo remote. The
  // lifetime of the connection to the remote process is handled by
  // `connection` and is bound to the lifetime of this instance.
  UpdateServiceProxyImpl(UpdaterScope scope,
                         std::unique_ptr<mojo::IsolatedConnection> connection,
                         mojo::Remote<mojom::UpdateService> remote);

  // Note: Provided OnceCallbacks are wrapped with
  // `mojo::WrapCallbackWithDefaultInvokeIfNotRun` to avoid deadlock if
  // connection to the remote is broken, and UpdateServiceProxyImpl will not be
  // destroyed while these calls are outstanding; the caller need not retain a
  // ref.
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
  void OnConnected(mojo::PendingReceiver<mojom::UpdateService> pending_receiver,
                   std::optional<mojo::PlatformChannelEndpoint> endpoint);
  void OnDisconnected();
  void EnsureConnecting();

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  base::TimeDelta get_version_timeout_;
  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateService> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<UpdateServiceProxyImpl> weak_factory_{this};
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_POSIX_H_
