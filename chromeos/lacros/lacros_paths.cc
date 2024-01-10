// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace {

struct LacrosPaths {
  base::FilePath ash_resource_dir;
};

// Keep track of whether the user data directory was initialized.
bool g_initialized_user_data_dir = false;

LacrosPaths& GetLacrosPaths() {
  static base::NoDestructor<LacrosPaths> lacros_paths;
  return *lacros_paths;
}

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case chromeos::lacros_paths::ASH_RESOURCES_DIR:
      *result = GetLacrosPaths().ash_resource_dir;
      return !result->empty();
    case chromeos::lacros_paths::LACROS_SHARED_DIR:
      if (base::SysInfo::IsRunningOnChromeOS()) {
        *result = base::FilePath(crosapi::kLacrosSharedDataPath);
      } else {
        *result = base::GetHomeDir().Append(".config").Append("lacros");
      }
      return true;
    case chromeos::lacros_paths::USER_DATA_DIR:
      // Check that the user data directory is not accessed before
      // initialization.
      CHECK(chromeos::lacros_paths::IsInitializedUserDataDir());
      // The value for USER_DATA_DIR should be consistent with ash-side
      // UserDataDir defined in browser_util::GetUserDataDir().
      if (base::SysInfo::IsRunningOnChromeOS()) {
        *result = base::FilePath(crosapi::kLacrosUserDataPath);
      } else {
        *result = base::GetHomeDir().Append(".config").Append("lacros");
      }
      return true;
    case chromeos::lacros_paths::ASH_DATA_DIR:
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        return false;
      }
      *result = base::FilePath(crosapi::kAshDataDir);
      return true;
    default:
      return false;
  }
}

}  // namespace

namespace chromeos {
namespace lacros_paths {

bool IsInitializedUserDataDir() {
  return g_initialized_user_data_dir;
}

void SetInitializedUserDataDir() {
  CHECK(!g_initialized_user_data_dir);
  g_initialized_user_data_dir = true;
}

void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

void SetAshResourcesPath(const base::FilePath& path) {
  GetLacrosPaths().ash_resource_dir = path;
}

}  // namespace lacros_paths
}  // namespace chromeos
