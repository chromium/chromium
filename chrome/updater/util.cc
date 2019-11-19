// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/updater/updater_version.h"

namespace updater {

bool GetProductDirectory(base::FilePath* path) {
  constexpr int kPathKey =
#if defined(OS_WIN)
      base::DIR_LOCAL_APP_DATA;
#elif defined(OS_MACOSX)
      base::DIR_APP_DATA;
#endif

  base::FilePath app_data_dir;
  if (!base::PathService::Get(kPathKey, &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve local app data directory.";
    return false;
  }

  const auto product_data_dir =
      app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII(PRODUCT_FULLNAME_STRING);
  if (!base::CreateDirectory(product_data_dir)) {
    LOG(ERROR) << "Can't create product directory.";
    return false;
  }

  *path = product_data_dir;
  return true;
}

}  // namespace updater
