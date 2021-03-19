// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CHECK_FOR_UPDATES_TASK_H_
#define CHROME_UPDATER_CHECK_FOR_UPDATES_TASK_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/update_service.h"

namespace update_client {
class Configurator;
class UpdateClient;
enum class Error;
}  // namespace update_client

namespace updater {
class PersistedData;

class CheckForUpdatesTask
    : public base::RefCountedThreadSafe<CheckForUpdatesTask> {
 public:
  CheckForUpdatesTask(
      scoped_refptr<update_client::Configurator> config,
      base::OnceCallback<void(UpdateService::Callback)> update_checker,
      base::OnceClosure callback);
  void Run();

  // Provides a way to remove apps from the persisted data if the app is no
  // longer installed on the machine.
  void UnregisterMissingApps();

 private:
  friend class base::RefCountedThreadSafe<CheckForUpdatesTask>;
  virtual ~CheckForUpdatesTask();

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

  // Returns a list of apps registered with the updater.
  std::vector<AppInfo> GetRegisteredApps();

  // Returns a list of apps that need to be unregistered.
  std::vector<PingInfo> GetAppIDsToRemove(const std::vector<AppInfo>& apps);

  // Callback to run after a `MaybeCheckForUpdates` has finished.
  // Triggers the completion of the whole task.
  void MaybeCheckForUpdatesDone();

  // Unregisters the apps in `app_ids_to_remove` and starts an update check
  // if necessary.
  void RemoveAppIDsAndSendUninstallPings(
      const std::vector<PingInfo>& app_ids_to_remove);

  // After an uninstall ping has been processed, reduces the number of pings
  // that we need to wait on before checking for updates.
  void UninstallPingSent(update_client::Error error);

  // Returns true if there are uninstall ping tasks which haven't finished.
  // Returns false if `number_of_pings_remaining_` is 0.
  // `number_of_pings_remaining_` is only updated on the tasks's sequence.
  bool WaitingOnUninstallPings() const;

  // Checks for updates of all registered applications if it has been longer
  // than the last check time by NextCheckDelay() amount defined in the
  // config.
  void MaybeCheckForUpdates();

  // Callback to run after `UnregisterMissingApps` has finished.
  // Triggers `MaybeCheckForUpdates`.
  void UnregisterMissingAppsDone();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<update_client::Configurator> config_;
  base::OnceCallback<void(UpdateService::Callback)> update_checker_;
  scoped_refptr<updater::PersistedData> persisted_data_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  base::OnceClosure callback_;
  int number_of_pings_remaining_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CHECK_FOR_UPDATES_TASK_H_
