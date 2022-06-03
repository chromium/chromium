// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

absl::optional<base::FilePath> GetUpdaterFolderPath(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(scope == UpdaterScope::kSystem
                                  ? base::DIR_PROGRAM_FILES
                                  : base::DIR_LOCAL_APP_DATA,
                              &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

// TODO(crbug.com/1241276) combine "updater.exe" literals.
base::FilePath GetExecutableRelativePath() {
  return base::FilePath(FILE_PATH_LITERAL("updater.exe"));
}

}  // namespace updater
