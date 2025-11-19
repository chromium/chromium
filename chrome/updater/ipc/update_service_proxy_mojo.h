// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_MOJO_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_MOJO_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/updater/ipc/update_service_proxy_impl.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_WIN)
#include <wrl/client.h>
#endif  // BUILDFLAG(IS_WIN)

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace mojo {
class IsolatedConnection;
class PlatformChannelEndpoint;
}  // namespace mojo

namespace updater {

// UpdateServiceProxyImpl connects to the active updater instance server and
// runs its implementation of UpdateService methods. All functions and
// callbacks must be called on the same sequence.
class UpdateServiceProxyMojoImpl : public UpdateServiceProxyImpl {
 public:
  // Create an UpdateServiceProxyMojoImpl which is not bound to a remote. It
  // will search for and establish a connection in a background sequence.
  UpdateServiceProxyMojoImpl(UpdaterScope scope, base::TimeDelta timeout);

  // Create an UpdateServiceProxyMojoImpl bound to the provided Mojo remote. The
  // lifetime of the connection to the remote process is handled by
  // `connection` and is bound to the lifetime of this instance.
  UpdateServiceProxyMojoImpl(
      UpdaterScope scope,
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::Remote<mojom::UpdateService> remote);

  // Note: Provided OnceCallbacks are wrapped with
  // `mojo::WrapCallbackWithDefaultInvokeIfNotRun` to avoid deadlock if
  // connection to the remote is broken, and UpdateServiceProxyMojoImpl will not
  // be destroyed while these calls are outstanding; the caller need not retain
  // a ref.
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
  ~UpdateServiceProxyMojoImpl() override;

#if BUILDFLAG(IS_WIN)
  void OnConnected(mojo::PendingReceiver<mojom::UpdateService> pending_receiver,
                   std::optional<mojo::PlatformChannelEndpoint> endpoint,
                   Microsoft::WRL::ComPtr<IUnknown> server);
#else   // BUILDFLAG(IS_WIN)
  void OnConnected(mojo::PendingReceiver<mojom::UpdateService> pending_receiver,
                   std::optional<mojo::PlatformChannelEndpoint> endpoint);
#endif  // BUILDFLAG(IS_WIN)

  void OnDisconnected();
  void EnsureConnecting();

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  base::TimeDelta get_version_timeout_;
  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateService> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<IUnknown> server_;
#endif  // BUILDFLAG(IS_WIN)

  base::WeakPtrFactory<UpdateServiceProxyMojoImpl> weak_factory_{this};
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_MOJO_H_
