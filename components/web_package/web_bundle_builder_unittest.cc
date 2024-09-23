// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_builder.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/byte_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

std::string kFallbackUrl = "https://test.example.org/";

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

std::vector<uint8_t> GetStringAsBytes(std::string_view contents) {
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
  base::span(written_size).copy_from(base::span(bundle).last<8u>());
  uint64_t written_size_int = base::U64FromBigEndian(written_size);
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
