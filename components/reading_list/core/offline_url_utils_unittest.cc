// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/offline_url_utils.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Checks the root directory of offline pages.
TEST(OfflineURLUtilsTest, OfflineRootDirectoryPathTest) {
  base::FilePath::StringType separator(&base::FilePath::kSeparators[0], 1);
  base::FilePath profile_path(FILE_PATH_LITERAL("profile_path"));
  base::FilePath offline_directory =
      reading_list::OfflineRootDirectoryPath(profile_path);
  // Expected value: profile_path/Offline
  std::string expected = base::StringPrintf(
      "profile_path%" PRFilePath "Offline", separator.c_str());
  EXPECT_EQ(expected, offline_directory.AsUTF8Unsafe());
}

// Checks the offline page directory is the MD5 of the URL
TEST(OfflineURLUtilsTest, OfflineURLDirectoryIDTest) {
  GURL url("http://www.google.com/test");
  // MD5 of "http://www.google.com/test"
  std::string md5 = "0090071ef710946a1263c276284bb3b8";
  std::string directory_id = reading_list::OfflineURLDirectoryID(url);
  EXPECT_EQ(md5, directory_id);
}

// Checks the offline page directory is
// |profile_path|/Offline/OfflineURLDirectoryID;
TEST(OfflineURLUtilsTest, OfflineURLDirectoryAbsolutePathTest) {
  base::FilePath::StringType separator(&base::FilePath::kSeparators[0], 1);
  base::FilePath profile_path(FILE_PATH_LITERAL("profile_path"));
  GURL url("http://www.google.com/test");
  base::FilePath offline_directory =
      reading_list::OfflineURLDirectoryAbsolutePath(profile_path, url);
  // Expected value: profile_path/Offline/0090071ef710946a1263c276284bb3b8
  std::string expected =
      base::StringPrintf("profile_path%" PRFilePath "Offline%" PRFilePath
                         "0090071ef710946a1263c276284bb3b8",
                         separator.c_str(), separator.c_str());
  EXPECT_EQ(expected, offline_directory.AsUTF8Unsafe());
}

// Checks the offline page directory is
// |profile_path|/Offline/OfflineURLDirectoryID;
TEST(OfflineURLUtilsTest, AbsolutePathForRelativePathTest) {
  base::FilePath::StringType separator(&base::FilePath::kSeparators[0], 1);
  base::FilePath profile_path(FILE_PATH_LITERAL("profile_path"));
  base::FilePath relative_path(FILE_PATH_LITERAL("relative"));
  relative_path = relative_path.Append(FILE_PATH_LITERAL("path"));
  base::FilePath absolute_path =
      reading_list::OfflineURLAbsolutePathFromRelativePath(profile_path,
                                                           relative_path);
  // Expected value: profile_path/Offline/relative/path
  std::string expected = base::StringPrintf(
      "profile_path%" PRFilePath "Offline%" PRFilePath "relative%" PRFilePath
      "path",
      separator.c_str(), separator.c_str(), separator.c_str());
  EXPECT_EQ(expected, absolute_path.AsUTF8Unsafe());
}

// Checks the offline page path is OfflineURLDirectoryID/page.html;
TEST(OfflineURLUtilsTest, OfflinePagePathTest) {
  base::FilePath::StringType separator(&base::FilePath::kSeparators[0], 1);
  GURL url("http://www.google.com/test");
  base::FilePath offline_page =
      reading_list::OfflinePagePath(url, reading_list::OFFLINE_TYPE_HTML);
  // Expected value: 0090071ef710946a1263c276284bb3b8/page.html
  std::string expected_html = base::StringPrintf(
      "0090071ef710946a1263c276284bb3b8%" PRFilePath "page.html",
      separator.c_str());
  EXPECT_EQ(expected_html, offline_page.AsUTF8Unsafe());
  offline_page =
      reading_list::OfflinePagePath(url, reading_list::OFFLINE_TYPE_PDF);
  // Expected value: 0090071ef710946a1263c276284bb3b8/file.pdf
  std::string expected_pdf = base::StringPrintf(
      "0090071ef710946a1263c276284bb3b8%" PRFilePath "file.pdf",
      separator.c_str());
  EXPECT_EQ(expected_pdf, offline_page.AsUTF8Unsafe());
}
