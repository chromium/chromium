// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_KEYSTONE_H_
#define CHROME_UPDATER_MAC_SETUP_KEYSTONE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class FilePath;
}  // namespace base

namespace updater {

// Create a plist file that makes legacy Keystone installer believe that a
// healthy newer version updater already exists and thus won't over-install.
bool CreateLegacyPlistFile(UpdaterScope scope,
                           const std::string& library_subdir,
                           const std::string& file_suffix);
bool CreateLegacyPlistFileForTesting(UpdaterScope scope,
                                     const base::FilePath& library_dir,
                                     const std::string& library_subdir,
                                     const std::string& file_suffix);

// Installs Keystone and the necessary supporting files.
bool InstallKeystone(UpdaterScope scope);

// Uninstalls Keystone from the system. Does not remove Keystone's ticket
// stores.
void UninstallKeystone(UpdaterScope scope);

// `Calls register_callback` with data from Keystone's ticket store if needed.
// This is a best-effort operation, tickets with errors are not migrated, but
// a complete parsing failure will be result in returning false.
bool MigrateKeystoneApps(
    const base::FilePath& keystone_path,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SETUP_KEYSTONE_H_
