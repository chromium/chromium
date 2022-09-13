// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include <string>

#include "chrome/updater/tag.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UtilTest, GetSwitchValueInLegacyFormat) {
  const std::wstring command_line =
      L"program.exe /handoff "
      L"\"&appid={8a69}&appname=Google Chrome&needsadmin=true&lang=en\" "
      L"/interactive /sessionid {123-456}";
  EXPECT_EQ(GetSwitchValueInLegacyFormat(command_line, L"handoff"),
            "&appid={8a69}&appname=Google Chrome&needsadmin=true&lang=en");
  EXPECT_TRUE(
      GetSwitchValueInLegacyFormat(command_line, L"interactive").empty());
  EXPECT_EQ(GetSwitchValueInLegacyFormat(command_line, L"sessionid"),
            "{123-456}");
  EXPECT_TRUE(
      GetSwitchValueInLegacyFormat(command_line, L"none_exist_switch").empty());
}

TEST(UtilTest, GetTagArgsFromLegacyCommandLine) {
  TagParsingResult result = GetTagArgsFromLegacyCommandLine(
      L"program.exe /handoff \"appguid={8a69}&appname=Chrome\" /appargs "
      L"\"&appguid={8a69}"
      L"&installerdata=%7B%22homepage%22%3A%22http%3A%2F%2Fwww.google.com%\" "
      L"/silent /sessionid {123-456}");
  EXPECT_EQ(result.error, tagging::ErrorCode::kSuccess);
  EXPECT_EQ(result.tag_args->apps.size(), size_t{1});
  EXPECT_EQ(result.tag_args->apps[0].app_id, "{8a69}");
  EXPECT_EQ(result.tag_args->apps[0].app_name, "Chrome");
  EXPECT_EQ(result.tag_args->apps[0].encoded_installer_data,
            "%7B%22homepage%22%3A%22http%3A%2F%2Fwww.google.com%");
}

}  // namespace updater
