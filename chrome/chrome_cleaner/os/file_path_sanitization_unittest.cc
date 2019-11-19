// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

#include <shlobj.h>

#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

base::string16 FirstComponent(const base::string16& original) {
  return original.substr(0, original.find(L"\\"));
}

TEST(FilePathSanitizationTests, NormalizePath) {
  base::FilePath expected_path =
      base::FilePath(L"c:\\program files\\desktop.ini");
  EXPECT_EQ(NormalizePath(base::FilePath(L"C:\\PROGRA~1\\DESKTOP.INI")),
            expected_path);
  EXPECT_EQ(NormalizePath(base::FilePath(L"c:\\pRoGrAm FiLeS\\desktop.INI")),
            expected_path);
  base::FilePath empty_path;
  EXPECT_EQ(NormalizePath(empty_path), empty_path);
}

TEST(FilePathSanitizationTests, NormalizePathUnicode) {
  EXPECT_EQ(
      NormalizePath(
          base::FilePath(L"C:\\\u03b1\u03c1\u03c7\u03b5\u03b9\u03b1 "
                         L"\u03c0\u03c1\u03bf\u03b3\u03c1"
                         L"\u03b1\u03bc\u03bc\u03b1\u03c4\u03bf\u03c2\\u03b5"
                         L"\u03c0\u03b9\u03c6"
                         L"\u03ac\u03bd\u03b5\u03b9\u03b1 "
                         L"\u03b5\u03c1\u03b3\u03b1\u03c3\u03af"
                         L"\u03b1\u03c2.iNi"))
          .value(),
      L"c:\\\u03b1\u03c1\u03c7\u03b5\u03b9\u03b1 \u03c0\u03c1\u03bf\u03b3\u03c1"
      L"\u03b1\u03bc\u03bc\u03b1\u03c4\u03bf\u03c2\\u03b5\u03c0\u03b9\u03c6"
      L"\u03ac\u03bd\u03b5\u03b9\u03b1 \u03b5\u03c1\u03b3\u03b1\u03c3\u03af"
      L"\u03b1\u03c2.ini");
}
TEST(FilePathSanitizationTests, SanitizePath) {
  base::FilePath programfiles_folder;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_PROGRAM_FILES),
                                     &programfiles_folder));

  base::FilePath absolute(L"C:\\Dummy\\Dummy.exe");
  base::string16 result = SanitizePath(absolute);
  EXPECT_EQ(NormalizePath(absolute).value(), result);

  base::FilePath relative(programfiles_folder.Append(L"Dummy\\Dummy.exe"));
  base::string16 sanitized_relative = SanitizePath(relative);
  EXPECT_NE(sanitized_relative, relative.value());
  EXPECT_EQ(L"CSIDL_PROGRAM_FILES\\dummy\\dummy.exe", sanitized_relative);

  base::FilePath empty_path;
  EXPECT_EQ(L"", SanitizePath(empty_path));
}

TEST(FilePathSanitizationTests, SanitizePathConsistency) {
  // Loop over all the rewrite rules used by sanitize path to make sure all the
  // rules work correctly. In particular this test verifies each rule is not
  // masked by another.
  base::FilePath arbitrary_path = NormalizePath(base::FilePath(L"Desktop.ini"));
  for (auto* rule = sanitization_internal::rewrite_rules; rule->path != nullptr;
       ++rule) {
    base::FilePath expanded_path;
    base::PathService::Get(rule->id, &expanded_path);
    expanded_path = expanded_path.Append(arbitrary_path);
    const auto sanitized_path = chrome_cleaner::SanitizePath(expanded_path);

    // The FirstComponent here is the label string used to sanitize the path. It
    // is extracted to verify the correct label string is being used.
    //
    // For example:
    // C:\Program Files (x86)\Common Files\Desktop.ini
    // maps to
    // CSIDL_PROGRAM_FILES_COMMON (CSIDL_PROGRAM_FILES_COMMON\Desktop.ini)
    // and shouldn't map to
    // CSIDL_PROGRAM_FILES        (CSIDL_PROGRAM_FILES\Common Files\Desktop.ini)
    // or it will clash with
    // C:\Program Files (x86)\Desktop.ini
    // which maps to
    // CSIDL_PROGRAM_FILES        (CSIDL_PROGRAM_FILES\desktop.ini)
    const auto first_dir = FirstComponent(sanitized_path);
    if (first_dir != rule->path) {
      ADD_FAILURE() << base::WideToUTF8(expanded_path.value())
                    << " is being Sanitized to "
                    << base::WideToUTF8(sanitized_path) << " instead of using "
                    << rule->path;
    }
  }
}

TEST(FilePathSanitizationTests, SanitizeCommandLine) {
  base::CommandLine switches =
      base::CommandLine::FromString(L"dummy.exe --arg=value --flag ");

  base::CommandLine already_sanitized(switches);
  already_sanitized.SetProgram(base::FilePath(L"c:\\dummy\\dummy.exe"));
  base::string16 result = SanitizeCommandLine(already_sanitized);
  EXPECT_EQ(already_sanitized.GetCommandLineString(), result);

  base::FilePath programfiles_folder;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_PROGRAM_FILES),
                                     &programfiles_folder));
  base::FilePath exe_in_programfiles =
      programfiles_folder.Append(L"dummy\\dummy.exe");

  base::CommandLine to_sanitize(switches);
  to_sanitize.SetProgram(exe_in_programfiles);
  base::string16 sanitized_cmd = SanitizeCommandLine(to_sanitize);
  EXPECT_NE(to_sanitize.GetCommandLineString(), sanitized_cmd);
  EXPECT_EQ(sanitized_cmd.find(exe_in_programfiles.value()),
            base::string16::npos)
      << sanitized_cmd;

  switches.AppendSwitchPath("path", exe_in_programfiles);
  switches.AppendArgPath(exe_in_programfiles);
  to_sanitize = base::CommandLine(switches);
  sanitized_cmd = SanitizeCommandLine(to_sanitize);
  EXPECT_NE(to_sanitize.GetCommandLineString(), sanitized_cmd);
  EXPECT_EQ(sanitized_cmd.find(exe_in_programfiles.value()),
            base::string16::npos)
      << sanitized_cmd;
}

TEST(FilePathSanitizationTests, ExpandSpecialFolderPath) {
  base::FilePath arbitrary_path(L"Desktop.ini");
  for (auto* rule = sanitization_internal::rewrite_rules; rule->path != nullptr;
       ++rule) {
    // Skip non-CSIDL entries.
    if (rule->id < sanitization_internal::PATH_CSIDL_START ||
        rule->id >= sanitization_internal::PATH_CSIDL_END) {
      continue;
    }

    // Fetch and validate expected path.
    base::FilePath expected_path;
    ASSERT_TRUE(base::PathService::Get(rule->id, &expected_path));
    ASSERT_FALSE(expected_path.empty());
    expected_path = expected_path.Append(arbitrary_path);

    int csidl = rule->id - sanitization_internal::PATH_CSIDL_START;
    base::FilePath expanded_path =
        ExpandSpecialFolderPath(csidl, arbitrary_path);
    EXPECT_EQ(expected_path, expanded_path)
        << "Failed special folder path expansion. Got: \""
        << base::WideToUTF8(expanded_path.value())
        << "\", but expected: " << base::WideToUTF8(expected_path.value());
  }
}

}  // namespace

}  // namespace chrome_cleaner
