// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALLER_STATE_H_
#define CHROME_INSTALLER_SETUP_INSTALLER_STATE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/version.h"
#include "base/win/windows_types.h"
#include "build/build_config.h"
#include "chrome/installer/setup/progress_calculator.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
}

namespace installer {

class InstallationState;
class InitialPreferences;

// Encapsulates the state of the current installation operation. This class
// interprets the command-line arguments and initial preferences and determines
// the operations to be performed.
class InstallerState {
 public:
  enum Level { UNKNOWN_LEVEL, USER_LEVEL, SYSTEM_LEVEL };

  enum Operation { UNINITIALIZED, SINGLE_INSTALL_OR_UPDATE, UNINSTALL };

  // Constructs an uninitialized instance; see Initialize().
  InstallerState();

  // Constructs an initialized but empty instance.
  explicit InstallerState(Level level);

  InstallerState(const InstallerState&) = delete;
  InstallerState& operator=(const InstallerState&) = delete;

  ~InstallerState();

  // Initializes this object based on the current operation.
  void Initialize(const base::CommandLine& command_line,
                  const InitialPreferences& prefs,
                  const InstallationState& machine_state);

  // The level (user or system) of this operation.
  Level level() const { return level_; }

  // An identifier of this operation.
  Operation operation() const { return operation_; }

  // A convenience method returning level() == SYSTEM_LEVEL.
  bool system_install() const;

  // The full path to the place where the operand resides.
  const base::FilePath& target_path() const { return target_path_; }

  // Sets the value returned by target_path().
  void set_target_path_for_testing(const base::FilePath& target_path) {
    target_path_ = target_path;
  }

  // True if the "msi" preference is set or if a product with the "msi" state
  // flag is set is to be operated on.
  bool is_msi() const { return msi_; }

  // True if the --verbose-logging command-line flag is set or if the
  // verbose_logging initial preferences option is true.
  bool verbose_logging() const { return verbose_logging_; }

  HKEY root_key() const { return root_key_; }

  // The ClientState key by which we interact with Google Update.
  const std::wstring& state_key() const { return state_key_; }

  // Returns the currently installed version in |target_path|.
  // Use IsValid() predicate to detect if product not installed.
  base::Version GetCurrentVersion(const InstallationState& machine_state) const;

  // Returns the critical update version if all of the following are true:
  // * --critical-update-version=CUV was specified on the command-line.
  // * !current_version.IsValid() or current_version < CUV.
  // * new_version >= CUV.
  // Otherwise, returns an invalid version.
  base::Version DetermineCriticalVersion(
      const base::Version& current_version,
      const base::Version& new_version) const;

  // Returns the path to the installer under Chrome version folder
  // (for example <target_path>\Google\Chrome\Application\<Version>\Installer)
  base::FilePath GetInstallerDirectory(const base::Version& version) const;

  // Sets the current stage of processing. This reports a progress value to
  // Google Update for presentation to a user.
  void SetStage(InstallerStage stage) const;

  // Sets installer result information in the registry for consumption by Google
  // Update. The InstallerResult value is set to 0 (SUCCESS) or 1
  // (FAILED_CUSTOM_ERROR) depending on whether |status| maps to success or not.
  // |status| itself is written to the InstallerError value.
  // |string_resource_id|, if non-zero, identifies a localized string written to
  // the InstallerResultUIString value. |launch_cmd|, if non-nullptr and
  // non-empty, is written to the InstallerSuccessLaunchCmdLine value.
  void WriteInstallerResult(InstallStatus status,
                            int string_resource_id,
                            const std::wstring* launch_cmd) const;

  // Returns true if this install needs to register an Active Setup command.
  bool RequiresActiveSetup() const;

 protected:
  // Clears the instance to an uninitialized state.
  void Clear();

  // Sets this object's level and updates the root_key_ accordingly.
  void set_level(Level level);

  Operation operation_;
  base::FilePath target_path_;
  std::wstring state_key_;
  base::Version critical_update_version_;
  ProgressCalculator progress_calculator_;
  Level level_;
  HKEY root_key_;
  bool msi_;
  bool verbose_logging_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALLER_STATE_H_
