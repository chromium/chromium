// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_LINUX_UPDATE_SERVICE_STUB_H_
#define CHROME_UPDATER_APP_SERVER_LINUX_UPDATE_SERVICE_STUB_H_

#include "chrome/updater/app/server/linux/mojom/updater_service.mojom.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/update_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace updater {

// Receives RPC calls from the client and delegates them to an UpdateService.
class UpdateServiceStub : public mojom::UpdateService {
 public:
  UpdateServiceStub(mojo::PendingReceiver<mojom::UpdateService> receiver,
                    scoped_refptr<updater::UpdateService> impl);
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
  void UpdateAll(UpdateAllCallback callback) override;
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              UpdateService::Priority priority,
              UpdateService::PolicySameVersionUpdate policy_same_version_update,
              UpdateCallback callback) override;
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

 private:
  mojo::Receiver<updater::mojom::UpdateService> receiver_;
  scoped_refptr<updater::UpdateService> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_LINUX_UPDATE_SERVICE_STUB_H_
