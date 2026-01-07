// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/firefox_importer_utils.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace {

// Base path relative to the user $HOME directory where the .mozilla directory
// is located when installed via standard method.
constexpr const base::FilePath::CharType* const
    kStandardFirefoxProfileStorageBasePath = FILE_PATH_LITERAL("");

// Base path relative to the user $HOME directory where the .mozilla directory
// is located when installed via Snap.
//
// Snap uses the 'common' directory to store data that persists across
// application revisions, avoiding large profile copies during updates.
// Documentation: https://snapcraft.io/docs/data-locations
constexpr const base::FilePath::CharType* const
    kSnapFirefoxProfileStorageBasePath =
        FILE_PATH_LITERAL("snap/firefox/common");

// Base path relative to the user $HOME directory where the .mozilla directory
// is located when installed via Flatpak.
//
// Flatpak assigns each application a unique ID ('org.mozilla.firefox' for
// Firefox) and stores user-specific data, configuration, and cache files in a
// dedicated directory structure under '$HOME/.var/app/$APPLICATION_ID'.
// Documentation:
// - https://docs.flatpak.org/en/latest/flatpak-command-reference.html
// - https://docs.flatpak.org/en/latest/conventions.html
constexpr const base::FilePath::CharType* const
    kFlatpakFirefoxProfileStorageBasePath =
        FILE_PATH_LITERAL(".var/app/org.mozilla.firefox");

// Search-priority-ordered array of base paths relative to the user $HOME
// directory where the .mozilla directory may be located.
constexpr const base::FilePath::CharType* const
    kFirefoxProfileStorageBasePaths[] = {kStandardFirefoxProfileStorageBasePath,
                                         kSnapFirefoxProfileStorageBasePath,
                                         kFlatpakFirefoxProfileStorageBasePath};

// The subpath to the profiles.ini file relative to a profile storage base path.
// This is common across installation methods.
constexpr const base::FilePath::CharType* const kFirefoxProfilesIniSubpath =
    FILE_PATH_LITERAL(".mozilla/firefox/profiles.ini");

}  // namespace

base::FilePath GetProfilesINI() {
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  if (home.empty()) {
    return base::FilePath();
  }

  // Iterate through possible Firefox profile roots (locations containing
  // `profiles.ini`), starting with the standard location.
  // Note: If a user has a stale standard profile (e.g. the application was
  // uninstalled, but user data was preserved) and an active Snap/Flatpak
  // installation, we will pick up the stale one.
  for (const auto* profile_storage_base : kFirefoxProfileStorageBasePaths) {
    base::FilePath ini_file = home;
    ini_file = ini_file.Append(profile_storage_base)
                   .Append(kFirefoxProfilesIniSubpath);
    if (base::PathExists(ini_file)) {
      return ini_file;
    }
  }

  return base::FilePath();
}
