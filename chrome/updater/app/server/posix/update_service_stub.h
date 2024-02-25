// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_POSIX_UPDATE_SERVICE_STUB_H_
#define CHROME_UPDATER_APP_SERVER_POSIX_UPDATE_SERVICE_STUB_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

namespace updater {

// Receives RPC calls from the client and delegates them to an UpdateService.
// The stub creates and manages a `NamedMojoIpcServer` to listen for and broker
// new Mojo connections with clients.
class UpdateServiceStub : public mojom::UpdateService {
 public:
  // Creates an `UpdateServiceStub` which forwards calls to `impl`. Opens a
  // `NamedMojoIpcServer` which listens on a socket whose name is decided by
  // `scope`.
  UpdateServiceStub(scoped_refptr<updater::UpdateService> impl,
                    UpdaterScope scope,
                    base::RepeatingClosure task_start_listener,
                    base::RepeatingClosure task_end_listener);
  UpdateServiceStub(const UpdateServiceStub&) = delete;
  UpdateServiceStub& operator=(const UpdateServiceStub&) = delete;
  ~UpdateServiceStub() override;

  // updater::mojom::UpdateService
  void GetVersion(GetVersionCallback callback) override;
  void FetchPolicies(FetchPoliciesCallback callback) override;
  void RegisterApp(mojom::RegistrationRequestPtr request,
                   RegisterAppCallback callback) override;
  void GetAppStates(GetAppStatesCallback callback) override;
  void RunPeriodicTasks(RunPeriodicTasksCallback callback) override;
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              UpdateService::Priority priority,
              UpdateService::PolicySameVersionUpdate policy_same_version_update,
              bool do_update_check_only,
              UpdateCallback callback) override;
  void UpdateAll(UpdateAllCallback callback) override;
  void Install(mojom::RegistrationRequestPtr registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               UpdateService::Priority priority,
               InstallCallback callback) override;
  void CancelInstalls(const std::string& app_id) override;
  void RunInstaller(const std::string& app_id,
                    const ::base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    RunInstallerCallback callback) override;
  void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      UpdateCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdaterIPCTestCase, AllRpcsComplete);
  FRIEND_TEST_ALL_PREFIXES(KSAdminTest, Register);
  // Creates an `UpdateServiceStub` and invokes a callback when the server
  // endpoint is created. This is useful for tests.
  UpdateServiceStub(
      scoped_refptr<updater::UpdateService> impl,
      UpdaterScope scope,
      base::RepeatingClosure task_start_listener,
      base::RepeatingClosure task_end_listener,
      base::RepeatingClosure endpoint_created_listener_for_testing);

  std::unique_ptr<mojom::UpdateService> filter_;
  named_mojo_ipc_server::NamedMojoIpcServer<mojom::UpdateService> server_;
  scoped_refptr<updater::UpdateService> impl_;
  base::RepeatingClosure task_start_listener_;
  base::RepeatingClosure task_end_listener_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_POSIX_UPDATE_SERVICE_STUB_H_
