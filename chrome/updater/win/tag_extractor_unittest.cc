// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/win/tag_extractor.h"
#include "chrome/updater/win/tag_extractor_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(TagExtractorTest, UntaggedExe) {
  ASSERT_TRUE(ExtractTagFromFile(test::GetTestFilePath("signed.exe").value(),
                                 TagEncoding::kUtf8)
                  .empty());
}

TEST(TagExtractorTest, TaggedExeEncodeUtf8) {
  ASSERT_STREQ(ExtractTagFromFile(
                   test::GetTestFilePath("tagged_encode_utf8.exe").value(),
                   TagEncoding::kUtf8)
                   .c_str(),
               "TestTag123");
}

TEST(TagExtractorTest, TaggedExeMagicUtf16) {
  ASSERT_STREQ(ExtractTagFromFile(
                   test::GetTestFilePath("tagged_magic_utf16.exe").value(),
                   TagEncoding::kUtf16)
                   .c_str(),
               "TestTag123");
}

}  // namespace updater
