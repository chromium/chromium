// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/linux_util.h"

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"

namespace updater {
namespace {

constexpr base::FilePath::CharType kSystemDataPath[] =
    FILE_PATH_LITERAL("/opt/");
constexpr base::FilePath::CharType kUserRelativeDataPath[] =
    FILE_PATH_LITERAL(".local/");

base::FilePath GetUpdaterFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_LOWERCASE_STRING)
      .Append(PRODUCT_FULLNAME_DASHED_LOWERCASE_STRING);
}

}  // namespace

const char kLauncherName[] = "launcher";

base::FilePath GetExecutableRelativePath() {
  return base::FilePath(base::StrCat({kExecutableName, kExecutableSuffix}));
}

std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
  base::FilePath path;
  switch (scope) {
    case UpdaterScope::kUser:
      if (base::PathService::Get(base::DIR_HOME, &path)) {
        return path.Append(kUserRelativeDataPath)
            .Append(GetUpdaterFolderName());
      }
      break;
    case UpdaterScope::kSystem:
      return base::FilePath(kSystemDataPath).Append(GetUpdaterFolderName());
  }
  return std::nullopt;
}

std::optional<base::FilePath> GetUpdateServiceLauncherPath(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetInstallDirectory(scope);
  return path ? std::optional<base::FilePath>(path->AppendASCII(kLauncherName))
              : std::nullopt;
}

bool MigrateLegacyUpdaters(
    UpdaterScope scope,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  // There is no legacy update client for Linux.
  return true;
}

std::optional<base::FilePath> GetBundledEnterpriseCompanionExecutablePath(
    UpdaterScope scope) {
  std::optional<base::FilePath> install_dir =
      GetVersionedInstallDirectory(scope);
  if (!install_dir) {
    return std::nullopt;
  }

  return install_dir->AppendASCII(
      base::StrCat({kCompanionAppExecutableName, kExecutableSuffix}));
}

}  // namespace updater
