// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::HasSubstr;

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
  if (proxy == nullptr || !proxy->proxy_url.IsSameOriginWith(GURL(proxy_url))) {
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

class InstallIsolatedWebAppFromCommandLineFlagTest : public ::testing::Test {
 public:
  InstallIsolatedWebAppFromCommandLineFlagTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    pref_service_.registry()->RegisterBooleanPref(
        policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed, true);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebApps);

  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("http://example.com:12345", absl::nullopt),
                  pref_service()),
              HasErrorWithSubstr("Isolated Web Apps are not enabled"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenDevModeFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine("http://example.com:12345", absl::nullopt),
          pref_service()),
      HasErrorWithSubstr("Isolated Web App Developer Mode is not enabled"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenDevModePolicyDisabled) {
  pref_service()->SetManagedPref(
      policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed,
      base::Value(false));

  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine("http://example.com:12345", absl::nullopt),
          pref_service()),
      HasErrorWithSubstr("Isolated Web App Developer Mode is not enabled"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagAbsent) {
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine(absl::nullopt, absl::nullopt), pref_service()),
      HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagEmpty) {
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine(absl::nullopt, base::FilePath::FromUTF8Unsafe("")),
          pref_service()),
      HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagInvalid) {
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine(absl::nullopt, base::FilePath::FromUTF8Unsafe(
                                               "does_not_exist.wbn)")),
          pref_service()),
      HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagIsDirectory) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine(absl::nullopt, cwd.directory()), pref_service()),
      HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(absl::nullopt, cwd.existing_file_name()),
                  pref_service()),
              IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValidAndAbsolute) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(absl::nullopt, cwd.existing_file_path()),
                  pref_service()),
              IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagAbsent) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("", absl::nullopt), pref_service()),
              HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagEmpty) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("", base::FilePath::FromUTF8Unsafe("")),
                  pref_service()),
              HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagEmptyAndBundleFlagInvalid) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(
                      "", base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
                  pref_service()),
              HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagEmptyAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine("", cwd.existing_file_name()), pref_service()),
      IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagAbsent) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("invalid", absl::nullopt), pref_service()),
              HasErrorWithSubstr("Invalid URL"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagEmpty) {
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine("invalid", base::FilePath::FromUTF8Unsafe("")),
          pref_service()),
      HasErrorWithSubstr("Invalid URL"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagInvalid) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("invalid", base::FilePath::FromUTF8Unsafe(
                                                   "does_not_exist.wbn")),
                  pref_service()),
              HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("invalid", cwd.existing_file_name()),
                  pref_service()),
              HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagAbsent) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("http://example.com", absl::nullopt),
                  pref_service()),
              IsDevModeProxy("http://example.com"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagWithPortValidAndBundleFlagAbsent) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("http://example.com:12345", absl::nullopt),
                  pref_service()),
              IsDevModeProxy("http://example.com:12345"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagHasPathAndBundleFlagInValid) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("http://example.com/path", absl::nullopt),
                  pref_service()),
              HasErrorWithSubstr("Non-origin URL provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagEmpty) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine("http://example.com",
                                    base::FilePath::FromUTF8Unsafe("")),
                  pref_service()),
              IsDevModeProxy("http://example.com"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagValidAndBundleFlagInvalid) {
  EXPECT_THAT(GetIsolationDataFromCommandLine(
                  CreateCommandLine(
                      "http://example.com",
                      base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
                  pref_service()),
              HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagValidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(
      GetIsolationDataFromCommandLine(
          CreateCommandLine("http://example.com", cwd.existing_file_name()),
          pref_service()),
      HasErrorWithSubstr("cannot both be provided"));
}

class InstallIsolatedWebAppFromCommandLineIsolationInfoTest
    : public ::testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(InstallIsolatedWebAppFromCommandLineIsolationInfoTest,
       GetIsolationInfoWhenInstalledBundleSucceeds) {
  IsolationData isolation_data =
      IsolationData{IsolationData::InstalledBundle{}};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  GetIsolationInfo(isolation_data, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();

  ASSERT_THAT(result.has_value(), false);
  EXPECT_THAT(result.error(), HasSubstr("is not implemented"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineIsolationInfoTest,
       GetIsolationInfoWhenDevModeBundleSucceeds) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(base::FilePath::FromASCII("test-0.swbn"));
  TestSignedWebBundle bundle = BuildDefaultTestSignedWebBundle();
  ASSERT_TRUE(base::WriteFile(path, bundle.data));

  IsolationData isolation_data =
      IsolationData{IsolationData::DevModeBundle{.path = path}};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;
  GetIsolationInfo(isolation_data, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();

  ASSERT_THAT(result.has_value(), true);
  EXPECT_EQ(result.value().web_bundle_id(), bundle.id);
}

TEST_F(InstallIsolatedWebAppFromCommandLineIsolationInfoTest,
       GetIsolationInfoWhenDevModeBundleFailsWhenFileNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().Append(
      base::FilePath::FromASCII("file_not_exist.swbn"));
  IsolationData isolation_data =
      IsolationData{IsolationData::DevModeBundle{.path = path}};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  GetIsolationInfo(isolation_data, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();

  ASSERT_THAT(result.has_value(), false);
  EXPECT_THAT(result.error(),
              HasSubstr("Failed to read the integrity block of the signed web "
                        "bundle: FILE_ERROR_NOT_FOUND"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineIsolationInfoTest,
       GetIsolationInfoWhenDevModeBundleFailsWhenInvalidFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(base::FilePath::FromASCII("invalid_file.swbn"));
  ASSERT_TRUE(
      base::WriteFile(path, "clearly, this is not a valid signed web bundle"));
  IsolationData isolation_data =
      IsolationData{IsolationData::DevModeBundle{.path = path}};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  GetIsolationInfo(isolation_data, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();

  ASSERT_THAT(result.has_value(), false);
  EXPECT_THAT(result.error(),
              HasSubstr("Failed to read the integrity block of the signed web "
                        "bundle: Wrong array size or magic bytes."));
}

TEST_F(InstallIsolatedWebAppFromCommandLineIsolationInfoTest,
       GetIsolationInfoSucceedsWhenDevModeProxy) {
  IsolationData isolation_data = IsolationData{IsolationData::DevModeProxy{}};
  base::test::TestFuture<base::expected<IsolatedWebAppUrlInfo, std::string>>
      test_future;

  GetIsolationInfo(isolation_data, test_future.GetCallback());
  base::expected<IsolatedWebAppUrlInfo, std::string> result = test_future.Get();

  EXPECT_THAT(result.has_value(), true);
}

}  // namespace
}  // namespace web_app
