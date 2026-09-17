// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/browser_info.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AssertParseBrowserInfoFails(const std::string& data) {
  BrowserInfo browser_info;
  Status status = BrowserInfo::ParseBrowserInfo(data, &browser_info);
  ASSERT_TRUE(status.IsError());
}

std::string CreateBrowserInfoJson(std::string_view extra_fields) {
  return base::StrCat(
      {"{\"Browser\": \"Chrome/37.0.2062.124\", \"WebKit-Version\": "
       "\"537.36 (@181352)\"",
       extra_fields, "}"});
}

}  // namespace

TEST(ParseBrowserInfo, InvalidJSON) {
  AssertParseBrowserInfoFails("[");
}

TEST(ParseBrowserInfo, NonDict) {
  AssertParseBrowserInfoFails("[]");
}

TEST(ParseBrowserInfo, NoBrowserKey) {
  AssertParseBrowserInfoFails("{}");
}

TEST(ParseBrowserInfo, BlinkVersionContainsSvnRevision) {
  std::string data("{\"Browser\": \"Chrome/37.0.2062.124\","
                   " \"WebKit-Version\": \"537.36 (@181352)\"}");
  BrowserInfo browser_info;
  Status status = browser_info.ParseBrowserInfo(data);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_info.browser_name);
  ASSERT_EQ("37.0.2062.124", browser_info.browser_version);
  ASSERT_EQ(37, browser_info.major_version);
  ASSERT_EQ(2062, browser_info.build_no);
  ASSERT_EQ(181352, browser_info.blink_revision);
  ASSERT_FALSE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(ParseBrowserInfo, RecognizesFilePathStyle) {
  struct TestCase {
    const char* file_path_style;
    FilePathStyle expected_file_path_style;
  };
  constexpr TestCase kTestCases[] = {
      {"windows", FilePathStyle::kWindows},
      {"posix", FilePathStyle::kPosix},
  };

  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.file_path_style);
    BrowserInfo browser_info;
    Status status =
        browser_info.ParseBrowserInfo(CreateBrowserInfoJson(base::StrCat(
            {R"(, "File-Path-Style": ")", test_case.file_path_style, R"(")"})));
    ASSERT_TRUE(status.IsOk());
    EXPECT_EQ(test_case.expected_file_path_style, browser_info.file_path_style);
  }
}

TEST(ParseBrowserInfo, MissingFilePathStyleIsNonFatal) {
  BrowserInfo browser_info;
  browser_info.file_path_style = FilePathStyle::kWindows;
  Status status = browser_info.ParseBrowserInfo(CreateBrowserInfoJson(""));
  ASSERT_TRUE(status.IsOk());
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(ParseBrowserInfo, UnrecognizedFilePathStyleIsNonFatal) {
  BrowserInfo browser_info;
  browser_info.file_path_style = FilePathStyle::kWindows;
  Status status = browser_info.ParseBrowserInfo(
      CreateBrowserInfoJson(R"(, "File-Path-Style": "native")"));
  ASSERT_TRUE(status.IsOk());
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(ParseBrowserInfo, EmptyFilePathStyleIsNonFatal) {
  BrowserInfo browser_info;
  browser_info.file_path_style = FilePathStyle::kWindows;
  Status status = browser_info.ParseBrowserInfo(
      CreateBrowserInfoJson(R"(, "File-Path-Style": "")"));
  ASSERT_TRUE(status.IsOk());
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(ParseBrowserInfo, NonStringFilePathStyleIsNonFatal) {
  BrowserInfo browser_info;
  browser_info.file_path_style = FilePathStyle::kWindows;
  Status status = browser_info.ParseBrowserInfo(
      CreateBrowserInfoJson(R"(, "File-Path-Style": 42)"));
  ASSERT_TRUE(status.IsOk());
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(ParseBrowserInfo, AndroidPackageForcesPosixFilePathStyle) {
  BrowserInfo browser_info;
  Status status = browser_info.ParseBrowserInfo(
      CreateBrowserInfoJson(R"(, "Android-Package": "com.android.chrome", )"
                            R"("File-Path-Style": "windows")"));
  ASSERT_TRUE(status.IsOk());
  EXPECT_TRUE(browser_info.is_android);
  EXPECT_EQ(FilePathStyle::kPosix, browser_info.file_path_style);
}

TEST(ParseBrowserInfo, BlinkVersionContainsGitHash) {
  std::string data("{\"Browser\": \"Chrome/37.0.2062.124\","
                   " \"WebKit-Version\":"
                   " \"537.36 (@28f741cfcabffe68a9c12c4e7152569c906bd88f)\"}");
  BrowserInfo browser_info;
  const int default_blink_revision = browser_info.blink_revision;
  Status status = browser_info.ParseBrowserInfo(data);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_info.browser_name);
  ASSERT_EQ("37.0.2062.124", browser_info.browser_version);
  ASSERT_EQ(37, browser_info.major_version);
  ASSERT_EQ(2062, browser_info.build_no);
  ASSERT_EQ(default_blink_revision, browser_info.blink_revision);
}

TEST(ParseBrowserString, KitKatWebView) {
  BrowserInfo browser_info;
  Status status = BrowserInfo::ParseBrowserString(
      false, "Version/4.0 Chrome/30.0.0.0", &browser_info);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("webview", browser_info.browser_name);
  ASSERT_EQ("30.0.0.0", browser_info.browser_version);
  ASSERT_EQ(30, browser_info.major_version);
  ASSERT_EQ(kToTBuildNo, browser_info.build_no);
  ASSERT_TRUE(browser_info.is_android);
  ASSERT_FALSE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kPosix, browser_info.file_path_style);
}

TEST(ParseBrowserString, LollipopWebView) {
  BrowserInfo browser_info;
  Status status =
      BrowserInfo::ParseBrowserString(true, "Chrome/37.0.0.0", &browser_info);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("webview", browser_info.browser_name);
  ASSERT_EQ("37.0.0.0", browser_info.browser_version);
  ASSERT_EQ(37, browser_info.major_version);
  ASSERT_EQ(kToTBuildNo, browser_info.build_no);
  ASSERT_TRUE(browser_info.is_android);
  ASSERT_FALSE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kPosix, browser_info.file_path_style);
}

TEST(ParseBrowserString, AndroidChrome) {
  BrowserInfo browser_info;
  Status status = BrowserInfo::ParseBrowserString(true, "Chrome/39.0.2171.59",
                                                  &browser_info);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_info.browser_name);
  ASSERT_EQ("39.0.2171.59", browser_info.browser_version);
  ASSERT_EQ(39, browser_info.major_version);
  ASSERT_EQ(2171, browser_info.build_no);
  ASSERT_TRUE(browser_info.is_android);
  ASSERT_FALSE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kPosix, browser_info.file_path_style);
}

