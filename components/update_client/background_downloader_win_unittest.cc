// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/update_client/background_downloader_win.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/win/windows_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {
namespace {
constexpr base::FilePath::CharType kTestDirPrefix[] =
    FILE_PATH_LITERAL("chrome_BITS_(test)_");
constexpr base::FilePath::CharType kTestDirMatcher[] =
    FILE_PATH_LITERAL("chrome_BITS_(test)_*");
constexpr char kTestDownloadFilename[] = "test_file.txt";
constexpr char kTestDownloadContent[] = "Hello, World!";
}  // namespace

class BackgroundDownloaderWinTest : public testing::Test {
 public:
  BackgroundDownloaderWinTest() = default;
  ~BackgroundDownloaderWinTest() override = default;

  // Overrides from testing::Test
  void TearDown() override;

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<BackgroundDownloader> downloader_ =
      base::MakeRefCounted<BackgroundDownloader>(nullptr);
};

void BackgroundDownloaderWinTest::TearDown() {
  downloader_->EnumerateDownloadDirs(
      kTestDirMatcher,
      [](const base::FilePath& dir) { base::DeletePathRecursively(dir); });
}

TEST_F(BackgroundDownloaderWinTest, CleansStaleDownloads) {
  base::FilePath download_dir_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(kTestDirPrefix, &download_dir_path));
  ASSERT_TRUE(
      base::WriteFile(download_dir_path.AppendASCII(kTestDownloadFilename),
                      kTestDownloadContent));

  // Manipulate the creation time of the directory.
  FILETIME creation_filetime =
      (base::Time::NowFromSystemTime() - base::Days(5)).ToFileTime();
  base::File download_dir(download_dir_path,
                          base::File::FLAG_OPEN |
                              base::File::FLAG_WIN_BACKUP_SEMANTICS |
                              base::File::FLAG_WRITE_ATTRIBUTES);
  ASSERT_TRUE(download_dir.IsValid());
  ASSERT_TRUE(::SetFileTime(download_dir.GetPlatformFile(), &creation_filetime,
                            NULL, NULL));
  download_dir.Close();
  downloader_->CleanupStaleDownloads();
  EXPECT_FALSE(base::DirectoryExists(download_dir_path));
}

TEST_F(BackgroundDownloaderWinTest, RetainsRecentDownloads) {
  base::FilePath download_dir_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(kTestDirPrefix, &download_dir_path));
  ASSERT_TRUE(
      base::WriteFile(download_dir_path.AppendASCII(kTestDownloadFilename),
                      kTestDownloadContent));
  downloader_->CleanupStaleDownloads();
  EXPECT_TRUE(base::DirectoryExists(download_dir_path));
}

}  // namespace update_client
