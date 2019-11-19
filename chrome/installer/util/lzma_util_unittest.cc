// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/lzma_util.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class LzmaUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("installer");
    ASSERT_TRUE(base::PathExists(data_dir_));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::ScopedTempDir temp_dir_;

  // The path to input data used in tests.
  base::FilePath data_dir_;
};

}  // namespace

// Test that we can open archives successfully.
TEST_F(LzmaUtilTest, OpenArchiveTest) {
  base::FilePath archive = data_dir_.AppendASCII("archive1.7z");
  LzmaUtilImpl lzma_util;
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.OpenArchive(archive));

  // We allow opening another archive (which will automatically close the first
  // archive).
  archive = data_dir_.AppendASCII("archive2.7z");
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.OpenArchive(archive));

  // Explicitly close and open the first archive again.
  lzma_util.CloseArchive();
  archive = data_dir_.AppendASCII("archive1.7z");
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.OpenArchive(archive));

  // Make sure non-existent archive returns error.
  archive = data_dir_.AppendASCII("archive.non_existent.7z");
  EXPECT_EQ(UNPACK_ARCHIVE_NOT_FOUND, lzma_util.OpenArchive(archive));
}

// Test that we can extract archives successfully.
TEST_F(LzmaUtilTest, UnPackTest) {
  base::FilePath extract_dir(temp_dir_.GetPath());
  extract_dir = extract_dir.AppendASCII("UnPackTest");
  ASSERT_FALSE(base::PathExists(extract_dir));
  EXPECT_TRUE(base::CreateDirectory(extract_dir));
  ASSERT_TRUE(base::PathExists(extract_dir));

  base::FilePath archive = data_dir_.AppendASCII("archive1.7z");
  LzmaUtilImpl lzma_util;
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.OpenArchive(archive));
  base::FilePath unpacked_file;
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.UnPack(extract_dir, &unpacked_file));
  EXPECT_FALSE(lzma_util.GetErrorCode());

  EXPECT_TRUE(base::PathExists(extract_dir.AppendASCII("a.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("a.exe"));

  archive = data_dir_.AppendASCII("archive2.7z");
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.OpenArchive(archive));
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.UnPack(extract_dir, &unpacked_file));
  EXPECT_TRUE(base::PathExists(extract_dir.AppendASCII("b.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("b.exe"));
  EXPECT_FALSE(lzma_util.GetErrorCode());

  lzma_util.CloseArchive();
  archive = data_dir_.AppendASCII("invalid_archive.7z");
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.OpenArchive(archive));
  EXPECT_EQ(UNPACK_SZAREX_OPEN_ERROR,
            lzma_util.UnPack(extract_dir, &unpacked_file));

  EXPECT_EQ(UNPACK_SZAREX_OPEN_ERROR,
            lzma_util.UnPack(extract_dir, &unpacked_file));

  archive = data_dir_.AppendASCII("archive3.7z");
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.OpenArchive(archive));
  EXPECT_EQ(UNPACK_NO_ERROR, lzma_util.UnPack(extract_dir, &unpacked_file));
  EXPECT_TRUE(base::PathExists(extract_dir.AppendASCII("archive\\a.exe")));
  EXPECT_TRUE(base::PathExists(
      extract_dir.AppendASCII("archive\\sub_dir\\text.txt")));
}

// Test the static method that can be used to unpack archives.
TEST_F(LzmaUtilTest, UnPackArchiveTest) {
  base::FilePath extract_dir(temp_dir_.GetPath());
  extract_dir = extract_dir.AppendASCII("UnPackArchiveTest");
  ASSERT_FALSE(base::PathExists(extract_dir));
  EXPECT_TRUE(base::CreateDirectory(extract_dir));
  ASSERT_TRUE(base::PathExists(extract_dir));

  base::FilePath archive = data_dir_.AppendASCII("archive1.7z");
  base::FilePath unpacked_file;

  EXPECT_EQ(UNPACK_NO_ERROR, UnPackArchive(archive, extract_dir, &unpacked_file,
                                           nullptr, nullptr));

  EXPECT_TRUE(base::PathExists(extract_dir.AppendASCII("a.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("a.exe"));

  archive = data_dir_.AppendASCII("archive2.7z");
  EXPECT_EQ(UNPACK_NO_ERROR, UnPackArchive(archive, extract_dir, &unpacked_file,
                                           nullptr, nullptr));

  EXPECT_TRUE(base::PathExists(extract_dir.AppendASCII("b.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("b.exe"));

  archive = data_dir_.AppendASCII("invalid_archive.7z");
  EXPECT_EQ(
      UNPACK_SZAREX_OPEN_ERROR,
      UnPackArchive(archive, extract_dir, &unpacked_file, nullptr, nullptr));
}
