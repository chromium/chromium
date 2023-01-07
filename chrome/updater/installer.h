// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_INSTALLER_H_
#define CHROME_UPDATER_INSTALLER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/update_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}

namespace updater {

struct AppInfo {
  AppInfo(const UpdaterScope scope,
          const std::string& app_id,
          const std::string& ap,
          const base::Version& app_version,
          const base::FilePath& ecp);
  AppInfo(const AppInfo&);
  AppInfo& operator=(const AppInfo&);
  ~AppInfo();

  UpdaterScope scope;
  std::string app_id;
  std::string ap;
  base::Version version;
  base::FilePath ecp;
};

using AppInstallerResult = update_client::CrxInstaller::Result;
using InstallProgressCallback = update_client::CrxInstaller::ProgressCallback;

// Runs an app installer.
//   The file `server_install_data` contains additional application-specific
// install configuration parameters extracted either from the update response or
// the app manifest.
AppInstallerResult RunApplicationInstaller(
    const AppInfo& app_info,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const absl::optional<base::FilePath>& server_install_data,
    const base::TimeDelta& timeout,
    InstallProgressCallback progress_callback);

// Manages the install of one application. Some of the functions of this
// class are blocking and can't be invoked on the main sequence.
//
// If the application installer completes with success, then the following
// post conditions are true: |update_client| updates persisted data in prefs,
// the CRX is installed in a versioned directory in apps/app_id/version,
// the application is considered to be registered for updates, and the
// application installed version matches the version recorded in prefs.
//
// If installing the CRX fails, or the installer fails, then prefs is not
// going to be updated. There will be some files left on the file system, which
// are going to be cleaned up next time the installer runs.
//
// Install directories not matching the |pv| version are lazy-deleted.
class Installer final : public update_client::CrxInstaller {
 public:
  Installer(const std::string& app_id,
            const std::string& client_install_data,
            const std::string& install_data_index,
            const std::string& target_channel,
            const std::string& target_version_prefix,
            bool rollback_allowed,
            bool update_disabled,
            UpdateService::PolicySameVersionUpdate policy_same_version_update,
            scoped_refptr<PersistedData> persisted_data,
            crx_file::VerifierFormat crx_verifier_format);
  Installer(const Installer&) = delete;
  Installer& operator=(const Installer&) = delete;

  std::string app_id() const { return app_id_; }

  // Returns a CrxComponent instance that describes the current install
  // state of the app. Updates the values of |pv_| and the |fingerprint_| with
  // the persisted values in prefs.
  //
  // Callers should only invoke this function when handling a CrxDataCallback
  // callback from update_client::Install or from update_client::Update. This
  // ensure that prefs has been updated with the most recent values, including
  // |pv| and |fingerprint|.
  update_client::CrxComponent MakeCrxComponent();

 private:
  ~Installer() override;

  // Overrides from update_client::CrxInstaller.
  void OnUpdateError(int error) override;
  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               std::unique_ptr<InstallParams> install_params,
               ProgressCallback progress_callback,
               Callback callback) override;
  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;
  bool Uninstall() override;

  Result InstallHelper(const base::FilePath& unpack_path,
                       std::unique_ptr<InstallParams> install_params,
                       ProgressCallback progress_callback);

  // Runs the installer code with sync primitives to allow the code to
  // create processes and wait for them to exit.
  void InstallWithSyncPrimitives(const base::FilePath& unpack_path,
                                 std::unique_ptr<InstallParams> install_params,
                                 ProgressCallback progress_callback,
                                 Callback callback);

  // Deletes recursively the install paths not matching the |pv_| version.
  void DeleteOlderInstallPaths();

  // Returns an install directory matching the |pv_| version.
  absl::optional<base::FilePath> GetCurrentInstallDir() const;

  SEQUENCE_CHECKER(sequence_checker_);

  UpdaterScope updater_scope_;

  const std::string app_id_;
  const std::string client_install_data_;
  const std::string install_data_index_;
  const bool rollback_allowed_;
  const std::string target_channel_;
  const std::string target_version_prefix_;
  const bool update_disabled_;
  const UpdateService::PolicySameVersionUpdate policy_same_version_update_;
  scoped_refptr<PersistedData> persisted_data_;
  const crx_file::VerifierFormat crx_verifier_format_;

  // These members are not updated when the installer succeeds.
  base::Version pv_;
  std::string ap_;
  base::FilePath checker_path_;
  std::string fingerprint_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_INSTALLER_H_
