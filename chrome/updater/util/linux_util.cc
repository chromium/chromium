// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/linux_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

constexpr base::FilePath::CharType kSystemDataPath[] =
    FILE_PATH_LITERAL("/opt/");
constexpr base::FilePath::CharType kUserRelativeDataPath[] =
    FILE_PATH_LITERAL(".local/");

}  // namespace

const char kLauncherName[] = "launcher";

base::FilePath GetExecutableRelativePath() {
  return base::FilePath(base::StrCat({kExecutableName, kExecutableSuffix}));
}

absl::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
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
  return absl::nullopt;
}

absl::optional<base::FilePath> GetUpdateServiceLauncherPath(
    UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetInstallDirectory(scope);
  return path ? absl::optional<base::FilePath>(path->AppendASCII(kLauncherName))
              : absl::nullopt;
}

}  // namespace updater
