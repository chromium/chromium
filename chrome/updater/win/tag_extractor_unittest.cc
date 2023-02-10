// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/updater/win/tag_extractor.h"
#include "chrome/updater/win/tag_extractor_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(TagExtractorTest, UntaggedExe) {
  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  const base::FilePath exe_path = source_path.AppendASCII("chrome")
                                      .AppendASCII("updater")
                                      .AppendASCII("test")
                                      .AppendASCII("data")
                                      .AppendASCII("signed.exe");
  std::string tag = ExtractTagFromFile(exe_path.value(), TagEncoding::kUtf8);
  ASSERT_TRUE(tag.empty());
}

TEST(TagExtractorTest, TaggedExeEncodeUtf8) {
  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  const base::FilePath exe_path = source_path.AppendASCII("chrome")
                                      .AppendASCII("updater")
                                      .AppendASCII("test")
                                      .AppendASCII("data")
                                      .AppendASCII("tagged_encode_utf8.exe");
  std::string tag = ExtractTagFromFile(exe_path.value(), TagEncoding::kUtf8);
  ASSERT_STREQ(tag.c_str(), "TestTag123");
}

TEST(TagExtractorTest, TaggedExeMagicUtf16) {
  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  const base::FilePath exe_path = source_path.AppendASCII("chrome")
                                      .AppendASCII("updater")
                                      .AppendASCII("test")
                                      .AppendASCII("data")
                                      .AppendASCII("tagged_magic_utf16.exe");
  std::string tag = ExtractTagFromFile(exe_path.value(), TagEncoding::kUtf16);
  ASSERT_STREQ(tag.c_str(), "TestTag123");
}

TEST(TagExtractorTest, AdvanceIt) {
  const std::vector<uint8_t> empty_binary;
  ASSERT_TRUE(AdvanceIt(empty_binary.begin(), 0, empty_binary.end()) ==
              empty_binary.end());

  const std::vector<uint8_t> binary(5);
  BinaryConstIt it = binary.begin();
  ASSERT_TRUE(AdvanceIt(it, 0, binary.end()) == it);
  ASSERT_TRUE(AdvanceIt(it, 4, binary.end()) == (it + 4));
  ASSERT_TRUE(AdvanceIt(it, 5, binary.end()) == binary.end());
  ASSERT_TRUE(AdvanceIt(it, 6, binary.end()) == binary.end());
}

TEST(TagExtractorTest, CheckRange) {
  const std::vector<uint8_t> empty_binary;
  ASSERT_FALSE(CheckRange(empty_binary.end(), 1, empty_binary.end()));

  const std::vector<uint8_t> binary(5);

  BinaryConstIt it = binary.begin();
  ASSERT_FALSE(CheckRange(it, 0, binary.end()));
  ASSERT_TRUE(CheckRange(it, 1, binary.end()));
  ASSERT_TRUE(CheckRange(it, 5, binary.end()));
  ASSERT_FALSE(CheckRange(it, 6, binary.end()));

  it = binary.begin() + 2;
  ASSERT_TRUE(CheckRange(it, 3, binary.end()));
  ASSERT_FALSE(CheckRange(it, 4, binary.end()));

  it = binary.begin() + 5;
  ASSERT_FALSE(CheckRange(it, 0, binary.end()));
  ASSERT_FALSE(CheckRange(it, 1, binary.end()));
}

}  // namespace updater
