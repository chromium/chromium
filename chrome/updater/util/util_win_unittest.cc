// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/util.h"

#include <optional>
#include <string>
#include <vector>

#include "chrome/updater/tag.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UtilTest, CommandLineForLegacyFormat) {
  std::optional<base::CommandLine> cmd_line = CommandLineForLegacyFormat(
      L"program.exe /handoff \"appguid={8a69}&appname=Chrome\" /appargs "
      L"\"&appguid={8a69}"
      L"&installerdata=%7B%22homepage%22%3A%22http%3A%2F%2Fwww.google.com%\" "
      L"/silent /sessionid {123-456}");

  EXPECT_TRUE(cmd_line);
  EXPECT_EQ(cmd_line->GetSwitchValueASCII("handoff"),
            "appguid={8a69}&appname=Chrome");
  EXPECT_TRUE(cmd_line->HasSwitch("silent"));
  EXPECT_EQ(cmd_line->GetSwitchValueASCII("sessionid"), "{123-456}");
  TagParsingResult result = GetTagArgsForCommandLine(*cmd_line);
  EXPECT_EQ(result.error, tagging::ErrorCode::kSuccess);
  EXPECT_EQ(result.tag_args->apps.size(), size_t{1});
  EXPECT_EQ(result.tag_args->apps[0].app_id, "{8a69}");
  EXPECT_EQ(result.tag_args->apps[0].app_name, "Chrome");
  EXPECT_EQ(result.tag_args->apps[0].encoded_installer_data,
            "%7B%22homepage%22%3A%22http%3A%2F%2Fwww.google.com%");
}

TEST(UtilTest, CommandLineForLegacyFormat_Mixed) {
  std::optional<base::CommandLine> cmd_line = CommandLineForLegacyFormat(
      L"program.exe --handoff \"appguid={8a69}&appname=Chrome\""
      L"/silent /sessionid {123-456}");

  EXPECT_FALSE(cmd_line);
}

TEST(UtilTest, CommandLineForLegacyFormat_WithArgs) {
  std::optional<base::CommandLine> cmd_line = CommandLineForLegacyFormat(
      L"program.exe arg1 /SWITCH1 value1 \"arg2 with space\" /Switch2 /s3");

  EXPECT_TRUE(cmd_line);
  EXPECT_EQ(cmd_line->GetArgs(),
            std::vector<std::wstring>({L"arg1", L"arg2 with space"}));
  EXPECT_EQ(cmd_line->GetSwitchValueASCII("switch1"), "value1");
  EXPECT_TRUE(cmd_line->HasSwitch("switch2"));
  EXPECT_TRUE(cmd_line->GetSwitchValueASCII("switch2").empty());
  EXPECT_TRUE(cmd_line->HasSwitch("s3"));
  EXPECT_TRUE(cmd_line->GetSwitchValueASCII("s3").empty());
}

TEST(UtilTest, CommandLineForLegacyFormat_SwitchWithEqualSign) {
  std::optional<base::CommandLine> cmd_line = CommandLineForLegacyFormat(
      L"updater.exe /enable-logging "
      L"/vmodule=*/components/update_client/*=2,*/chrome/updater/*=2 "
      L"/handoff \"appguid={CDABE316-39CD-43BA-8440-6D1E0547AEE6}&lang=en\"");

  EXPECT_TRUE(cmd_line);
  EXPECT_TRUE(cmd_line->HasSwitch("enable-logging"));
  EXPECT_TRUE(cmd_line->GetSwitchValueASCII("enable-logging").empty());
  EXPECT_EQ(cmd_line->GetSwitchValueASCII("vmodule"),
            "*/components/update_client/*=2,*/chrome/updater/*=2");
  EXPECT_EQ(cmd_line->GetSwitchValueASCII("handoff"),
            "appguid={CDABE316-39CD-43BA-8440-6D1E0547AEE6}&lang=en");
}

}  // namespace updater
