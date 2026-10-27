// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevator.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace elevation_service {

class ElevatorTest : public ::testing::Test {};

TEST_F(ElevatorTest, StringHandlingTest) {
  {
    std::string str;
    const std::string kFirstString("hello again");
    Elevator::AppendStringWithLength(kFirstString, str);
    EXPECT_EQ(sizeof(uint32_t) + kFirstString.length(), str.length());

    const std::string kSecondString("another string");
    Elevator::AppendStringWithLength(kSecondString, str);
    EXPECT_EQ(sizeof(uint32_t) + kFirstString.length() + sizeof(uint32_t) +
                  kSecondString.length(),
              str.length());

    const std::string first_str = Elevator::PopFromStringFront(str);
    EXPECT_EQ(kFirstString, first_str);
    EXPECT_EQ(sizeof(uint32_t) + kSecondString.length(), str.length());

    const std::string second_str = Elevator::PopFromStringFront(str);
    EXPECT_EQ(kSecondString, second_str);

    const std::string error_str = Elevator::PopFromStringFront(str);
    EXPECT_TRUE(error_str.empty());

    std::string str2;

    Elevator::AppendStringWithLength("", str2);
    EXPECT_EQ(sizeof(uint32_t), str2.length());

    std::string empty_str = Elevator::PopFromStringFront(str2);
    EXPECT_TRUE(empty_str.empty());
  }
  {
    // Test data with embedded nulls to make sure it's handled correctly.
    const std::vector<uint8_t> test_data1{6, 5, 4, 3, 2, 1, 0, 1, 2,
                                          3, 4, 5, 6, 0, 1, 2, 3, 4};
    const std::vector<uint8_t> test_data2{0, 1, 4, 0, 1, 4, 5, 0};
    std::string str;
    Elevator::AppendStringWithLength(
        std::string(test_data1.cbegin(), test_data1.cend()), str);
    Elevator::AppendStringWithLength(
        std::string(test_data2.cbegin(), test_data2.cend()), str);
    EXPECT_EQ(sizeof(uint32_t) + test_data1.size() + sizeof(uint32_t) +
                  test_data2.size(),
              str.length());
    const std::string first_str = Elevator::PopFromStringFront(str);
    EXPECT_TRUE(
        std::equal(test_data1.cbegin(), test_data1.cend(), first_str.cbegin()));
    const std::string second_str = Elevator::PopFromStringFront(str);
    EXPECT_TRUE(std::equal(test_data2.cbegin(), test_data2.cend(),
                           second_str.cbegin()));
  }
}

TEST_F(ElevatorTest, GenerateIsolatedCommandLine) {
  const base::FilePath kTrustedProgram(FILE_PATH_LITERAL(
      "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe"));

  // Helper that parses `untrusted_str`, generates the isolated command line
  // string using `Elevator::GenerateIsolatedCommandLine`, and then re-parses
  // that output string with `base::CommandLine::FromString` just as the
  // isolated child browser process does on startup.
  auto generate_and_parse = [&](std::wstring_view untrusted_str) {
    const base::CommandLine untrusted =
        base::CommandLine::FromString(untrusted_str);
    const std::wstring isolated_str =
        Elevator::GenerateIsolatedCommandLine(kTrustedProgram, untrusted);
    return base::CommandLine::FromString(isolated_str);
  };

  {
    // Verify that allowed switches (including `--no-startup-window` and
    // `--source-shortcut-location`) are copied, disallowed switches are
    // stripped, `--isolated` is added, and the program path is replaced with
    // the trusted executable path.
    const base::CommandLine result = generate_and_parse(
        L"\"C:\\untrusted\\path.exe\" --disable-component-update "
        L"--profile-directory=\"Profile 1\" --no-startup-window "
        L"--source-shortcut-location=taskbar");

    EXPECT_EQ(kTrustedProgram, result.GetProgram());
    EXPECT_TRUE(result.HasSwitch(::switches::kIsolated));
    EXPECT_TRUE(result.HasSwitch(::switches::kNoStartupWindow));
    EXPECT_EQ("Profile 1",
              result.GetSwitchValueASCII(::switches::kProfileDirectory));
    EXPECT_EQ("taskbar",
              result.GetSwitchValueASCII(::switches::kSourceShortcutLocation));
    EXPECT_FALSE(result.HasSwitch(::switches::kDisableComponentUpdate));
    EXPECT_TRUE(result.GetArgs().empty());
  }

  {
    // Verify positional arguments, including switch-like arguments after `--`,
    // are preserved as positional arguments and not interpreted as switches in
    // the isolated process.
    const base::CommandLine result = generate_and_parse(
        L"untrusted.exe --disable-component-update --restart arg1 \"arg 2\" "
        L"-- /disable-component-update --disable-component-update");

    EXPECT_EQ(kTrustedProgram, result.GetProgram());
    EXPECT_TRUE(result.HasSwitch(::switches::kIsolated));
    EXPECT_TRUE(result.HasSwitch(::switches::kRestart));
    EXPECT_FALSE(result.HasSwitch(::switches::kDisableComponentUpdate));
    const base::CommandLine::StringVector expected_args = {
        L"arg1", L"arg 2", L"/disable-component-update",
        L"--disable-component-update"};
    EXPECT_EQ(expected_args, result.GetArgs());
  }
}

}  // namespace elevation_service
