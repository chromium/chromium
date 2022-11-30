// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_type_util.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace download_type_util {

TEST(DownloadProtectionUtilTest, KnownValues) {
  EXPECT_EQ(ClientDownloadRequest::WIN_EXECUTABLE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.exe"))));
  EXPECT_EQ(ClientDownloadRequest::CHROME_EXTENSION,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.crx"))));
  EXPECT_EQ(ClientDownloadRequest::ZIPPED_EXECUTABLE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.zip"))));
  EXPECT_EQ(ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.rar"))));
  EXPECT_EQ(ClientDownloadRequest::MAC_EXECUTABLE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.pkg"))));
  EXPECT_EQ(ClientDownloadRequest::ANDROID_APK,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.apk"))));
}

}  // namespace download_type_util
}  // namespace safe_browsing
