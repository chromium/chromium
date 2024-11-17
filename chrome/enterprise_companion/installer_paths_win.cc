// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer_paths.h"

#include <optional>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"

namespace enterprise_companion {

const char kExecutableName[] = "enterprise_companion.exe";

std::optional<base::FilePath> GetInstallDirectory() {
  base::FilePath program_files_dir;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return {};
  }
  return program_files_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

std::optional<base::FilePath> FindExistingInstall() {
  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  if (!install_dir) {
    return {};
  }
  install_dir = install_dir->AppendASCII(kExecutableName);
  return base::PathExists(*install_dir) ? install_dir : std::nullopt;
}

}  // namespace enterprise_companion
