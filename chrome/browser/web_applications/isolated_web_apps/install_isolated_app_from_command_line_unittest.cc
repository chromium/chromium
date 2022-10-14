// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_app_from_command_line.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
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

MATCHER_P(IsDevModeBundle,
          bundle_path,
          std::string(negation ? "isn't " : "Dev Mode bundle at: \"") +
              bundle_path.AsUTF8Unsafe() + '"') {
  if (!arg.has_value() || !arg.value().has_value()) {
    DescribeOptionalIsolationData(result_listener, arg);
    return false;
  }
  const IsolationData::DevModeBundle* bundle =
      absl::get_if<IsolationData::DevModeBundle>(&arg.value().value().content);
  if (bundle == nullptr || bundle->path != bundle_path) {
    DescribeOptionalIsolationData(result_listener, arg);
    return false;
  }
  return true;
}

// Sets the current working directory to a location that contains a file.
// The working directory is restored when the object is destroyed.
class ScopedWorkingDirectoryWithFile {
 public:
  ScopedWorkingDirectoryWithFile() {
    // Rather than creating a temporary directory and file, just use the
    // current binary, which we know will always exist.
    CHECK(base::GetCurrentDirectory(&original_working_directory_));
    CHECK(base::PathService::Get(base::FILE_EXE, &executable_path_));
    CHECK(base::SetCurrentDirectory(executable_path_.DirName()));
  }

  ~ScopedWorkingDirectoryWithFile() {
    CHECK(base::SetCurrentDirectory(original_working_directory_));
  }

  base::FilePath existing_file_path() { return executable_path_; }

  base::FilePath existing_file_name() { return executable_path_.BaseName(); }

  base::FilePath directory() { return executable_path_.DirName(); }

 private:
  base::FilePath original_working_directory_;
  base::FilePath executable_path_;
};

base::CommandLine CreateCommandLine(
    absl::optional<base::StringPiece> proxy_flag_value,
    absl::optional<base::FilePath> bundle_flag_value) {
  base::CommandLine command_line{base::CommandLine::NoProgram::NO_PROGRAM};
  if (proxy_flag_value.has_value()) {
    command_line.AppendSwitchASCII("install-isolated-web-app-from-url",
                                   proxy_flag_value.value());
  }
  if (bundle_flag_value.has_value()) {
    command_line.AppendSwitchPath("install-isolated-web-app-from-file",
                                  bundle_flag_value.value());
  }
  return command_line;
}

class InstallIsolatedAppFromCommandLineFlagTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagAbsent) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(absl::nullopt, absl::nullopt)),
              HasNoValue());
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagEmpty) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateCommandLine(
                  absl::nullopt, base::FilePath::FromUTF8Unsafe(""))),
              HasNoValue());
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagInvalid) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateCommandLine(
                  absl::nullopt,
                  base::FilePath::FromUTF8Unsafe("does_not_exist.wbn)"))),
              HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagIsDirectory) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(absl::nullopt, cwd.directory())),
              HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(absl::nullopt, cwd.existing_file_name())),
              IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValidAndAbsolute) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(absl::nullopt, cwd.existing_file_path())),
              IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagAbsent) {
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(CreateCommandLine("", absl::nullopt)),
      HasNoValue());
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagEmpty) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("", base::FilePath::FromUTF8Unsafe(""))),
              HasNoValue());
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagEmptyAndBundleFlagInvalid) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateCommandLine(
                  "", base::FilePath::FromUTF8Unsafe("does_not_exist.wbn"))),
              HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagEmptyAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("", cwd.existing_file_name())),
              IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagAbsent) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("invalid", absl::nullopt)),
              HasErrorWithSubstr("Invalid URL"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagEmpty) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateCommandLine(
                  "invalid", base::FilePath::FromUTF8Unsafe(""))),
              HasErrorWithSubstr("Invalid URL"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagInvalid) {
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(CreateCommandLine(
          "invalid", base::FilePath::FromUTF8Unsafe("does_not_exist.wbn"))),
      HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("invalid", cwd.existing_file_name())),
              HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagAbsent) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("http://example.com", absl::nullopt)),
              IsDevModeProxy("http://example.com"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagEmpty) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateCommandLine(
                  "http://example.com", base::FilePath::FromUTF8Unsafe(""))),
              IsDevModeProxy("http://example.com"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagValidAndBundleFlagInvalid) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateCommandLine(
                  "http://example.com",
                  base::FilePath::FromUTF8Unsafe("does_not_exist.wbn"))),
              HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagValidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(CreateCommandLine(
                  "http://example.com", cwd.existing_file_name())),
              HasErrorWithSubstr("cannot both be provided"));
}

}  // namespace
}  // namespace web_app
