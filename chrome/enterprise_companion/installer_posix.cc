// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer_posix.h"

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/installer_paths.h"

namespace enterprise_companion {

const int kInstallDirPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                       base::FILE_PERMISSION_READ_BY_GROUP |
                                       base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                       base::FILE_PERMISSION_READ_BY_OTHERS |
                                       base::FILE_PERMISSION_EXECUTE_BY_OTHERS;

// Uninstall the application by deleting the installation directory. On Mac, the
// updater will recognize that the application has been uninstalled (via the
// existence checker path) and remove its registration. On Linux, the updater
// hasn't shipped and thus does not expose a means to manipulate registrations.
bool Uninstall() {
  std::optional<base::FilePath> install_directory = GetInstallDirectory();
  if (!install_directory) {
    LOG(ERROR) << "Failed to get install directory";
    return false;
  }
  return base::DeletePathRecursively(*install_directory);
}

}  // namespace enterprise_companion
