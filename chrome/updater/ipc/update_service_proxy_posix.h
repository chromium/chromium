// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_POSIX_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_POSIX_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
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

struct RegistrationRequest;

// UpdateServiceProxy is an UpdateService that connects to the active updater
// instance server and runs its implementation of UpdateService methods. All
// functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  // Create an UpdateServiceProxy which is not bound to a remote. It will search
  // for and establish a connection in a background sequence.
  UpdateServiceProxy(UpdaterScope scope, const base::TimeDelta& timeout);

  // Create an UpdateServiceProxy bound to the provided Mojo remote. The
  // lifetime of the connection to the remote process is handled by
  // `connection` and is bound to the lifetime of this instance.
  UpdateServiceProxy(UpdaterScope scope,
                     std::unique_ptr<mojo::IsolatedConnection> connection,
                     mojo::Remote<mojom::UpdateService> remote);

  // Overrides for updater::UpdateService.
  // Note: Provided OnceCallbacks are wrapped with
  // `mojo::WrapCallbackWithDefaultInvokeIfNotRun` to avoid deadlock if
  // connection to the remote is broken, and UpdateServiceProxy will not be
  // destroyed while these calls are outstanding; the caller need not retain a
  // ref.
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
  ~UpdateServiceProxy() override;
  void OnConnected(mojo::PendingReceiver<mojom::UpdateService> pending_receiver,
                   absl::optional<mojo::PlatformChannelEndpoint> endpoint);
  void OnDisconnected();
  void EnsureConnecting();

  void GetVersionDone(base::OnceCallback<void(const base::Version&)> callback,
                      const base::Version& result);
  void FetchPoliciesDone(base::OnceCallback<void(int)> callback, int result);
  void RegisterAppDone(base::OnceCallback<void(int)> callback, int result);
  void GetAppStatesDone(base::OnceCallback<void(const std::vector<AppState>&)>,
                        const std::vector<AppState>& results);
  void RunPeriodicTasksDone(base::OnceClosure callback);
  void CheckForUpdateDone(Callback callback, Result result);
  void UpdateDone(Callback callback, Result result);
  void UpdateAllDone(Callback callback, Result result);
  void InstallDone(Callback callback, Result result);
  void RunInstallerDone(Callback callback, Result result);

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  base::TimeDelta get_version_timeout_;
  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateService> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<UpdateServiceProxy> weak_factory_{this};
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_POSIX_H_
