// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_system_access_ui_helper {

namespace {

struct UnaryTestData {
  base::FilePath::StringType input;
  std::u16string expected;
};

static const struct UnaryTestData cases[] = {
    {FILE_PATH_LITERAL(""), u""},
    {FILE_PATH_LITERAL("aa"), u"aa"},
    {FILE_PATH_LITERAL("/aa/bb"), u"bb"},
    {FILE_PATH_LITERAL("/aa/bb/"), u"bb"},
    {FILE_PATH_LITERAL("/aa/bb//"), u"bb"},
    {FILE_PATH_LITERAL("/aa/bb/ccc"), u"ccc"},
    {FILE_PATH_LITERAL("/aa"), u"aa"},
    {FILE_PATH_LITERAL("/"), u"/"},
    {FILE_PATH_LITERAL("//"), u"//"},
    {FILE_PATH_LITERAL("///"), u"/"},
    {FILE_PATH_LITERAL("aa/"), u"aa"},
    {FILE_PATH_LITERAL("aa/bb"), u"bb"},
    {FILE_PATH_LITERAL("aa/bb/"), u"bb"},
    {FILE_PATH_LITERAL("aa/bb//"), u"bb"},
    {FILE_PATH_LITERAL("aa//bb//"), u"bb"},
    {FILE_PATH_LITERAL("aa//bb/"), u"bb"},
    {FILE_PATH_LITERAL("aa//bb"), u"bb"},
    {FILE_PATH_LITERAL("//aa/bb"), u"bb"},
    {FILE_PATH_LITERAL("//aa/"), u"aa"},
    {FILE_PATH_LITERAL("//aa"), u"aa"},
    {FILE_PATH_LITERAL("0:"), u"0:"},
    {FILE_PATH_LITERAL("@:"), u"@:"},
    {FILE_PATH_LITERAL("[:"), u"[:"},
    {FILE_PATH_LITERAL("`:"), u"`:"},
    {FILE_PATH_LITERAL("{:"), u"{:"},
#if BUILDFLAG(IS_WIN)
    {FILE_PATH_LITERAL("\u0143:"), u"\u0143:"},
#endif  // BUILDFLAG(IS_WIN)
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    {FILE_PATH_LITERAL("c:"), u"c:"},
    {FILE_PATH_LITERAL("C:"), u"C:"},
    {FILE_PATH_LITERAL("A:"), u"A:"},
    {FILE_PATH_LITERAL("Z:"), u"Z:"},
    {FILE_PATH_LITERAL("a:"), u"a:"},
    {FILE_PATH_LITERAL("z:"), u"z:"},
    {FILE_PATH_LITERAL("c:aa"), u"aa"},
    {FILE_PATH_LITERAL("c:/"), u"c:/"},
    {FILE_PATH_LITERAL("c://"), u"c://"},
    {FILE_PATH_LITERAL("c:///"), u"/"},
    {FILE_PATH_LITERAL("c:/aa"), u"aa"},
    {FILE_PATH_LITERAL("c:/aa/"), u"aa"},
    {FILE_PATH_LITERAL("c:/aa/bb"), u"bb"},
    {FILE_PATH_LITERAL("c:aa/bb"), u"bb"},
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    {FILE_PATH_LITERAL("\\aa\\bb"), u"bb"},
    {FILE_PATH_LITERAL("\\aa\\bb\\"), u"bb"},
    {FILE_PATH_LITERAL("\\aa\\bb\\\\"), u"bb"},
    {FILE_PATH_LITERAL("\\aa\\bb\\ccc"), u"ccc"},
    {FILE_PATH_LITERAL("\\aa"), u"aa"},
    {FILE_PATH_LITERAL("\\"), u"\\"},
    {FILE_PATH_LITERAL("\\\\"), u"\\\\"},
    {FILE_PATH_LITERAL("\\\\\\"), u"\\"},
    {FILE_PATH_LITERAL("aa\\"), u"aa"},
    {FILE_PATH_LITERAL("aa\\bb"), u"bb"},
    {FILE_PATH_LITERAL("aa\\bb\\"), u"bb"},
    {FILE_PATH_LITERAL("aa\\bb\\\\"), u"bb"},
    {FILE_PATH_LITERAL("aa\\\\bb\\\\"), u"bb"},
    {FILE_PATH_LITERAL("aa\\\\bb\\"), u"bb"},
    {FILE_PATH_LITERAL("aa\\\\bb"), u"bb"},
    {FILE_PATH_LITERAL("\\\\aa\\bb"), u"bb"},
    {FILE_PATH_LITERAL("\\\\aa\\"), u"aa"},
    {FILE_PATH_LITERAL("\\\\aa"), u"aa"},
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    {FILE_PATH_LITERAL("c:\\"), u"c:\\"},
    {FILE_PATH_LITERAL("c:\\\\"), u"c:\\\\"},
    {FILE_PATH_LITERAL("c:\\\\\\"), u"\\"},
    {FILE_PATH_LITERAL("c:\\aa"), u"aa"},
    {FILE_PATH_LITERAL("c:\\aa\\"), u"aa"},
    {FILE_PATH_LITERAL("c:\\aa\\bb"), u"bb"},
    {FILE_PATH_LITERAL("c:aa\\bb"), u"bb"},
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
};

static const struct UnaryTestData elided_cases[] = {
    {FILE_PATH_LITERAL("spaces are elided.txt                        .exe"),
     u"spaces are elided.txt\u2026.exe"},
};

}  // namespace

class FileSystemAccessUIHelpersTest : public testing::Test {};

TEST_F(FileSystemAccessUIHelpersTest, GetPathForDisplayAsParagraph) {
  content::PathInfo input(base::FilePath(FILE_PATH_LITERAL("path")), "display");
#if BUILDFLAG(IS_ANDROID)
  // Android content-URIs should use display-name.
  input.path = base::FilePath("content://auth/path");
  EXPECT_EQ(u"display", GetPathForDisplayAsParagraph(input));
#endif

  for (const auto& i : cases) {
    input.path = base::FilePath(i.input);
    std::u16string observed = GetPathForDisplayAsParagraph(input);
    EXPECT_EQ(i.expected, observed)
        << "input: " << i.input
        << ", expected: " << base::UTF16ToUTF8(i.expected);
  }
}

TEST_F(FileSystemAccessUIHelpersTest, GetElidedPathForDisplayAsTitle) {
  content::PathInfo input(base::FilePath(FILE_PATH_LITERAL("path")), "display");
#if BUILDFLAG(IS_ANDROID)
  // Android content-URIs should use display-name.
  input.path = base::FilePath("content://auth/path");
  EXPECT_EQ(u"display", GetElidedPathForDisplayAsTitle(input));
#endif

  for (const auto& i : cases) {
    input.path = base::FilePath(i.input);
    std::u16string observed = GetElidedPathForDisplayAsTitle(input);
    EXPECT_EQ(i.expected, observed)
        << "input: " << i.input
        << ", expected: " << base::UTF16ToUTF8(i.expected);
  }

  for (const auto& i : elided_cases) {
    input.path = base::FilePath(i.input);
    std::u16string observed = GetElidedPathForDisplayAsTitle(input);
    EXPECT_EQ(i.expected, observed)
        << "input: " << i.input
        << ", expected: " << base::UTF16ToUTF8(i.expected);
  }
}

}  // namespace file_system_access_ui_helper
