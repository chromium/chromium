// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#import "chrome/updater/util/path_util.h"

namespace updater {

std::optional<base::FilePath> GetLibraryFolderPath(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return base::apple::GetUserLibraryPath();
    case UpdaterScope::kSystem: {
      base::FilePath local_library_path;
      if (!base::apple::GetLocalDirectory(NSLibraryDirectory,
                                          &local_library_path)) {
        VLOG(1) << "Could not get local library path";
        return std::nullopt;
      }
      return local_library_path;
    }
  }
}

std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  return path ? std::optional<base::FilePath>(
                    path->Append("Application Support")
                        .Append(COMPANY_SHORTNAME_STRING)
                        .Append(PRODUCT_FULLNAME_STRING))
              : std::nullopt;
}

}  // namespace updater
