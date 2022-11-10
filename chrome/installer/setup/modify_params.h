// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_MODIFY_PARAMS_H_
#define CHROME_INSTALLER_SETUP_MODIFY_PARAMS_H_

#include "base/memory/raw_ref.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace installer {

class InstallationState;
class InstallerState;

// ModifyParams represents the collection of parameters needed
// for modify operations (uninstall, repair, os upgrade, etc.)
// as well as for install operations.
//
// ModifyParams are expected to be constructed on the stack,
// with the lifetime of the contained references and pointers to
// be a strict subset of the calling stack frame.
struct ModifyParams {
  const raw_ref<InstallerState> installer_state;
  const raw_ref<InstallationState> installation_state;

  // Path to the executable (setup.exe)
  const raw_ref<const base::FilePath> setup_path;

  // Current installed version if valid; otherwise, no version is installed.
  const raw_ref<const base::Version> current_version;

  ModifyParams(InstallerState& installer_state,
               InstallationState& installation_state,
               const base::FilePath& setup_path,
               const base::Version& current_version)
      : installer_state(installer_state),
        installation_state(installation_state),
        setup_path(setup_path),
        current_version(current_version) {}
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_MODIFY_PARAMS_H_
