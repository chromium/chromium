// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/archive_patch_helper.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ArchivePatchHelperTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("installer");
    ASSERT_TRUE(base::PathExists(data_dir_));
  }

  static void TearDownTestCase() { data_dir_.clear(); }

  void SetUp() override {
    // Create a temp directory for testing.
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    // Clean up test directory manually so we can fail if it leaks.
    ASSERT_TRUE(test_dir_.Delete());
  }

  // The path to input data used in tests.
  static base::FilePath data_dir_;

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir test_dir_;
};

base::FilePath ArchivePatchHelperTest::data_dir_;

}  // namespace

TEST_F(ArchivePatchHelperTest, ZucchiniPatching) {
  base::FilePath src = data_dir_.AppendASCII("archive1.7z");
  base::FilePath patch = data_dir_.AppendASCII("zucchini_archive.diff");
  base::FilePath dest = test_dir_.GetPath().AppendASCII("archive2.7z");
  installer::ArchivePatchHelper archive_helper(
      test_dir_.GetPath(), base::FilePath(), src, dest,
      installer::UnPackConsumer::SETUP_EXE_PATCH);
  archive_helper.set_last_uncompressed_file(patch);
  EXPECT_TRUE(archive_helper.ZucchiniEnsemblePatch());
  base::FilePath base = data_dir_.AppendASCII("archive2.7z");
  EXPECT_TRUE(base::ContentsEqual(dest, base));
}

TEST_F(ArchivePatchHelperTest, InvalidDiff_MisalignedCblen) {
  base::FilePath src = data_dir_.AppendASCII("bin.old");
  base::FilePath patch = data_dir_.AppendASCII("misaligned_cblen.diff");
  base::FilePath dest = test_dir_.GetPath().AppendASCII("bin.new");
  installer::ArchivePatchHelper archive_helper(
      test_dir_.GetPath(), base::FilePath(), src, dest,
      installer::UnPackConsumer::SETUP_EXE_PATCH);
  archive_helper.set_last_uncompressed_file(patch);
  // Should fail, but not crash.
  EXPECT_FALSE(archive_helper.BinaryPatch());
}

TEST_F(ArchivePatchHelperTest, InvalidDiff_NegativeSeek) {
  base::FilePath src = data_dir_.AppendASCII("bin.old");
  base::FilePath patch = data_dir_.AppendASCII("negative_seek.diff");
  base::FilePath dest = test_dir_.GetPath().AppendASCII("bin.new");
  installer::ArchivePatchHelper archive_helper(
      test_dir_.GetPath(), base::FilePath(), src, dest,
      installer::UnPackConsumer::SETUP_EXE_PATCH);
  archive_helper.set_last_uncompressed_file(patch);
  // Should fail, but not crash.
  EXPECT_FALSE(archive_helper.BinaryPatch());
}
