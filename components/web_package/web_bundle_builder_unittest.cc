// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_builder.h"

#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

std::string kFallbackUrl = "https://test.example.org/";

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

std::vector<uint8_t> GetStringAsBytes(base::StringPiece contents) {
  auto bytes = base::as_bytes(base::make_span(contents));
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

}  // namespace

class WebBundleBuilderTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleBuilderTest, CorrectWebBundleSizeIsWritten) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  uint8_t written_size[8];
  memcpy(written_size, bundle.data() + bundle.size() - 8, 8);
  uint64_t written_size_int;
  base::ReadBigEndian(written_size, &written_size_int);
  EXPECT_EQ(bundle.size(), written_size_int);
}

TEST_F(WebBundleBuilderTest, ByteByByteComparison) {
  WebBundleBuilder builder;
  builder.AddExchange(
      "https://test.example.org/",
      {{":status", "200"}, {"content-type", "text/html; charset=UTF-8"}},
      "<a href='index.html'>click for web bundles</a>");
  builder.AddExchange(
      "https://test.example.org/index.html",
      {{":status", "200"}, {"content-type", "text/html; charset=UTF-8"}},
      "<p>Hello Web Bundles!</p>");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  std::vector<uint8_t> expected_bundle = GetStringAsBytes(
      GetTestFileContents(base::FilePath(FILE_PATH_LITERAL("simple_b2.wbn"))));
  EXPECT_EQ(bundle, expected_bundle);
}

TEST_F(WebBundleBuilderTest, MoreThan23ResponsesInABundle) {
  WebBundleBuilder builder;
  for (int i = 0; i < 24; ++i) {
    builder.AddExchange("https://test.example.org/" + base::NumberToString(i),
                        {{":status", "200"}, {"content-type", "text/html;"}},
                        "<p>Hello Web Bundles!</p>");
  }
  std::vector<uint8_t> bundle = builder.CreateBundle();
  std::vector<uint8_t> expected_bundle = GetStringAsBytes(GetTestFileContents(
      base::FilePath(FILE_PATH_LITERAL("24_responses.wbn"))));
  EXPECT_EQ(bundle, expected_bundle);
}

}  // namespace web_package
