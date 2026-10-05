// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/delivery_optimization_downloader_win.h"

#include <windows.h>

#include <deliveryoptimization.h>
#include <stdint.h>
#include <winerror.h>

#include <limits>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {
namespace {
constexpr base::FilePath::CharType kTestDirPrefix[] = FILE_PATH_LITERAL(
    "DeliveryOptimizationDownloaderWinTest_chrome_DO_(test)_");
constexpr base::FilePath::CharType kTestDirMatcher[] = FILE_PATH_LITERAL(
    "DeliveryOptimizationDownloaderWinTest_chrome_DO_(test)_*");
constexpr wchar_t kTestDownloadFilename[] = L"test_file.txt";
constexpr char kTestDownloadContent[] = "Hello, World!";
}  // namespace

class DeliveryOptimizationDownloaderWinTest : public testing::Test {
 protected:
  // Overrides from testing::Test
  void TearDown() override;

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<DeliveryOptimizationDownloader> downloader_ =
      base::MakeRefCounted<DeliveryOptimizationDownloader>(
          nullptr,
          "DeliveryOptimizationDownloaderWinTest");
};

void DeliveryOptimizationDownloaderWinTest::TearDown() {
  base::FilePath dir;
  ASSERT_TRUE(base::GetSecureTempDirectory(&dir));
  CleanupDirectoriesOlderThan(dir, kTestDirMatcher, base::Seconds(0));
}

TEST_F(DeliveryOptimizationDownloaderWinTest, CleansStaleDownloads) {
  base::FilePath download_dir_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(kTestDirPrefix, &download_dir_path));
  ASSERT_TRUE(base::WriteFile(download_dir_path.Append(kTestDownloadFilename),
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
                            nullptr, nullptr));
  download_dir.Close();
  downloader_->CleanupStaleDownloads();
  EXPECT_FALSE(base::DirectoryExists(download_dir_path));
}

TEST_F(DeliveryOptimizationDownloaderWinTest, RetainsRecentDownloads) {
  base::FilePath download_dir_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(kTestDirPrefix, &download_dir_path));
  ASSERT_TRUE(base::WriteFile(download_dir_path.Append(kTestDownloadFilename),
                              kTestDownloadContent));
  downloader_->CleanupStaleDownloads();
  EXPECT_TRUE(base::DirectoryExists(download_dir_path));
}

TEST(DeliveryOptimizationDownloaderHelpersTest, GetHttpStatusFromHresult) {
  using Downloader = DeliveryOptimizationDownloader;
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(S_OK), 0);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(E_FAIL), 0);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(HTTP_E_STATUS_NOT_FOUND), 404);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(HTTP_E_STATUS_SERVER_ERROR),
            500);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(HTTP_E_STATUS_VERSION_NOT_SUP),
            505);

  // Any 5xx code is decoded, not only the standard ones, so that it can be
  // classified as a server error.
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 511)),
            511);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 599)),
            599);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 600)),
            0);
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 99)),
            0);

  // Other facilities with a plausible low word are not HTTP statuses.
  EXPECT_EQ(Downloader::GetHttpStatusFromHresult(
                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 404)),
            0);
}

TEST(DeliveryOptimizationDownloaderHelpersTest, GetDownloadError) {
  using Downloader = DeliveryOptimizationDownloader;
  DO_DOWNLOAD_STATUS status = {};
  EXPECT_EQ(Downloader::GetDownloadError(status), S_OK);

  status.ExtendedError = HTTP_E_STATUS_SERVER_ERROR;
  EXPECT_EQ(Downloader::GetDownloadError(status), HTTP_E_STATUS_SERVER_ERROR);

  // |Error| takes precedence over |ExtendedError|.
  status.Error = E_ABORT;
  EXPECT_EQ(Downloader::GetDownloadError(status), E_ABORT);

  // Success codes are not errors.
  status.Error = S_FALSE;
  status.ExtendedError = S_FALSE;
  EXPECT_EQ(Downloader::GetDownloadError(status), S_OK);
}

TEST(DeliveryOptimizationDownloaderHelpersTest, GetDownloadByteCount) {
  using Downloader = DeliveryOptimizationDownloader;
  int64_t downloaded_bytes = 0;
  int64_t total_bytes = 0;

  // A total of zero means the size is not known yet.
  DO_DOWNLOAD_STATUS status = {};
  Downloader::GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);
  EXPECT_EQ(downloaded_bytes, 0);
  EXPECT_EQ(total_bytes, -1);

  status.BytesTransferred = 10;
  status.BytesTotal = 100;
  Downloader::GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);
  EXPECT_EQ(downloaded_bytes, 10);
  EXPECT_EQ(total_bytes, 100);

  // Values which do not fit in an int64_t are reported as unknown.
  status.BytesTransferred = std::numeric_limits<uint64_t>::max();
  status.BytesTotal = std::numeric_limits<uint64_t>::max();
  Downloader::GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);
  EXPECT_EQ(downloaded_bytes, -1);
  EXPECT_EQ(total_bytes, -1);
}

}  // namespace update_client
