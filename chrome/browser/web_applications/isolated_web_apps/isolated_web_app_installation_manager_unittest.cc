// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/task/task_traits.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
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

using base::test::ErrorIs;
using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::VariantWith;

using MaybeIwaLocation =
    base::expected<absl::optional<IsolatedWebAppLocation>, std::string>;

class FakeWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;

  void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppLocation& location,
      const absl::optional<base::Version>& expected_version,
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

}  // namespace

class IsolatedWebAppInstallationManagerTest : public WebAppTest {
 public:
  IsolatedWebAppInstallationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  void SetUp() override {
    WebAppTest::SetUp();
    fake_provider().SetScheduler(
        std::make_unique<FakeWebAppCommandScheduler>(*profile()));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile()->GetTestingPrefService();
  }

  IsolatedWebAppInstallationManager& manager() {
    return fake_provider().isolated_web_app_installation_manager();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(IsolatedWebAppInstallationManagerTest,
       NoInstallationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebApps);

  base::test::RepeatingTestFuture<
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>>
      future;
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  manager().OnReportInstallationResultForTesting(future.GetCallback());
  manager().InstallFromCommandLine(
      CreateCommandLine("http://example.com:12345", absl::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  EXPECT_THAT(future.Take(),
              ErrorIs(HasSubstr("Isolated Web Apps are not enabled")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       NoInstallationWhenDevModeFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  base::test::RepeatingTestFuture<
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>>
      future;
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  manager().OnReportInstallationResultForTesting(future.GetCallback());
  manager().InstallFromCommandLine(
      CreateCommandLine("http://example.com:12345", absl::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       NoInstallationWhenDevModePolicyDisabled) {
  pref_service()->SetManagedPref(
      prefs::kDevToolsAvailability,
      base::Value(base::to_underlying(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed)));

  base::test::RepeatingTestFuture<
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>>
      future;
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  manager().OnReportInstallationResultForTesting(future.GetCallback());
  manager().InstallFromCommandLine(
      CreateCommandLine("http://example.com:12345", absl::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagAbsent) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(Eq(absl::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagEmpty) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(Eq(absl::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt,
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn)")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagIsDirectory) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, cwd.directory()), future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, cwd.existing_file_name()),
      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(Optional(VariantWith<DevModeBundle>(
                  Field(&DevModeBundle::path, cwd.existing_file_path())))));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValidAndAbsolute) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(absl::nullopt, cwd.existing_file_path()),
      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(Optional(VariantWith<DevModeBundle>(
                  Field(&DevModeBundle::path, cwd.existing_file_path())))));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagAbsent) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("", absl::nullopt),

      future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(Eq(absl::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagEmpty) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("", base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(Eq(absl::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagEmptyAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("",
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallsAppWhenProxyFlagEmptyAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("", cwd.existing_file_name()), future.GetCallback());
  EXPECT_THAT(future.Get(),
              ValueIs(Optional(VariantWith<DevModeBundle>(
                  Field(&DevModeBundle::path, cwd.existing_file_path())))));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagAbsent) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid", absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("Invalid URL")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagEmpty) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid", base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("Invalid URL")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid",
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("invalid", cwd.existing_file_name()),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagAbsent) {
  constexpr base::StringPiece kUrl = "http://example.com";
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(kUrl, absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(Optional(VariantWith<DevModeProxy>(_))));
  EXPECT_TRUE(absl::get<DevModeProxy>(**future.Get())
                  .proxy_url.IsSameOriginWith(GURL(kUrl)));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallsAppWhenProxyFlagWithPortValidAndBundleFlagAbsent) {
  constexpr base::StringPiece kUrl = "http://example.com:12345";
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(kUrl, absl::nullopt), future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(Optional(VariantWith<DevModeProxy>(_))));
  EXPECT_TRUE(absl::get<DevModeProxy>(**future.Get())
                  .proxy_url.IsSameOriginWith(GURL(kUrl)));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagHasPathAndBundleFlagInValid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("http://example.com/path", absl::nullopt),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("Non-origin URL provided")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagEmpty) {
  constexpr base::StringPiece kUrl = "http://example.com";
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine(kUrl, base::FilePath::FromUTF8Unsafe("")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ValueIs(Optional(VariantWith<DevModeProxy>(_))));
  EXPECT_TRUE(absl::get<DevModeProxy>(**future.Get())
                  .proxy_url.IsSameOriginWith(GURL(kUrl)));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagValidAndBundleFlagInvalid) {
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("http://example.com",
                        base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       ErrorWhenProxyFlagValidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  base::test::TestFuture<MaybeIwaLocation> future;
  IsolatedWebAppInstallationManager::GetIsolatedWebAppLocationFromCommandLine(
      CreateCommandLine("http://example.com", cwd.existing_file_name()),
      future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("cannot both be provided")));
}

}  // namespace web_app
