// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/utilities.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/component_updater/component_updater_paths.h"

namespace screen_ai {

namespace {
const base::FilePath::CharType kScreenAISubDirName[] =
    FILE_PATH_LITERAL("screen_ai");

const base::FilePath::CharType kScreenAIComponentBinaryName[] =
    FILE_PATH_LITERAL("libchrome_screen_ai.so");

enum {
  PATH_START = 13000,

  // Note that this value is not kept between sessions or shared between
  // processes.
  PATH_SCREEN_AI_LIBRARY_BINARY,

  PATH_END
};

}  // namespace

base::FilePath GetRelativeInstallDir() {
  return base::FilePath(kScreenAISubDirName);
}

base::FilePath GetComponentDir() {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);
  if (components_dir.empty())
    return base::FilePath();

  return components_dir.Append(kScreenAISubDirName);
}

base::FilePath GetLatestComponentBinaryPath() {
  base::FilePath screen_ai_dir = GetComponentDir();
  if (screen_ai_dir.empty())
    return base::FilePath();

  // Get latest version.
  base::FileEnumerator enumerator(screen_ai_dir,
                                  /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath latest_version_dir;
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }

  base::FilePath component_path =
      latest_version_dir.Append(kScreenAIComponentBinaryName);
  if (!base::PathExists(component_path))
    return base::FilePath();

  return component_path;
}

void StoreComponentBinaryPath(const base::FilePath& path) {
  base::PathService::Override(PATH_SCREEN_AI_LIBRARY_BINARY, path);
}

base::FilePath GetStoredComponentBinaryPath() {
  base::FilePath path;
  base::PathService::Get(PATH_SCREEN_AI_LIBRARY_BINARY, &path);
  return path;
}

}  // namespace screen_ai