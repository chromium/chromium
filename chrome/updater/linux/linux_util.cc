// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/linux_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

constexpr base::FilePath::CharType kSystemDataPath[] =
    FILE_PATH_LITERAL("/opt/");
constexpr base::FilePath::CharType kUserRelativeDataPath[] =
    FILE_PATH_LITERAL(".local/");

}  // namespace

absl::optional<base::FilePath> GetApplicationDataDirectory(UpdaterScope scope) {
  base::FilePath path;
  switch (scope) {
    case UpdaterScope::kUser:
      if (base::PathService::Get(base::DIR_HOME, &path))
        return path.Append(kUserRelativeDataPath);
      break;
    case UpdaterScope::kSystem:
      return base::FilePath(kSystemDataPath);
  }
  return absl::nullopt;
}

absl::optional<base::FilePath> GetBaseInstallDirectory(UpdaterScope scope) {
  return GetApplicationDataDirectory(scope);
}

base::FilePath GetExecutableRelativePath() {
  return base::FilePath(kExecutableName);
}

}  // namespace updater
