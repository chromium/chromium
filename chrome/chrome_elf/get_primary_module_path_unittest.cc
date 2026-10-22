// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/get_primary_module_path.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(GetPrimaryModulePathTest, Do) {
  // In developer builds, chrome_elf.dll is in the same directory as the test
  // executable.
  EXPECT_TRUE(base::FilePath::CompareEqualIgnoreCase(
      install_static::GetPrimaryModulePath().value(),
      base::PathService::CheckedGet(base::DIR_EXE)
          .Append(FILE_PATH_LITERAL("chrome_elf.dll"))
          .value()));
}
