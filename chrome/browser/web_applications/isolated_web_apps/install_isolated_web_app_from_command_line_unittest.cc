// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/task/task_traits.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using MaybeIwaLocation =
    base::expected<absl::optional<IsolatedWebAppLocation>, std::string>;

void DescribeOptionalLocation(::testing::MatchResultListener* result_listener,
                              MaybeIwaLocation arg) {
  if (arg.has_value()) {
    if (arg.value().has_value()) {
      *result_listener
          << WebApp::IsolationData(arg.value().value()).AsDebugValue();
    } else {
      *result_listener << "nullopt";
    }
  } else {
    *result_listener << "an error with message: \"" << arg.error() << '"';
  }
}

MATCHER_P(HasErrorWithSubstr,
          substr,
          std::string(negation ? "not " : "") +
              " an error with a message containing: \"" + substr + '"') {
  if (arg.has_value() || arg.error().find(substr) == std::string::npos) {
    DescribeOptionalLocation(result_listener, arg);
    return false;
  }
  return true;
}

MATCHER(HasNoValue, negation ? "not absent" : "absent") {
  if (!arg.has_value() || arg.value().has_value()) {
    DescribeOptionalLocation(result_listener, arg);
    return false;
  }
  return true;
}

MATCHER_P(IsDevModeProxy,
          proxy_url,
          base::StrCat({negation ? "isn't " : "Dev Mode proxy with URL: \"",
                        proxy_url, "\""})) {
  if (!arg.has_value() || !arg.value().has_value()) {
    DescribeOptionalLocation(result_listener, arg);
    return false;
  }
  const DevModeProxy* proxy = absl::get_if<DevModeProxy>(&arg.value().value());
  if (proxy == nullptr || !proxy->proxy_url.IsSameOriginWith(GURL(proxy_url))) {
    DescribeOptionalLocation(result_listener, arg);
    return false;
  }
  return true;
}

MATCHER_P(IsDevModeBundle,
          bundle_path,
          std::string(negation ? "isn't " : "Dev Mode bundle at: \"") +
              bundle_path.AsUTF8Unsafe() + '"') {
  if (!arg.has_value() || !arg.value().has_value()) {
    DescribeOptionalLocation(result_listener, arg);
    return false;
  }
  const DevModeBundle* bundle =
      absl::get_if<DevModeBundle>(&arg.value().value());
  if (bundle == nullptr || bundle->path != bundle_path) {
    DescribeOptionalLocation(result_listener, arg);
    return false;
  }
  return true;
}

class FakeWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;

  void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppLocation& location,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      WebAppCommandScheduler::InstallIsolatedWebAppCallback callback,
      const base::Location& call_location) override {}
};

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

class InstallIsolatedWebAppFromCommandLineFlagTest : public WebAppTest {
 public:
  InstallIsolatedWebAppFromCommandLineFlagTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile()->GetTestingPrefService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebApps);

  base::test::RepeatingTestFuture<
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>>
      future;
  auto manager = IsolatedWebAppCommandLineInstallManager(*profile());
  FakeWebAppCommandScheduler fake_command_scheduler(*profile(), nullptr);
  manager.OnReportInstallationResultForTesting(future.GetCallback());
  manager.SetSubsystems(&fake_command_scheduler);
  manager.Start();

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  manager.InstallFromCommandLine(
      CreateCommandLine("http://example.com:12345", absl::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  auto error = future.Take();
  ASSERT_THAT(!error.has_value(), testing::IsTrue());
  EXPECT_THAT(error.error(),
              testing::HasSubstr("Isolated Web Apps are not enabled"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenDevModeFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  base::test::RepeatingTestFuture<
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>>
      future;
  auto manager = IsolatedWebAppCommandLineInstallManager(*profile());
  FakeWebAppCommandScheduler fake_command_scheduler(*profile(), nullptr);
  manager.OnReportInstallationResultForTesting(future.GetCallback());
  manager.SetSubsystems(&fake_command_scheduler);
  manager.Start();

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  manager.InstallFromCommandLine(
      CreateCommandLine("http://example.com:12345", absl::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  auto error = future.Take();
  ASSERT_THAT(!error.has_value(), testing::IsTrue());
  EXPECT_THAT(
      error.error(),
      testing::HasSubstr("Isolated Web App Developer Mode is not enabled"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenDevModePolicyDisabled) {
  pref_service()->SetManagedPref(
      policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed,
      base::Value(false));

  base::test::RepeatingTestFuture<
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>>
      future;
  auto manager = IsolatedWebAppCommandLineInstallManager(*profile());
  FakeWebAppCommandScheduler fake_command_scheduler(*profile(), nullptr);
  manager.OnReportInstallationResultForTesting(future.GetCallback());
  manager.SetSubsystems(&fake_command_scheduler);
  manager.Start();

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  manager.InstallFromCommandLine(
      CreateCommandLine("http://example.com:12345", absl::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  auto error = future.Take();
  ASSERT_THAT(!error.has_value(), testing::IsTrue());
  EXPECT_THAT(
      error.error(),
      testing::HasSubstr("Isolated Web App Developer Mode is not enabled"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagAbsent) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagEmpty) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt,
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn)")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagIsDirectory) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, cwd.directory()), future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, cwd.existing_file_name()),
      future.GetCallback());
  EXPECT_THAT(future.Get(), IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValidAndAbsolute) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, cwd.existing_file_path()),
      future.GetCallback());
  EXPECT_THAT(future.Get(), IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagAbsent) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(CreateCommandLine("", absl::nullopt),

                                           future.GetCallback());
  EXPECT_THAT(future.Get(), HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagEmpty) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("", base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasNoValue());
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagEmptyAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("",
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("Invalid path provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagEmptyAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("", cwd.existing_file_name()), future.GetCallback());
  EXPECT_THAT(future.Get(), IsDevModeBundle(cwd.existing_file_path()));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagAbsent) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid", absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("Invalid URL"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagEmpty) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid", base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("Invalid URL"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid",
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid", cwd.existing_file_name()),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagAbsent) {
  constexpr base::StringPiece kUrl = "http://example.com";
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(kUrl, absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), IsDevModeProxy(kUrl));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagWithPortValidAndBundleFlagAbsent) {
  constexpr base::StringPiece kUrl = "http://example.com:12345";
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(kUrl, absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), IsDevModeProxy(kUrl));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagHasPathAndBundleFlagInValid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("http://example.com/path", absl::nullopt),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("Non-origin URL provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagEmpty) {
  constexpr base::StringPiece kUrl = "http://example.com";
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(kUrl, base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), IsDevModeProxy(kUrl));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagValidAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("http://example.com",
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("cannot both be provided"));
}

TEST_F(InstallIsolatedWebAppFromCommandLineFlagTest,
       ErrorWhenProxyFlagValidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("http://example.com", cwd.existing_file_name()),
      future.GetCallback());
  EXPECT_THAT(future.Get(), HasErrorWithSubstr("cannot both be provided"));
}

}  // namespace
}  // namespace web_app
