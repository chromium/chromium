// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_PATHS_H_
#define CHROMEOS_LACROS_LACROS_PATHS_H_

namespace base {
class FilePath;
}

// This file declares path keys for lacros. These can be used with the
// PathService to access various special directories and files.

namespace chromeos {
namespace lacros_paths {

enum {
  PATH_START = 13000,

  // Directory that contains ash's application assets.
  ASH_RESOURCES_DIR,

  // Directory that contains Lacros files that are shared across users.
  LACROS_SHARED_DIR,

  // Directory that contains user data in Lacros.
  USER_DATA_DIR,

  // Directory that contains data in Ash.
  ASH_DATA_DIR,

  PATH_END
};

// Returns true if the user data directory has been initialized,
// false otherwise.
bool IsInitializedUserDataDir();

// Signals that the user data directory has been initialized.
void SetInitializedUserDataDir();

// Call once to register the provide for the path keys defined above.
void RegisterPathProvider();

// Set ash resources dir path to `ash_resources_dir`.
// Given via crosapi.
void SetAshResourcesPath(const base::FilePath& path);

}  // namespace lacros_paths
}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_PATHS_H_
