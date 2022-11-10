// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_PARAMS_H_
#define CHROME_INSTALLER_SETUP_INSTALL_PARAMS_H_

#include "base/memory/raw_ref.h"
#include "modify_params.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace installer {

class InstallationState;
class InstallerState;

// InstallParams represents the collection of parameters needed
// for install operations.
//
// InstallParams are expected to be constructed on the stack,
// with the lifetime of the contained references and pointers to
// be a strict subset of the calling stack frame.
struct InstallParams : public ModifyParams {
  // Path to the archive (chrome.7z)
  const raw_ref<const base::FilePath> archive_path;
  // Unpacked Chrome package (inside |temp_path|)
  const raw_ref<const base::FilePath> src_path;
  // Working directory used during install/update
  const raw_ref<const base::FilePath> temp_path;
  // Chrome version to be installed
  const raw_ref<const base::Version> new_version;

  InstallParams(InstallerState& installer_state,
                InstallationState& installation_state,
                const base::FilePath& setup_path,
                const base::Version& current_version,
                const base::FilePath& archive_path,
                const base::FilePath& src_path,
                const base::FilePath& temp_path,
                const base::Version& new_version)
      : ModifyParams(installer_state,
                     installation_state,
                     setup_path,
                     current_version),
        archive_path(archive_path),
        src_path(src_path),
        temp_path(temp_path),
        new_version(new_version) {}
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_PARAMS_H_
