// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service.h"

namespace base {
class Version;
}

namespace updater {

struct RegistrationRequest;
enum class UpdaterScope;
class UpdateServiceProxyImpl;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  explicit UpdateServiceProxy(UpdaterScope updater_scope);

  // Overrides for updater::UpdateService.
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

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceProxyImpl> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_WIN_H_
