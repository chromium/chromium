// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/decompress.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(MiniDecompressTest, ExpandTest) {
  base::FilePath source_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path);
  source_path = source_path.Append(FILE_PATH_LITERAL("chrome"))
                    .Append(FILE_PATH_LITERAL("installer"))
                    .Append(FILE_PATH_LITERAL("test"))
                    .Append(FILE_PATH_LITERAL("data"))
                    .Append(FILE_PATH_LITERAL("SETUP.EX_"));

  // Prepare a temp folder that will be automatically deleted along with
  // our temporary test data.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath dest_path(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("setup.exe")));

  // Decompress our test file.
  EXPECT_TRUE(mini_installer::Expand(source_path.value().c_str(),
                                     dest_path.value().c_str()));

  // Check if the expanded file is a valid executable.
  DWORD type = static_cast<DWORD>(-1);
  EXPECT_TRUE(GetBinaryType(dest_path.value().c_str(), &type));
  EXPECT_EQ(static_cast<DWORD>(SCS_32BIT_BINARY), type);
}
