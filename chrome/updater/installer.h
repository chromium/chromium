// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "base/values.h"
#include "base/version.h"
#include "chrome/updater/persisted_data.h"
#include "components/update_client/update_client.h"

namespace updater {

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
            scoped_refptr<PersistedData> persisted_data);
  Installer(const Installer&) = delete;
  Installer& operator=(const Installer&) = delete;

  const std::string app_id() const { return app_id_; }

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
                                 const std::string& public_key,
                                 std::unique_ptr<InstallParams> install_params,
                                 ProgressCallback progress_callback,
                                 Callback callback);

  // Handles the application installer specified by the |app_installer| and
  // its |arguments|. This data is returned by the update server as part of
  // the manifest object in an update response. Handling of the application
  // installer is typically OS-specific, such as building a command line,
  // creating processes, mounting images, running scripts, and collecting
  // exit codes. The install progress, if it can be collected, is reported by
  // invoking the |progress_callback|.
  int RunApplicationInstaller(const base::FilePath& app_installer,
                              const std::string& arguments,
                              ProgressCallback progress_callback);

  // Deletes recursively the install paths not matching the |pv_| version.
  void DeleteOlderInstallPaths();

  // Returns an install directory matching the |pv_| version.
  base::FilePath GetCurrentInstallDir() const;

  SEQUENCE_CHECKER(sequence_checker_);

  const std::string app_id_;
  scoped_refptr<PersistedData> persisted_data_;

  // These members are not updated when the installer succeeds.
  base::Version pv_;
  base::FilePath checker_path_;
  std::string fingerprint_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_INSTALLER_H_
