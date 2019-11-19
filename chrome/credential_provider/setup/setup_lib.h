// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_LIB_H_
#define CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_LIB_H_

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace credential_provider {

struct FakesForTesting;

// Define command line swtiches for setup.

namespace switches {

extern const char kParentHandle[];
extern const char kInstallPath[];
extern const char kUninstall[];
extern const char kEnableStats[];
extern const char kDisableStats[];

}  // namespace switches

// Does a full install of GCP.  |installer_path| is the full path to the
// installer exe and |product_version| is the version of GCP being installed.
HRESULT DoInstall(const base::FilePath& installer_path,
                  const base::string16& product_version,
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

// Returns the basenames of the files that are installed by setup.  This is
// used in tests to validate that files are correctly installed.
void GetInstalledFileBasenames(const base::FilePath::CharType* const** names,
                               size_t* count);

// Enable or disable stats and crash report collection.  Returns 0 on success
// and -1 on failure.
int EnableStatsCollection(const base::CommandLine& cmdline);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_SETUP_SETUP_LIB_H_
