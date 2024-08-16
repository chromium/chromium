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

namespace {

std::optional<base::FilePath> GetInstallDirectoryForKey(int path_key) {
  base::FilePath program_files_dir;
  if (!base::PathService::Get(path_key, &program_files_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return std::nullopt;
  }
  return program_files_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

}  // namespace

const char kExecutableName[] = "enterprise_companion.exe";

// Returns the preferred installation directory for the current process'
// architecture.
std::optional<base::FilePath> GetInstallDirectory() {
  return GetInstallDirectoryForKey(base::DIR_PROGRAM_FILES);
}

// Searches for an installed app, preferring native installations as opposed to
// 32 on 64-bit.
std::optional<base::FilePath> FindExistingInstall() {
  std::optional<base::FilePath> native_install =
      GetInstallDirectoryForKey(base::DIR_PROGRAM_FILES6432);
  native_install = native_install ? native_install->AppendASCII(kExecutableName)
                                  : native_install;
  if (native_install && base::PathExists(*native_install)) {
    return native_install;
  }

  std::optional<base::FilePath> x86_install =
      GetInstallDirectoryForKey(base::DIR_PROGRAM_FILESX86);
  if (!x86_install) {
    return std::nullopt;
  }
  x86_install = x86_install->AppendASCII(kExecutableName);
  return base::PathExists(*x86_install) ? x86_install : std::nullopt;
}

std::optional<base::FilePath> GetInstallDirectoryForAlternateArch() {
#if defined(ARCH_CPU_32_BITS)
  if (base::win::OSInfo::GetInstance()->IsWowDisabled()) {
    // 32-on-32 bit has no alternate install directories.
    return {};
  } else {
    // Emulated 32-bit competes with 64-bit installations in C:\Program Files.
    return GetInstallDirectoryForKey(base::DIR_PROGRAM_FILES6432);
  }
#elif defined(ARCH_CPU_64_BITS)
  // 64-bit competes with 32-bit installations in C:\Program Files (x86).
  return GetInstallDirectoryForKey(base::DIR_PROGRAM_FILESX86);
#else
#error CPU architecture is unknown.
#endif
}

}  // namespace enterprise_companion
