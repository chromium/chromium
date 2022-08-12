// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_app_from_command_line_flag.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

base::CommandLine CreateDefaultCommandLine(base::StringPiece flag_value) {
  base::CommandLine command_line{base::CommandLine::NoProgram::NO_PROGRAM};
  command_line.AppendSwitchASCII("install-isolated-apps-at-startup",
                                 flag_value);
  return command_line;
}

class InstallIsolatedAppFromCommandLineFlag : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(InstallIsolatedAppFromCommandLineFlag, InstallsAppFromCommandLineFlag) {
  EXPECT_THAT(GetAppsToInstallFromCommandLine(
                  CreateDefaultCommandLine("http://example.com")),
              UnorderedElementsAre(Eq("http://example.com")));
}

TEST_F(InstallIsolatedAppFromCommandLineFlag,
       InstallsDifferentAppFromCommandLineFlag) {
  EXPECT_THAT(GetAppsToInstallFromCommandLine(
                  CreateDefaultCommandLine("http://different-example.com")),
              UnorderedElementsAre(Eq("http://different-example.com")));
}

TEST_F(InstallIsolatedAppFromCommandLineFlag,
       InstallsMultipleCommaSeparatedAppsFromCommandLineFlag) {
  EXPECT_THAT(GetAppsToInstallFromCommandLine(CreateDefaultCommandLine(
                  "http://app.com,http://app2.com,http://app3.com")),
              UnorderedElementsAre(Eq("http://app.com"), Eq("http://app2.com"),
                                   Eq("http://app3.com")));
}

TEST_F(InstallIsolatedAppFromCommandLineFlag, RemoveWhitespacesBetweenAppUrls) {
  EXPECT_THAT(
      GetAppsToInstallFromCommandLine(
          CreateDefaultCommandLine("  http://app.com  ,  http://app2.com")),
      UnorderedElementsAre(Eq("http://app.com"), Eq("http://app2.com")));
}

TEST_F(InstallIsolatedAppFromCommandLineFlag, RemoveEmptyUrls) {
  EXPECT_THAT(
      GetAppsToInstallFromCommandLine(CreateDefaultCommandLine(
          ",  ,http://app.com  ,,,, http://app2.com,,")),
      UnorderedElementsAre(Eq("http://app.com"), Eq("http://app2.com")));
}

TEST_F(InstallIsolatedAppFromCommandLineFlag,
       DoNotCallInstallationWhenFlagIsEmpty) {
  EXPECT_THAT(GetAppsToInstallFromCommandLine(CreateDefaultCommandLine("")),
              IsEmpty());
}

TEST_F(InstallIsolatedAppFromCommandLineFlag,
       DoNotCallInstallationWhenFlagIsNotPresent) {
  const base::CommandLine command_line{
      base::CommandLine::NoProgram::NO_PROGRAM};
  EXPECT_THAT(GetAppsToInstallFromCommandLine(command_line), IsEmpty());
}

}  // namespace
}  // namespace web_app
