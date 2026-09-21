// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_module/child_module_helper.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

namespace child_module {

base::FilePath GetModulesDir() {
#if BUILDFLAG(IS_WIN)
  base::FilePath install_dir;
  if (!base::PathService::Get(base::DIR_EXE, &install_dir)) {
    return base::FilePath();
  }
  return install_dir.AppendASCII(chrome::kChromeVersion)
      .Append(kModulesDirName);
#else
  // TODO(crbug.com/558598893): Revisit this once we settle on how per-User Data
  // dir version tracking is handled, and determine the official staging
  // directory for non-Windows platforms (e.g., macOS outer bundle or system
  // directories). For now, look under DIR_USER_DATA so development and
  // component updater workflows can function.
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return base::FilePath();
  }
  return user_data_dir.Append(kModulesDirName)
      .AppendASCII(chrome::kChromeVersion);
#endif
}

base::FilePath GetManifestPath(const base::FilePath& version_dir) {
  if (version_dir.empty()) {
    return base::FilePath();
  }
  return version_dir.Append(kManifestFilename);
}

base::FilePath GetRendererBinaryPath(const base::Version& version) {
  if (!version.IsValid()) {
    return base::FilePath();
  }
  base::FilePath modules_dir = GetModulesDir();
  if (modules_dir.empty()) {
    return base::FilePath();
  }
  return modules_dir
      .AppendASCII(version.GetString())
#if BUILDFLAG(IS_WIN)
      .Append(chrome::kRendererDll);
#else
      .Append(chrome::kRendererProcessExecutableName);
#endif
}

}  // namespace child_module
