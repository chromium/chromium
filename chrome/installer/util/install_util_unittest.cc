// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_util.h"

#include <Aclapi.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/google_update_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Not;
using ::testing::Return;

TEST(InstallUtilTest, ComposeCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  std::pair<std::wstring, std::wstring> params[] = {
      std::make_pair(std::wstring(L""), std::wstring(L"")),
      std::make_pair(std::wstring(L""),
                     std::wstring(L"--do-something --silly")),
      std::make_pair(std::wstring(L"spam.exe"), std::wstring(L"")),
      std::make_pair(std::wstring(L"spam.exe"),
                     std::wstring(L"--do-something --silly")),
  };
  for (std::pair<std::wstring, std::wstring>& param : params) {
    InstallUtil::ComposeCommandLine(param.first, param.second, &command_line);
    EXPECT_EQ(param.first, command_line.GetProgram().value());
    if (param.second.empty()) {
      EXPECT_TRUE(command_line.GetSwitches().empty());
    } else {
      EXPECT_EQ(2U, command_line.GetSwitches().size());
      EXPECT_TRUE(command_line.HasSwitch("do-something"));
      EXPECT_TRUE(command_line.HasSwitch("silly"));
    }
  }
}

TEST(InstallUtilTest, GetCurrentDate) {
  std::wstring date(InstallUtil::GetCurrentDate());
  EXPECT_EQ(8u, date.length());
  if (date.length() == 8) {
    // For an invalid date value, SystemTimeToFileTime will fail.
    // We use this to validate that we have a correct date string.
    SYSTEMTIME systime = {0};
    FILETIME ft = {0};
    // Just to make sure our assumption holds.
    EXPECT_FALSE(SystemTimeToFileTime(&systime, &ft));
    // Now fill in the values from our string.
    systime.wYear = _wtoi(date.substr(0, 4).c_str());
    systime.wMonth = _wtoi(date.substr(4, 2).c_str());
    systime.wDay = _wtoi(date.substr(6, 2).c_str());
    // Check if they make sense.
    EXPECT_TRUE(SystemTimeToFileTime(&systime, &ft));
  }
}

TEST(InstallUtilTest, GetToastActivatorRegistryPath) {
  std::wstring toast_activator_reg_path =
      InstallUtil::GetToastActivatorRegistryPath();
  EXPECT_FALSE(toast_activator_reg_path.empty());

  // Confirm that the string is a path followed by a GUID.
  size_t guid_begin = toast_activator_reg_path.find('{');
  EXPECT_NE(std::wstring::npos, guid_begin);
  ASSERT_GE(guid_begin, 1u);
  EXPECT_EQ(L'\\', toast_activator_reg_path[guid_begin - 1]);

  // A GUID has the form "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}".
  constexpr size_t kGuidLength = 38;
  EXPECT_EQ(kGuidLength, toast_activator_reg_path.length() - guid_begin);

  EXPECT_EQ('}', toast_activator_reg_path.back());
}

TEST(InstallUtilTest, GuidToSquid) {
  ASSERT_EQ(InstallUtil::GuidToSquid(L"EDA620E3-AA98-3846-B81E-3493CB2E0E02"),
            L"3E026ADE89AA64838BE14339BCE2E020");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Tests that policy-overrides for channel values are included in generated
// command lines.
TEST(AppendModeAndChannelSwitchesTest, ExtendedStable) {
  static constexpr struct {
    const wchar_t* channel_override;
    bool is_extended_stable_channel;
  } kTestData[] = {
      {
          /*channel_override=*/nullptr,
          /*is_extended_stable_channel=*/false,
      },
      {
          /*channel_override=*/L"",
          /*is_extended_stable_channel=*/false,
      },
      {
          /*channel_override=*/L"stable",
          /*is_extended_stable_channel=*/false,
      },
      {
          /*channel_override=*/L"beta",
          /*is_extended_stable_channel=*/false,
      },
      {
          /*channel_override=*/L"dev",
          /*is_extended_stable_channel=*/false,
      },
      {
          /*channel_override=*/L"extended",
          /*is_extended_stable_channel=*/true,
      },
  };

  for (const auto& test_data : kTestData) {
    // Install process-wide InstallDetails for the given test data.
    auto install_details =
        std::make_unique<install_static::PrimaryInstallDetails>();
    install_details->set_mode(
        &install_static::kInstallModes[install_static::STABLE_INDEX]);
    install_details->set_channel(L"");
    if (test_data.channel_override) {
      install_details->set_channel_origin(
          install_static::ChannelOrigin::kPolicy);
      install_details->set_channel_override(test_data.channel_override);
    }
    install_details->set_is_extended_stable_channel(
        test_data.is_extended_stable_channel);
    install_static::ScopedInstallDetails scoped_details(
        std::move(install_details));

    // Generate a command line.
    base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
    InstallUtil::AppendModeAndChannelSwitches(&cmd_line);

    // Ensure that it has the proper --channel switch.
    if (test_data.channel_override) {
      ASSERT_TRUE(cmd_line.HasSwitch(installer::switches::kChannel));
      ASSERT_EQ(cmd_line.GetSwitchValueNative(installer::switches::kChannel),
                test_data.channel_override);
    }
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
