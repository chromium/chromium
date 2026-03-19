// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/path_util.h"

namespace updater {

namespace {

base::FilePath GetUpdaterFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_LOWERCASE_STRING)
      .Append(PRODUCT_FULLNAME_DASHED_LOWERCASE_STRING);
}

}  // namespace

std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
  static constexpr base::FilePath::CharType kSystemDataPath[] =
      FILE_PATH_LITERAL("/opt/");
  static constexpr base::FilePath::CharType kUserRelativeDataPath[] =
      FILE_PATH_LITERAL(".local/");

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

}  // namespace updater
