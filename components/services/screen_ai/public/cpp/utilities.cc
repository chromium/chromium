// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/utilities.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"

namespace screen_ai {

namespace {
const base::FilePath::CharType kScreenAISubDirName[] =
    FILE_PATH_LITERAL("screen_ai");

const base::FilePath::CharType kScreenAIComponentBinaryName[] =
#if BUILDFLAG(IS_WIN)
    FILE_PATH_LITERAL("chrome_screen_ai.dll");
#else
    FILE_PATH_LITERAL("libchromescreenai.so");
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The path to the Screen AI DLC directory.
// TODO(https://crbug.com/1278249): Replace by get it from  DlcServiceClient
// after installation.
constexpr char kScreenAIDlcRootPath[] =
    "/run/imageloader/screen-ai/package/root/";
#endif

}  // namespace

base::FilePath GetRelativeInstallDir() {
  return base::FilePath(kScreenAISubDirName);
}

base::FilePath GetComponentBinaryFileName() {
  return base::FilePath(kScreenAIComponentBinaryName);
}

base::FilePath GetComponentDir() {
  base::FilePath components_dir;
  if (!base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                              &components_dir) ||
      components_dir.empty()) {
    return base::FilePath();
  }

  return components_dir.Append(kScreenAISubDirName);
}

base::FilePath GetLatestComponentBinaryPath() {
  base::FilePath latest_version_dir;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  latest_version_dir = base::FilePath::FromASCII(kScreenAIDlcRootPath);
#else
  base::FilePath screen_ai_dir = GetComponentDir();
  if (screen_ai_dir.empty())
    return base::FilePath();

  // Get latest version.
  base::FileEnumerator enumerator(screen_ai_dir,
                                  /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }
#endif

  if (latest_version_dir.empty()) {
    return base::FilePath();
  }

  base::FilePath component_path =
      latest_version_dir.Append(kScreenAIComponentBinaryName);
  if (!base::PathExists(component_path))
    return base::FilePath();

  return component_path;
}

}  // namespace screen_ai
