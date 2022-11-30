// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_util.h"

#include "chrome/chrome_cleaner/os/disk_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// The following include is needed to use EXPECT_NONFATAL_FAILURE.
#include "testing/gtest/include/gtest/gtest-spi.h"

namespace chrome_cleaner {

namespace {
const wchar_t kFileName1[] = L"test_path1";
const wchar_t kFileName2[] = L"test_path2";

}  // namespace

TEST(TestUtilTest, ExpectEqualFilePathSets) {
  // Messages are logged to a vector for testing.
  LoggingOverride logger;
  FilePathSet matched_files;
  FilePathSet expected_files;

  matched_files.Insert(base::FilePath(kFileName1));
  EXPECT_NONFATAL_FAILURE(
      ExpectEqualFilePathSets(matched_files, expected_files), "");
  EXPECT_TRUE(logger.LoggingMessagesContain(
      "Unexpected file in matched footprints: 'test_path1'"));

  logger.FlushMessages();
  expected_files.Insert(base::FilePath(kFileName1));
  ExpectEqualFilePathSets(matched_files, expected_files);

  logger.FlushMessages();
  expected_files.Insert(base::FilePath(kFileName2));
  EXPECT_NONFATAL_FAILURE(
      ExpectEqualFilePathSets(matched_files, expected_files), "");
  EXPECT_TRUE(logger.LoggingMessagesContain(
      "Missing expected footprint: 'test_path2'"));
}

}  // namespace chrome_cleaner
