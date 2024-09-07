// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_impl_impl.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace updater {
class Configurator;
struct RegistrationRequest;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceImpl : public UpdateService {
 public:
  UpdateServiceImpl(UpdaterScope scope, scoped_refptr<Configurator> config);

  // Overrides for updater::UpdateService.
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
  ~UpdateServiceImpl() override;

  void AcceptEula();
  bool IsEulaAccepted();

  bool IsOemMode();

  SEQUENCE_CHECKER(sequence_checker_);

  UpdaterScope scope_;
  scoped_refptr<Configurator> config_;
  scoped_refptr<UpdateServiceImplImpl> delegate_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_