TEST(ParseBrowserString, DesktopChrome) {
  BrowserInfo browser_info;
  Status status = BrowserInfo::ParseBrowserString(false, "Chrome/39.0.2171.59",
                                                  &browser_info);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_info.browser_name);
  ASSERT_EQ("39.0.2171.59", browser_info.browser_version);
  ASSERT_EQ(39, browser_info.major_version);
  ASSERT_EQ(2171, browser_info.build_no);
  ASSERT_FALSE(browser_info.is_android);
  ASSERT_FALSE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(ParseBrowserString, HeadlessChrome) {
  BrowserInfo browser_info;
  Status status = BrowserInfo::ParseBrowserString(
      false, "HeadlessChrome/39.0.2171.59", &browser_info);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome-headless-shell", browser_info.browser_name);
  ASSERT_EQ("39.0.2171.59", browser_info.browser_version);
  ASSERT_EQ(39, browser_info.major_version);
  ASSERT_EQ(2171, browser_info.build_no);
  ASSERT_FALSE(browser_info.is_android);
  ASSERT_TRUE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(ParseBlinkVersionString, GitHash) {
  int rev = -1;
  Status status = BrowserInfo::ParseBlinkVersionString(
      "537.36 (@28f741cfcabffe68a9c12c4e7152569c906bd88f)", &rev);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(-1, rev);
}

TEST(ParseBlinkVersionString, SvnRevision) {
  int blink_revision = -1;
  Status status =
      BrowserInfo::ParseBlinkVersionString("537.36 (@159105)", &blink_revision);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(159105, blink_revision);
}

TEST(IsGitHash, GitHash) {
  ASSERT_TRUE(
      BrowserInfo::IsGitHash("28f741cfcabffe68a9c12c4e7152569c906bd88f"));
}

TEST(IsGitHash, ShortGitHash) {
  ASSERT_TRUE(BrowserInfo::IsGitHash("1493aa5"));
}

TEST(IsGitHash, GitHashWithUpperCaseCharacters) {
  ASSERT_TRUE(
      BrowserInfo::IsGitHash("28F741CFCABFFE68A9C12C4E7152569C906BD88F"));
}

TEST(IsGitHash, SvnRevision) {
  ASSERT_FALSE(BrowserInfo::IsGitHash("159105"));
}

TEST(FillFromBrowserVersionResponse, Chrome) {
  base::DictValue response;
  response.Set("product", "Chrome/37.0.2062.124");
  BrowserInfo browser_info;
  Status status = browser_info.FillFromBrowserVersionResponse(response);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_info.browser_name);
  ASSERT_EQ("37.0.2062.124", browser_info.browser_version);
  ASSERT_EQ(37, browser_info.major_version);
  ASSERT_EQ(2062, browser_info.build_no);
  ASSERT_FALSE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}

TEST(FillFromBrowserVersionResponse, HeadlessChrome) {
  base::DictValue response;
  response.Set("product", "HeadlessChrome/39.0.2171.59");
  BrowserInfo browser_info;
  Status status = browser_info.FillFromBrowserVersionResponse(response);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome-headless-shell", browser_info.browser_name);
  ASSERT_EQ("39.0.2171.59", browser_info.browser_version);
  ASSERT_EQ(39, browser_info.major_version);
  ASSERT_EQ(2171, browser_info.build_no);
  ASSERT_TRUE(browser_info.is_headless_shell);
  EXPECT_EQ(FilePathStyle::kUnknown, browser_info.file_path_style);
}
