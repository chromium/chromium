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

const base::FilePath::CharType kScreenAILibraryFileName[] =
    FILE_PATH_LITERAL("libchrome_screen_ai.so");
}  // namespace

const base::FilePath GetRelativeInstallDir() {
  return base::FilePath(kScreenAISubDirName);
}

const base::FilePath GetLibraryFilePath() {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);

  if (components_dir.empty())
    return base::FilePath();

  // Get latest version.
  base::FileEnumerator enumerator(components_dir.Append(kScreenAISubDirName),
                                  /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath latest_version_dir;
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }

  base::FilePath library_path =
      latest_version_dir.Append(kScreenAILibraryFileName);
  if (!base::PathExists(library_path))
    return base::FilePath();

  return library_path;
}

}  // namespace screen_ai