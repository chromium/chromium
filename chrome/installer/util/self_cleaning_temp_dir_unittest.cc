// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/self_cleaning_temp_dir.h"

#include <windows.h>

#include <stdint.h>
#include <wincrypt.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns a string of 8 characters consisting of the letter 'R' followed by
// seven random hex digits.
std::string GetRandomFilename() {
  uint8_t data[4];
  HCRYPTPROV crypt_ctx = 0;

  // Get four bytes of randomness.  Use CAPI rather than the CRT since I've
  // seen the latter trivially repeat.
  EXPECT_NE(FALSE, CryptAcquireContext(&crypt_ctx, nullptr, nullptr,
                                       PROV_RSA_FULL, CRYPT_VERIFYCONTEXT));
  EXPECT_NE(FALSE, CryptGenRandom(crypt_ctx, std::size(data), &data[0]));
  EXPECT_NE(FALSE, CryptReleaseContext(crypt_ctx, 0));

  // Hexify the value.
  std::string result = base::HexEncode(data);
  EXPECT_EQ(8u, result.size());

  // Replace the first digit with the letter 'R' (for "random", get it?).
  result[0] = 'R';

  return result;
}

}  // namespace

namespace installer {

class SelfCleaningTempDirTest : public testing::Test {};

// Test the implementation of GetTopDirToCreate when given the root of a
// volume.
TEST_F(SelfCleaningTempDirTest, TopLevel) {
  base::FilePath base_dir;
  SelfCleaningTempDir::GetTopDirToCreate(base::FilePath(L"C:\\"), &base_dir);
  EXPECT_TRUE(base_dir.empty());
}

// Test the implementation of GetTopDirToCreate when given a non-existent dir
// under the root of a volume.
TEST_F(SelfCleaningTempDirTest, TopLevelPlusOne) {
  base::FilePath base_dir;
  base::FilePath parent_dir(L"C:\\");
  parent_dir = parent_dir.AppendASCII(GetRandomFilename());
  SelfCleaningTempDir::GetTopDirToCreate(parent_dir, &base_dir);
  EXPECT_EQ(parent_dir, base_dir);
}

// Test that all intermediate dirs are cleaned up if they're empty when
// Delete() is called.
TEST_F(SelfCleaningTempDirTest, RemoveUnusedOnDelete) {
  // Make a directory in which we'll work.
  base::ScopedTempDir work_dir;
  EXPECT_TRUE(work_dir.CreateUniqueTempDir());

  // Make up some path under the temp dir.
  base::FilePath parent_temp_dir(
      work_dir.GetPath().Append(L"One").Append(L"Two"));
  SelfCleaningTempDir temp_dir;
  EXPECT_TRUE(temp_dir.Initialize(parent_temp_dir, L"Three"));
  EXPECT_EQ(parent_temp_dir.Append(L"Three"), temp_dir.path());
  EXPECT_TRUE(base::DirectoryExists(temp_dir.path()));
  EXPECT_TRUE(temp_dir.Delete());
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.Append(L"Three")));
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir));
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.DirName()));
  EXPECT_TRUE(base::DirectoryExists(parent_temp_dir.DirName().DirName()));
  EXPECT_TRUE(work_dir.Delete());
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.DirName().DirName()));
}

// Test that two clients can work in the same area.
TEST_F(SelfCleaningTempDirTest, TwoClients) {
  // Make a directory in which we'll work.
  base::ScopedTempDir work_dir;
  EXPECT_TRUE(work_dir.CreateUniqueTempDir());

  // Make up some path under the temp dir.
  base::FilePath parent_temp_dir(
      work_dir.GetPath().Append(L"One").Append(L"Two"));
  SelfCleaningTempDir temp_dir1;
  SelfCleaningTempDir temp_dir2;
  // First client is created.
  EXPECT_TRUE(temp_dir1.Initialize(parent_temp_dir, L"Three"));
  // Second client is created in the same space.
  EXPECT_TRUE(temp_dir2.Initialize(parent_temp_dir, L"Three"));
  // Both clients are where they are expected.
  EXPECT_EQ(parent_temp_dir.Append(L"Three"), temp_dir1.path());
  EXPECT_EQ(parent_temp_dir.Append(L"Three"), temp_dir2.path());
  EXPECT_TRUE(base::DirectoryExists(temp_dir1.path()));
  EXPECT_TRUE(base::DirectoryExists(temp_dir2.path()));
  // Second client goes away.
  EXPECT_TRUE(temp_dir2.Delete());
  // The first is now useless.
  EXPECT_FALSE(base::DirectoryExists(temp_dir1.path()));
  // But the intermediate dirs are still present
  EXPECT_TRUE(base::DirectoryExists(parent_temp_dir));
  // Now the first goes away.
  EXPECT_TRUE(temp_dir1.Delete());
  // And cleans up after itself.
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.Append(L"Three")));
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir));
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.DirName()));
  EXPECT_TRUE(base::DirectoryExists(parent_temp_dir.DirName().DirName()));
  EXPECT_TRUE(work_dir.Delete());
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.DirName().DirName()));
}

// Test that all intermediate dirs are cleaned up if they're empty when the
// destructor is called.
TEST_F(SelfCleaningTempDirTest, RemoveUnusedOnDestroy) {
  // Make a directory in which we'll work.
  base::ScopedTempDir work_dir;
  EXPECT_TRUE(work_dir.CreateUniqueTempDir());

  // Make up some path under the temp dir.
  base::FilePath parent_temp_dir(
      work_dir.GetPath().Append(L"One").Append(L"Two"));
  {
    SelfCleaningTempDir temp_dir;
    EXPECT_TRUE(temp_dir.Initialize(parent_temp_dir, L"Three"));
    EXPECT_EQ(parent_temp_dir.Append(L"Three"), temp_dir.path());
    EXPECT_TRUE(base::DirectoryExists(temp_dir.path()));
  }
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.Append(L"Three")));
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir));
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.DirName()));
  EXPECT_TRUE(base::DirectoryExists(parent_temp_dir.DirName().DirName()));
  EXPECT_TRUE(work_dir.Delete());
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.DirName().DirName()));
}

// Test that intermediate dirs are left behind if they're not empty when the
// destructor is called.
TEST_F(SelfCleaningTempDirTest, LeaveUsedOnDestroy) {
  static const char kHiHon[] = "hi, hon";

  // Make a directory in which we'll work.
  base::ScopedTempDir work_dir;
  EXPECT_TRUE(work_dir.CreateUniqueTempDir());

  // Make up some path under the temp dir.
  base::FilePath parent_temp_dir(
      work_dir.GetPath().Append(L"One").Append(L"Two"));
  {
    SelfCleaningTempDir temp_dir;
    EXPECT_TRUE(temp_dir.Initialize(parent_temp_dir, L"Three"));
    EXPECT_EQ(parent_temp_dir.Append(L"Three"), temp_dir.path());
    EXPECT_TRUE(base::DirectoryExists(temp_dir.path()));
    // Drop a file somewhere.
    EXPECT_TRUE(base::WriteFile(
        parent_temp_dir.AppendASCII(GetRandomFilename()), kHiHon));
  }
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.Append(L"Three")));
  EXPECT_TRUE(base::DirectoryExists(parent_temp_dir));
  EXPECT_TRUE(work_dir.Delete());
  EXPECT_FALSE(base::DirectoryExists(parent_temp_dir.DirName().DirName()));
}

}  // namespace installer
