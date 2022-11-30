// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/migration/migration_utils.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chromecast/base/cast_paths.h"

namespace chromecast {
namespace cast_browser_migration {

namespace {

constexpr char kLargeConfigExtension[] = ".large";

bool CopyConfigFileIfMissing(bool is_large) {
  base::FilePath new_config_path;
  CHECK(base::PathService::Get(FILE_CAST_BROWSER_CONFIG, &new_config_path));
  if (is_large) {
    new_config_path = new_config_path.AddExtension(kLargeConfigExtension);
  }
  if (base::PathExists(new_config_path)) {
    return true;
  }

  base::FilePath old_config_path;
  CHECK(base::PathService::Get(FILE_CAST_CONFIG, &old_config_path));
  if (is_large) {
    old_config_path = old_config_path.AddExtension(kLargeConfigExtension);
  }
  bool success = base::CopyFile(old_config_path, new_config_path);
  if (!success) {
    LOG(ERROR) << "Failed to copy pref config: " << old_config_path.value();
    base::DeleteFile(new_config_path);
  }
  return success;
}

}  // namespace

bool CopyPrefConfigsIfMissing() {
  return CopyConfigFileIfMissing(/*is_large=*/false) &&
         CopyConfigFileIfMissing(/*is_large=*/true);
}

}  // namespace cast_browser_migration
}  // namespace chromecast
