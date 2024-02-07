// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/utilities.h"

#include "base/check_is_test.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"

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

#if BUILDFLAG(IS_CHROMEOS)
// The path to the Screen AI DLC directory.
constexpr char kScreenAIDlcRootPath[] =
    "/run/imageloader/screen-ai/package/root/";
#endif

#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
#if BUILDFLAG(IS_LINUX)
constexpr base::FilePath::CharType kScreenAIResourcePathForTests[] =
    FILE_PATH_LITERAL("third_party/screen-ai/linux/resources");
#elif BUILDFLAG(IS_MAC)
#if defined(ARCH_CPU_X86_64)
constexpr base::FilePath::CharType kScreenAIResourcePathForTests[] =
    FILE_PATH_LITERAL("third_party/screen-ai/macos_amd64/resources");
#elif defined(ARCH_CPU_ARM64)
constexpr base::FilePath::CharType kScreenAIResourcePathForTests[] =
    FILE_PATH_LITERAL("third_party/screen-ai/macos_arm64/resources");
#endif  // defined(ARCH_CPU_X86_64)
#endif  // BUILDFLAG(IS_LINUX)

// Get the directory that contains the ScreenAI component for testing.
base::FilePath GetTestComponentDir() {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));

  base::FilePath screenai_library_dir =
      test_data_dir.Append(base::FilePath(kScreenAIResourcePathForTests));

  CHECK(base::PathExists(screenai_library_dir));
  return screenai_library_dir;
}

// Get the absolute path of the ScreenAI component for testing.
base::FilePath GetTestComponentBinaryPath() {
  base::FilePath test_data_dir = GetTestComponentDir();

  base::FilePath screenai_library_path =
      test_data_dir.Append(kScreenAIComponentBinaryName);

  CHECK(base::PathExists(screenai_library_path));
  return screenai_library_path;
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)

}  // namespace

base::FilePath GetRelativeInstallDir() {
  return base::FilePath(kScreenAISubDirName);
}

base::FilePath GetComponentBinaryFileName() {
  return base::FilePath(kScreenAIComponentBinaryName);
}

base::FilePath GetComponentDir() {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
  // When in `ScreenAITestMode`, return the path that contains the screen-ai
  // binary downloaded from CIPD.
  if (features::IsScreenAITestModeEnabled()) {
    CHECK_IS_TEST();
    return GetTestComponentDir();
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)

  base::FilePath components_dir;
  if (!base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                              &components_dir) ||
      components_dir.empty()) {
    return base::FilePath();
  }

  return components_dir.Append(kScreenAISubDirName);
}

base::FilePath GetLatestComponentBinaryPath() {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
  if (features::IsScreenAITestModeEnabled()) {
    CHECK_IS_TEST();
    return GetTestComponentBinaryPath();
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)

  base::FilePath latest_version_dir;
#if BUILDFLAG(IS_CHROMEOS)
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
