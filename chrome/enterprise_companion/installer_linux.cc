// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "installer_posix.h"

namespace enterprise_companion {

bool Install() {
  std::optional<base::FilePath> install_directory = GetInstallDirectory();
  if (!install_directory) {
    LOG(ERROR) << "Failed to get install directory";
    return false;
  }

  return InstallToDir(*install_directory);
}

}  // namespace enterprise_companion
