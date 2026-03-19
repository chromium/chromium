// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/path_util.h"

namespace updater {

std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(IsSystemInstall(scope)
                                  ? base::DIR_PROGRAM_FILESX86
                                  : base::DIR_LOCAL_APP_DATA,
                              &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return std::nullopt;
  }
  return app_data_dir.AppendUTF8(COMPANY_SHORTNAME_STRING)
      .AppendUTF8(PRODUCT_FULLNAME_STRING);
}

}  // namespace updater
