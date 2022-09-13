// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_app_from_command_line.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::Optional;

base::CommandLine CreateDefaultCommandLine(base::StringPiece flag_value) {
  base::CommandLine command_line{base::CommandLine::NoProgram::NO_PROGRAM};
  command_line.AppendSwitchASCII("install-isolated-app-at-startup", flag_value);
  return command_line;
}

class InstallIsolatedAppFromCommandLineFlagTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsAppFromCommandLineFlag) {
  EXPECT_THAT(GetAppToInstallFromCommandLine(
                  CreateDefaultCommandLine("http://example.com")),
              Optional(Eq(GURL("http://example.com"))));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsDifferentAppFromCommandLineFlag) {
  EXPECT_THAT(GetAppToInstallFromCommandLine(
                  CreateDefaultCommandLine("http://different-example.com")),
              Optional(Eq(GURL("http://different-example.com"))));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest, NoneForInvalidUrls) {
  EXPECT_THAT(
      GetAppToInstallFromCommandLine(CreateDefaultCommandLine("badurl")),
      Eq(absl::nullopt));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       DoNotCallInstallationWhenFlagIsEmpty) {
  EXPECT_THAT(GetAppToInstallFromCommandLine(CreateDefaultCommandLine("")),
              Eq(absl::nullopt));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       DoNotCallInstallationWhenFlagIsNotPresent) {
  const base::CommandLine command_line{
      base::CommandLine::NoProgram::NO_PROGRAM};
  EXPECT_THAT(GetAppToInstallFromCommandLine(command_line), Eq(absl::nullopt));
}

}  // namespace
}  // namespace web_app
