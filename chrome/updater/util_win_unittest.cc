// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UtilTest, GetSwitchValueInLegacyFormat) {
  std::string GetSwitchValueInLegacyFormat(const std::wstring& command_line,
                                           const std::wstring& switch_name);
  const std::wstring command_line =
      L"program.exe /handoff "
      L"\"&appid={8a69}&app_name=Google Chrome&needsadmin=true&lang=en\" "
      L"/interactive /sessionid {123-456}";
  EXPECT_EQ(GetSwitchValueInLegacyFormat(command_line, L"handoff"),
            "&appid={8a69}&app_name=Google Chrome&needsadmin=true&lang=en");
  EXPECT_TRUE(
      GetSwitchValueInLegacyFormat(command_line, L"interactive").empty());
  EXPECT_EQ(GetSwitchValueInLegacyFormat(command_line, L"sessionid"),
            "{123-456}");
  EXPECT_TRUE(
      GetSwitchValueInLegacyFormat(command_line, L"none_exist_switch").empty());
}

}  // namespace updater
