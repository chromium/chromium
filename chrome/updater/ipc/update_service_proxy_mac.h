// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_MAC_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_MAC_H_

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

@class CRUUpdateServiceProxyImpl;

namespace base {
class SequencedTaskRunner;
class Version;
}  // namespace base

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {

// All functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  UpdateServiceProxy(UpdaterScope scope,
                     const base::TimeDelta& get_version_timeout);

  // Overrides for UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override;
  void FetchPolicies(base::OnceCallback<void(int)> callback) override;
  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) override;
  void GetAppStates(
      base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>)
      override;
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

  // Reset invalidates the existing connection, causing error callbacks to fire,
  // and reinitializes it for further use.
  void Reset();

  SEQUENCE_CHECKER(sequence_checker_);

  UpdaterScope scope_;
  base::TimeDelta get_version_timeout_;
  base::scoped_nsobject<CRUUpdateServiceProxyImpl> client_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_PROXY_MAC_H_
