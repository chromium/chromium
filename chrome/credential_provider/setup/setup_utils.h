// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_UTILS_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/values.h"
#include "base/win/windows_types.h"

namespace base {
class FilePath;
}  // namespace base

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

// Switch that indicates the fresh installation.
extern const char kStandaloneInstall[];

// Dynamic install parameter switch which is only set for MSIs.
extern const char kInstallerData[];

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

  std::wstring GetCurrentDate();

  // Parse the provided installer data argument and load into
  // |installer_data_dictionary_|.
  bool InitializeFromInstallerData(base::FilePath prefs_path);

  // Indicates that GCPW installation source is MSI.
  bool is_msi_installation_;

  // Dictionary is parsed from the installer data argument which is set only for
  // MSIs.
  base::Value::Dict installer_data_dictionary_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_UTILS_H_
