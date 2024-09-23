// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/installer.h"

#include <shlobj.h>

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char install_switch[] = "--install=";
constexpr char enable_logging_switch[] = "--enable-logging";
constexpr char logging_module_switch[] = "--vmodule=";

void ExpectExactlyOneOccurrence(const std::wstring& search_string,
                                const std::wstring& substring) {
  auto pos = search_string.find(substring);
  EXPECT_NE(pos, std::wstring::npos);
  pos = search_string.find(substring, pos + substring.size());
  EXPECT_EQ(pos, std::wstring::npos);
}

void ExpectSwitchValue(const std::wstring& cmd_line,
                       const std::wstring& switch_str,
                       const std::wstring& expected_value) {
  auto pos = cmd_line.find(switch_str);
  EXPECT_NE(pos, std::wstring::npos);
  pos += switch_str.size();
  EXPECT_LE(pos + expected_value.size(), cmd_line.size());
  EXPECT_EQ(cmd_line.substr(pos, expected_value.size()), expected_value);
}
}  // namespace

// Tests that `HandleRunElevated` returns `UNEXPECTED_ELEVATION_LOOP` when
// not elevated and called with `kCmdLineExpectElevated` argument.
TEST(InstallerTest, HandleRunElevated) {
  if (::IsUserAnAdmin()) {
    return;
  }

  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("UpdaterSetup.exe")));
  command_line.AppendSwitch(updater::kInstallSwitch);
  command_line.AppendSwitch(updater::kSystemSwitch);
  command_line.AppendSwitch(updater::kCmdLineExpectElevated);

  updater::ProcessExitResult exit_result =
      updater::HandleRunElevated(command_line);
  EXPECT_EQ(exit_result.exit_code, updater::UNEXPECTED_ELEVATION_LOOP);
  EXPECT_EQ(exit_result.windows_error, 0U);
}

TEST(InstallerTest, FindOfflineDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath unpack_path = temp_dir.GetPath();
  base::FilePath metainstall_dir = unpack_path.Append(L"bin");
  ASSERT_TRUE(base::CreateDirectory(metainstall_dir));

  EXPECT_FALSE(updater::FindOfflineDir(unpack_path).has_value());

  base::FilePath offline_install_dir =
      metainstall_dir.Append(L"Offline")
          .Append(L"{8D5D0563-F2A0-40E3-932D-AFEAE261A9D1}");
  ASSERT_TRUE(base::CreateDirectory(offline_install_dir));

  std::optional<base::FilePath> offline_dir =
      updater::FindOfflineDir(unpack_path);
  EXPECT_TRUE(offline_dir.has_value());
  EXPECT_EQ(offline_dir->BaseName(),
            base::FilePath(L"{8D5D0563-F2A0-40E3-932D-AFEAE261A9D1}"));
}

TEST(BuildInstallerCommandLineArgumentsTest, EnableLoggingSwitch) {
  // Test that --enable-logging switch is added if none is provided.
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, "fake_tag"}));
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(enable_logging_switch));

  // Test that no --enable-logging switch is added if one is provided.
  cmd_line_args.clear();
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str =
      base::SysUTF8ToWide(base::StrCat({"UpdaterSetup.exe ", install_switch,
                                        "fake_tag ", enable_logging_switch}));
  exit_result = updater::BuildInstallerCommandLineArguments(
      command_line_str.c_str(), cmd_line_args.get(), cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(enable_logging_switch));
}

TEST(BuildInstallerCommandLineArgumentsTest, LoggingModuleSwitch) {
  // Test that --vmodule switch is added if none is provided.
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, "fake_tag"}));
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(logging_module_switch));

  // Test that no --vmodule switch is added if one is provided.
  cmd_line_args.clear();
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, "fake_tag ",
                    logging_module_switch, "fake_module"}));
  exit_result = updater::BuildInstallerCommandLineArguments(
      command_line_str.c_str(), cmd_line_args.get(), cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(logging_module_switch));
  ExpectSwitchValue(command_line_str,
                    base::SysUTF8ToWide(logging_module_switch),
                    std::wstring(L"fake_module"));
}

TEST(BuildInstallerCommandLineArgumentsTest, CommandStringOverflow) {
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  std::string long_tag(updater::kInstallerMaxCommandString + 1, 'A');
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, long_tag}));
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::COMMAND_STRING_OVERFLOW);
}

TEST(BuildInstallerCommandLineArgumentsTest, NoArguments) {
  // Passing in no arguments on the command line will attempt to
  // extract the embedded tag, but since the test executable is not
  // tagged this should not add any --tag switches.
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::INVALID_OPTION);
}

TEST(BuildInstallerCommandLineArgumentsTest, LegacyCommandLine) {
  std::optional<base::CommandLine> cmd_line =
      updater::CommandLineForLegacyFormat(
          L"UpdaterSetup.exe /install "
          L"\"appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%"
          L"20Chrome&needsadmin=Prefers&lang=en\"");
  ASSERT_TRUE(cmd_line.has_value());
  updater::CommandString cmd_line_args;
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(
          cmd_line->GetCommandLineString().c_str(), cmd_line_args.get(),
          cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  const base::CommandLine command_line = base::CommandLine::FromString(
      base::StrCat({L"exe.exe ", cmd_line_args.get()}));
  EXPECT_EQ(command_line.GetSwitchValueASCII(updater::kInstallSwitch),
            "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%"
            "20Chrome&needsadmin=Prefers&lang=en");
}
