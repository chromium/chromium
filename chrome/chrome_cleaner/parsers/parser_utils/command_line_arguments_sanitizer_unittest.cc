// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/parser_utils/command_line_arguments_sanitizer.h"

#include <shlobj.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(CommandLineArgumentsSanitizerTest, SanitizeURLArgumentsWithScheme) {
  const std::wstring kTestArguments =
      base::JoinString({L"http://www.somesite.com/hello/this/is/private",
                        L"ftp://some-ftp-site.com/private/data",
                        L"--some-flag=http://somesite.com.mx/private/data"},
                       L" ");
  std::vector<std::wstring> sanitized_arguments =
      SanitizeArguments(kTestArguments);
  std::vector<std::wstring> expected_arguments = {
      L"--some-flag=http://somesite.com.mx", L"http://www.somesite.com",
      L"ftp://some-ftp-site.com"};
  EXPECT_THAT(sanitized_arguments,
              testing::UnorderedElementsAreArray(expected_arguments));
}

TEST(CommandLineArgumentsSanitizerTest, SanitizeURLArgumentsWithoutScheme) {
  const std::wstring kTestArguments = base::JoinString(
      {L"www.somesite.com/hello/this/is/private", L"somesite.com/private/data",
       L"--some-flag=somesite.com/private/data"},
      L" ");
  std::vector<std::wstring> sanitized_arguments =
      SanitizeArguments(kTestArguments);
  std::vector<std::wstring> expected_arguments = {
      L"--some-flag=http://somesite.com", L"http://www.somesite.com",
      L"http://somesite.com"};
  EXPECT_THAT(sanitized_arguments,
              testing::UnorderedElementsAreArray(expected_arguments));
}

TEST(CommandLineArgumentsSanitizerTest, SanitizeFilePaths) {
  base::FilePath user_path;
  base::PathService::Get(CsidlToPathServiceKey(CSIDL_APPDATA), &user_path);
  const std::wstring kSanitizedUserPath = L"CSIDL_PROFILE\\appdata\\roaming";

  const std::wstring kTestArguments = base::JoinString(
      {user_path.value(), L" local/relative/path",
       L"--flag=" + user_path.value(), L" --another-flag=local/relative/path",
       L"--spaces=\"C:\\folder \\with \\spaces\"",
       L"--unicode=C:\\unicode\x0278\\folder"},
      L" ");

  std::vector<std::wstring> sanitized_arguments =
      SanitizeArguments(kTestArguments);
  std::vector<std::wstring> expected_arguments = {
      L"--another-flag=local/relative/path",
      L"--flag=" + kSanitizedUserPath,
      L"--spaces=c:\\folder \\with \\spaces",
      L"--unicode=c:\\unicode\x0278\\folder",
      kSanitizedUserPath,
      L"local/relative/path"};
  EXPECT_THAT(sanitized_arguments,
              testing::UnorderedElementsAreArray(expected_arguments));
}

TEST(CommandLineArgumentsSanitizerTest, NotUrlOrFilePathArguments) {
  const std::wstring kTestArguments =
      L"--some-flag not-a-url --some-other-flag=notaurl";
  std::vector<std::wstring> sanitized_arguments =
      SanitizeArguments(kTestArguments);
  std::vector<std::wstring> expected_arguments = {
      L"--some-flag", L"--some-other-flag=notaurl", L"not-a-url"};
  EXPECT_THAT(sanitized_arguments,
              testing::UnorderedElementsAreArray(expected_arguments));
}

}  // namespace chrome_cleaner
