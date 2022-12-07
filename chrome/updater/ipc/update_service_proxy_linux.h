// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_LINUX_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_LINUX_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom-forward.h"
#include "chrome/updater/update_service.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace mojo {
class IsolatedConnection;
}

namespace updater {

class UpdateServiceProxyImpl;
enum class UpdaterScope;
struct RegistrationRequest;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  // Create an UpdateServiceProxy which is not bound to a remote. It will search
  // for and establish a connection in a background sequence.
  explicit UpdateServiceProxy(UpdaterScope scope);

  // Create an UpdateServiceProxy bound to the provided Mojo remote. The
  // lifetime of the connection to the remote process is handled by
  // `connection` and is bound to the lifetime of this instance.
  UpdateServiceProxy(UpdaterScope scope,
                     std::unique_ptr<mojo::IsolatedConnection> connection,
                     mojo::Remote<mojom::UpdateService> remote);

  // Overrides for updater::UpdateService.
  // Note: Provided OnceCallbacks are wrapped with
  // `mojo::WrapCallbackWithDefaultInvokeIfNotRun` to avoid deadlock if
  // connection to the remote is broken.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override;
  void FetchPolicies(base::OnceCallback<void(int)> callback) override;
  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) override;
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
  void EnsureConnecting();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceProxyImpl> impl_;
};

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateService> remote);

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_LINUX_H_
