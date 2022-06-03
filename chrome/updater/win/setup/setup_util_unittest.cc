// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterSetupUtil, ParseFilesFromDeps) {
  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  const base::FilePath deps = source_path.AppendASCII("chrome")
                                  .AppendASCII("updater")
                                  .AppendASCII("test")
                                  .AppendASCII("data")
                                  .AppendASCII("updater.runtime_deps");
  const auto files = ParseFilesFromDeps(deps);
  EXPECT_EQ(files.size(), 5u);
  EXPECT_EQ(files[0], base::FilePath(FILE_PATH_LITERAL(".\\updater.exe")));
  EXPECT_EQ(files[1], base::FilePath(FILE_PATH_LITERAL(".\\base.dll")));
  EXPECT_EQ(files[2], base::FilePath(FILE_PATH_LITERAL("msvcp140d.dll")));
  EXPECT_EQ(files[3], base::FilePath(FILE_PATH_LITERAL("icudtl.dat")));
  EXPECT_EQ(files[4], base::FilePath(FILE_PATH_LITERAL(
                          "gen\\chrome\\updater\\win\\uninstall.cmd")));
}

}  // namespace updater
