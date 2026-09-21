// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_module/child_module_helper.h"

#include "base/files/file_path.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace child_module {

TEST(ChildModuleHelperTest, GetModulesDirNotEmpty) {
  base::FilePath dir = GetModulesDir();
  EXPECT_NE(dir, base::FilePath());
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(dir.BaseName(), base::FilePath(kModulesDirName));
#else
  EXPECT_EQ(dir.DirName().BaseName(), base::FilePath(kModulesDirName));
#endif
}

TEST(ChildModuleHelperTest, GetManifestPath) {
  base::FilePath version_dir = GetModulesDir().AppendASCII("147.0.7727.51");
  EXPECT_EQ(GetManifestPath(version_dir),
            version_dir.Append(kManifestFilename));
  EXPECT_EQ(GetManifestPath(base::FilePath()), base::FilePath());
}

TEST(ChildModuleHelperTest, GetRendererBinaryPath) {
  base::Version version("147.0.7727.51");
  base::FilePath expected_path = GetModulesDir().AppendASCII("147.0.7727.51");
#if BUILDFLAG(IS_WIN)
  expected_path = expected_path.Append(chrome::kRendererDll);
#else
  expected_path = expected_path.Append(chrome::kRendererProcessExecutableName);
#endif
  EXPECT_EQ(GetRendererBinaryPath(version), expected_path);

  EXPECT_EQ(GetRendererBinaryPath(base::Version()), base::FilePath());
}

}  // namespace child_module
