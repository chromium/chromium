// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_LIB_H_
#define CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_LIB_H_

#include <string>

#include "base/files/file_path.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace credential_provider {

struct FakesForTesting;

// Does a full install of GCP.  |installer_path| is the full path to the
// installer exe and |product_version| is the version of GCP being installed.
HRESULT DoInstall(const base::FilePath& installer_path,
                  const std::wstring& product_version,
                  FakesForTesting* fakes);

// Does a full uninstall of GCP.  |installer_path| is the full path to the
// installer exe, |dest_path| is the directory containing the GCP to uninstall.
HRESULT DoUninstall(const base::FilePath& installer_path,
                    const base::FilePath& dest_path,
                    FakesForTesting* fakes);

// Relaunches the installer at |installer_path| in a new process, telling it to
// uninstall GCP from the parent directory of |installer_path|.  This function
// returns immediately and does not wait for the new process to complete.
// This new process will wait for this one to exit before continuing so that
// files are not locked and can be deleted correctly.
HRESULT RelaunchUninstaller(const base::FilePath& installer_path);

// Enable or disable stats and crash report collection. Returns 0 on success
// and -1 on failure.
int EnableStatsCollection(const base::CommandLine& cmdline);

// Writes the UninstallString and UninstallArguments values to the product's
// ClientState key in support of uninstallation by the MSI wrapper.
HRESULT WriteUninstallRegistryValues(const base::FilePath& setup_exe);

// Writes the registry entries Credential Provider uses at runtime.
HRESULT WriteCredentialProviderRegistryValues(
    const base::FilePath& install_path);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_LIB_H_
