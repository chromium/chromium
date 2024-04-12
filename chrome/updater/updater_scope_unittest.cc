// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_scope.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class GetUpdaterScopeForCommandLineTest : public testing::Test {
 protected:
  base::CommandLine command_line_ =
      base::CommandLine(GetExecutableRelativePath());
};

TEST_F(GetUpdaterScopeForCommandLineTest, NoParams) {
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_), UpdaterScope::kUser);
  EXPECT_FALSE(IsPrefersForCommandLine(command_line_));
}

TEST_F(GetUpdaterScopeForCommandLineTest, System) {
  command_line_.AppendSwitch(kSystemSwitch);
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_),
            UpdaterScope::kSystem);
  EXPECT_FALSE(IsPrefersForCommandLine(command_line_));
}

#if BUILDFLAG(IS_WIN)

TEST_F(GetUpdaterScopeForCommandLineTest, Prefers) {
  command_line_.AppendSwitch(kCmdLinePrefersUser);
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_), UpdaterScope::kUser);
  EXPECT_FALSE(IsPrefersForCommandLine(command_line_));
}

TEST_F(GetUpdaterScopeForCommandLineTest, System_And_Prefers) {
  command_line_.AppendSwitch(kSystemSwitch);
  command_line_.AppendSwitch(kCmdLinePrefersUser);
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_),
            UpdaterScope::kSystem);
  EXPECT_FALSE(IsPrefersForCommandLine(command_line_));
}

struct TagSwitchTestCase {
  const std::string tag_switch;
};

class TagSwitchTest : public ::testing::WithParamInterface<TagSwitchTestCase>,
                      public GetUpdaterScopeForCommandLineTest {};

INSTANTIATE_TEST_SUITE_P(TagSwitchTestCases,
                         TagSwitchTest,
                         ::testing::ValuesIn(std::vector<TagSwitchTestCase>{
                             {kInstallSwitch},
                             {kHandoffSwitch},
                         }));

TEST_P(TagSwitchTest, TagPrefers) {
  command_line_.AppendSwitchASCII(
      GetParam().tag_switch,
      "appguid=5F46DE36-737D-4271-91C1-C062F9FE21D9&"
      "appname=TestApp3&"
      "needsadmin=prefers&");
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_),
            UpdaterScope::kSystem);
  EXPECT_TRUE(IsPrefersForCommandLine(command_line_));
}

TEST_P(TagSwitchTest, Prefers_And_TagPrefers) {
  command_line_.AppendSwitch(kCmdLinePrefersUser);
  command_line_.AppendSwitchASCII(
      GetParam().tag_switch,
      "appguid=5F46DE36-737D-4271-91C1-C062F9FE21D9&"
      "appname=TestApp3&"
      "needsadmin=prefers&");
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_), UpdaterScope::kUser);
  EXPECT_TRUE(IsPrefersForCommandLine(command_line_));
}

TEST_P(TagSwitchTest, TagRuntime) {
  command_line_.AppendSwitchASCII(GetParam().tag_switch, "runtime=true");
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_), UpdaterScope::kUser);
}

TEST_P(TagSwitchTest, TagRuntimeUser) {
  command_line_.AppendSwitchASCII(GetParam().tag_switch,
                                  "runtime=true&needsadmin=false&");
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_), UpdaterScope::kUser);
}

TEST_P(TagSwitchTest, TagRuntimeSystem) {
  command_line_.AppendSwitchASCII(GetParam().tag_switch,
                                  "runtime=true&needsadmin=true&");
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_),
            UpdaterScope::kSystem);
}
#endif

}  // namespace updater
