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

void DescribeOptionalIsolationData(
    ::testing::MatchResultListener* result_listener,
    base::expected<absl::optional<IsolationData>, std::string> arg) {
  if (arg.has_value()) {
    if (arg.value().has_value())
      *result_listener << arg.value()->AsDebugValue();
    else
      *result_listener << "nullopt";
  } else {
    *result_listener << "an error with message: \"" << arg.error() << '"';
  }
}

MATCHER_P(HasErrorWithSubstr,
          substr,
          std::string(negation ? "not " : "") +
              " an error with a message containing: \"" + substr + '"') {
  if (arg.has_value() || arg.error().find(substr) == std::string::npos) {
    DescribeOptionalIsolationData(result_listener, arg);
    return false;
  }
  return true;
}

MATCHER(HasNoValue, negation ? "not absent" : "absent") {
  if (!arg.has_value() || arg.value().has_value()) {
    DescribeOptionalIsolationData(result_listener, arg);
    return false;
  }
  return true;
}

MATCHER_P(IsDevModeProxy,
          proxy_url,
          std::string(negation ? "isn't " : "Dev Mode proxy with URL: \"") +
              proxy_url + '"') {
  if (!arg.has_value() || !arg.value().has_value()) {
    DescribeOptionalIsolationData(result_listener, arg);
    return false;
  }
  const IsolationData::DevModeProxy* proxy =
      absl::get_if<IsolationData::DevModeProxy>(&arg.value().value().content);
  if (proxy == nullptr || GURL(proxy->proxy_url) != GURL(proxy_url)) {
    DescribeOptionalIsolationData(result_listener, arg);
    return false;
  }
  return true;
}

base::CommandLine CreateDefaultCommandLine(base::StringPiece flag_value) {
  base::CommandLine command_line{base::CommandLine::NoProgram::NO_PROGRAM};
  command_line.AppendSwitchASCII("install-isolated-web-app-from-url",
                                 flag_value);
  return command_line;
}

class InstallIsolatedAppFromCommandLineFlagTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsAppFromCommandLineFlag) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateDefaultCommandLine("http://example.com")),
              IsDevModeProxy("http://example.com"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsDifferentAppFromCommandLineFlag) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateDefaultCommandLine("http://different-example.com")),
              IsDevModeProxy("http://different-example.com"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest, NoneForInvalidUrls) {
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(CreateDefaultCommandLine("badurl")),
      HasErrorWithSubstr("Invalid URL"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       DoNotCallInstallationWhenFlagIsEmpty) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateDefaultCommandLine("")),
              HasNoValue());
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       DoNotCallInstallationWhenFlagIsNotPresent) {
  const base::CommandLine command_line{
      base::CommandLine::NoProgram::NO_PROGRAM};
  EXPECT_THAT(GetIsolationDataFromCommandLine(command_line), HasNoValue());
}

}  // namespace
}  // namespace web_app
