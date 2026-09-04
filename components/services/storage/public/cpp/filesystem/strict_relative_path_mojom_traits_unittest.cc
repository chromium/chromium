// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/strict_relative_path_mojom_traits.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

TEST(StrictRelativePathTraitsTest, ValidRelativePaths) {
  const base::FilePath test_paths[] = {
      base::FilePath(FILE_PATH_LITERAL("file.txt")),
      base::FilePath(FILE_PATH_LITERAL("dir/file.txt")),
      base::FilePath(FILE_PATH_LITERAL("dir/subdir/file.txt")),
  };

  for (const auto& original : test_paths) {
    base::FilePath deserialized;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::StrictRelativePath>(
        original, deserialized));
    EXPECT_EQ(original, deserialized);
  }
}

TEST(StrictRelativePathTraitsTest, RejectsInvalidPaths) {
  const base::FilePath test_paths[] = {
      base::FilePath(FILE_PATH_LITERAL("../file.txt")),
      base::FilePath(FILE_PATH_LITERAL("dir/../file.txt")),
  };

  for (const auto& original : test_paths) {
    base::FilePath deserialized;
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::StrictRelativePath>(
        original, deserialized));
  }
}

TEST(StrictRelativePathTraitsTest, RejectsReservedWindowsNames) {
  const base::FilePath test_paths[] = {
      base::FilePath(FILE_PATH_LITERAL("con")),
      base::FilePath(FILE_PATH_LITERAL("con ")),
      base::FilePath(FILE_PATH_LITERAL("con. ")),
      base::FilePath(FILE_PATH_LITERAL("con.txt")),
      base::FilePath(FILE_PATH_LITERAL("conin$")),
      base::FilePath(FILE_PATH_LITERAL("conin$ ")),
      base::FilePath(FILE_PATH_LITERAL("conout$")),
      base::FilePath(FILE_PATH_LITERAL("nul")),
      base::FilePath(FILE_PATH_LITERAL("dir1/nul")),
      base::FilePath(FILE_PATH_LITERAL("dir1/aux/file.txt")),
      base::FilePath(FILE_PATH_LITERAL("dir1/aux . /file.txt")),
  };

  for (const auto& original : test_paths) {
    base::FilePath deserialized;
#if BUILDFLAG(IS_WIN)
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::StrictRelativePath>(
        original, deserialized));
#else
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::StrictRelativePath>(
        original, deserialized));
    EXPECT_EQ(original, deserialized);
#endif
  }
}

TEST(StrictRelativePathTraitsTest, AllowsHarmlessExtensionsOnSpecialDevices) {
  const base::FilePath test_paths[] = {
      base::FilePath(FILE_PATH_LITERAL("conin$.txt")),
      base::FilePath(FILE_PATH_LITERAL("conout$.log")),
  };

  for (const auto& original : test_paths) {
    base::FilePath deserialized;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::StrictRelativePath>(
        original, deserialized));
    EXPECT_EQ(original, deserialized);
  }
}

}  // namespace
}  // namespace storage
