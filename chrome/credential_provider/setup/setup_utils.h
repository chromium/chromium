// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_UTILS_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/win/windows_types.h"

namespace credential_provider {

namespace switches {

// These are command line switches to the setup program.

// Indicates the handle of the parent setup process when setup relaunches itself
// during uninstall.
extern const char kParentHandle[];

// Indicates the full path to the GCP installation to delete.  This switch is
// only used during uninstall.
extern const char kInstallPath[];

// Indicates to setup that it is being run to inunstall GCP.  If this switch
// is not present the assumption is to install GCP.
extern const char kUninstall[];

// Command line arguments used to either enable or disable stats and crash
// dump collection.  When either of these command line args is used setup
// will perform the requested action and exit without trying to install or
// uninstall anything.  Disable takes precedence over enable.
extern const char kEnableStats[];
extern const char kDisableStats[];

extern const char kStandaloneInstall[];

}  // namespace switches

class StandaloneInstallerConfigurator {
 public:
  // Used to retrieve singleton instance of the StandaloneInstallerConfigurator.
  static StandaloneInstallerConfigurator* Get();

  void ConfigureInstallationType(const base::CommandLine& cmdline);

  HRESULT AddUninstallKey(const base::FilePath& install_path);

  HRESULT RemoveUninstallKey();

  bool IsStandaloneInstallation() const;

 private:
  StandaloneInstallerConfigurator();

  virtual ~StandaloneInstallerConfigurator();

  // Returns the storage used for the instance pointer.
  static StandaloneInstallerConfigurator** GetInstanceStorage();

  base::string16 GetCurrentDate();

  bool is_standalone_installation_;
};

bool IsStandaloneInstallation(const base::CommandLine& command_line);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_UTILS_H_
