// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_DYNAMIC_PATCH_COMPONENT_H_
#define CHROME_INSTALLER_SETUP_DYNAMIC_PATCH_COMPONENT_H_

#include <memory>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "base/win/sid.h"
#include "chrome/installer/setup/component_interface.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class DictValue;
}  // namespace base

namespace installer {

class InstallationState;
class InstallerState;

// Component implementation for Chrome Dynamic Patches. A patch is installed
// into a directory specific to a user and User Data directory (see
// `child_module::ComputeUserPathComponent()`) within the directory of the
// currently-running Chrome version if it is compatible with the patch, or
// otherwise the highest compatible version whose directory exists on disk.
class DynamicPatchComponent : public ComponentInterface {
 public:
  // Returns a new instance for the User Data directory given by the --udd
  // switch in `command_line` and, for a system-level install, the user given by
  // the --user-sid switch. The current user is used for a user-level install.
  // Returns nullptr if either is missing or invalid.
  static std::unique_ptr<DynamicPatchComponent> Create(
      const base::CommandLine& command_line,
      const InstallationState& original_state,
      const InstallerState& installer_state);

  // `sid` identifies the user, `user_data_dir` is the User Data directory for
  // which the patch is to be installed, and `current_version` is the version of
  // Chrome currently running (or invalid if unknown).
  DynamicPatchComponent(base::win::Sid sid,
                        base::FilePath user_data_dir,
                        base::Version current_version);
  DynamicPatchComponent(const DynamicPatchComponent&) = delete;
  DynamicPatchComponent& operator=(const DynamicPatchComponent&) = delete;
  ~DynamicPatchComponent() override;

  // ComponentInterface:
  bool ReadManifest(const base::DictValue& manifest) override;
  base::expected<base::FilePath, InstallStatus> DetermineDestinationRoot(
      const InstallerState& installer_state,
      std::string_view crx_id) override;
  bool InitializeDestinationRoot(
      const InstallerState& installer_state,
      const base::FilePath& destination_root) override;

 private:
  const base::win::Sid sid_;
  const base::FilePath user_data_dir_;
  const base::Version current_version_;

  // The versions of Chrome that the patch applies to; set by ReadManifest().
  base::flat_set<base::Version> compatible_versions_;
};

// Returns true if `sid` represents an ordinary user account (local, domain,
// or Azure AD user), and false for service accounts, groups, or system SIDs.
// This is a structural check of the SID only; it does not look up the account
// (so it performs no network I/O) and does not verify that the account exists.
bool IsUserSid(const base::win::Sid& sid);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_DYNAMIC_PATCH_COMPONENT_H_
