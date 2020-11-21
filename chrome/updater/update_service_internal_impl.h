// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/update_service_internal.h"

namespace update_client {
enum class Error;
class UpdateClient;
}  // namespace update_client

namespace updater {

class Configurator;
class PersistedData;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceInternalImpl : public UpdateServiceInternal {
 public:
  explicit UpdateServiceInternalImpl(
      scoped_refptr<updater::Configurator> config);

  // Overrides for updater::UpdateServiceInternal.
  void Run(base::OnceClosure callback) override;
  void InitializeUpdateService(base::OnceClosure callback) override;

  void Uninitialize() override;

 private:
  ~UpdateServiceInternalImpl() override;

  SEQUENCE_CHECKER(sequence_checker_);

  struct AppInfo {
    AppInfo(const std::string& app_id,
            const base::Version& app_version,
            const base::FilePath& ecp)
        : app_id_(app_id), app_version_(app_version), ecp_(ecp) {}
    std::string app_id_;
    base::Version app_version_;
    base::FilePath ecp_;
  };

  struct PingInfo {
    PingInfo(const std::string& app_id,
             const base::Version& app_version,
             int ping_reason)
        : app_id_(app_id),
          app_version_(app_version),
          ping_reason_(ping_reason) {}
    std::string app_id_;
    base::Version app_version_;
    int ping_reason_;
  };

  // Checks for updates of all registered applications if it has been longer
  // than the last check time by NextCheckDelay() amount defined in the config.
  void MaybeCheckForUpdates();

  // Returns a list of apps registered with the updater.
  std::vector<AppInfo> GetRegisteredApps();

  void UnregisterMissingAppsDone();

  // Provides a way to remove apps from the persisted data if the app is no
  // longer installed on the machine.
  void UnregisterMissingApps(const std::vector<AppInfo>& apps);

  // After an uninstall ping has been processed, reduces the number of pings
  // that we need to wait on before checking for updates.
  void UninstallPingSent(update_client::Error error);

  // Returns true if there are uninstall ping tasks which haven't finished.
  // Returns false if |number_of_pings_remaining_| is 0.
  bool WaitingOnUninstallPings() const;

  // Returns a list of apps that need to be unregistered.
  std::vector<UpdateServiceInternalImpl::PingInfo> GetAppIDsToRemove(
      const std::vector<AppInfo>& apps);

  // Unregisters the apps in |app_ids_to_remove| and starts an update check
  // if necessary.
  void RemoveAppIDsAndSendUninstallPings(
      const std::vector<PingInfo>& app_ids_to_remove);

  scoped_refptr<updater::Configurator> config_;
  scoped_refptr<updater::PersistedData> persisted_data_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  base::OnceClosure callback_;
  int number_of_pings_remaining_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_
