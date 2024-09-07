// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
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
#include "url/gurl.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

MATCHER_P(IsSameOriginWith, url, "") {
  *result_listener << "where it is not same origin with " << url.spec();
  return arg.IsSameOriginWith(url);
}

using MaybeInstallIsolatedWebAppCommandSuccess =
    IsolatedWebAppInstallationManager::MaybeInstallIsolatedWebAppCommandSuccess;
using MaybeIwaInstallSource =
    IsolatedWebAppInstallationManager::MaybeIwaInstallSource;

class FakeWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;

  void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppInstallSource& install_source,
      const std::optional<base::Version>& expected_version,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      WebAppCommandScheduler::InstallIsolatedWebAppCallback callback,
      const base::Location& call_location) override {
    // This test is only interested in IsolatedWebAppInstallationManager
    // behavior, so stub out the install command to avoid needing to
    // configure it correctly.
    std::move(callback).Run(InstallIsolatedWebAppCommandSuccess(
        url_info, base::Version("0"),
        IwaStorageUnownedBundle(base::FilePath())));
  }
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
    std::optional<std::string_view> proxy_flag_value,
    std::optional<base::FilePath> bundle_flag_value) {
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
      CreateCommandLine("https://example.com:12345", std::nullopt),
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
      CreateCommandLine("https://example.com:12345", std::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallFromSecureRemoteProxySucceeds) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("https://example.com"),
      IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(), HasValue());
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallFromInsecureRemoteProxyFails) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("http://example.com"),
      IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(), ErrorIs(HasSubstr("Proxy URL not trustworthy")));
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallFromHttpLocalhostProxySucceeds) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("http://localhost:8080"),
      IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(), HasValue());
}

TEST_F(IsolatedWebAppInstallationManagerTest,
       InstallFromProxyWithBadSchemeFails) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("file://hi"),
      IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(),
              ErrorIs(HasSubstr("Proxy URL must be HTTP or HTTPS")));
}

TEST_F(IsolatedWebAppInstallationManagerTest, InstallFromProxyWithPathFails) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("http://localhost:8080/foo"),
      IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(), ErrorIs(HasSubstr("Non-origin URL provided")));
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
      CreateCommandLine("https://example.com:12345", std::nullopt),
      std::move(keep_alive), /*optional_profile_keep_alive=*/nullptr,
      base::TaskPriority::USER_VISIBLE);
  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
}

class IsolatedWebAppInstallationManagerCommandLineTest
    : public IsolatedWebAppInstallationManagerTest {
 protected:
  MaybeIwaInstallSource ParseCommandLine(
      std::optional<std::string_view> proxy_flag_value,
      std::optional<base::FilePath> bundle_flag_value) {
    base::test::TestFuture<MaybeIwaInstallSource> future;
    IsolatedWebAppInstallationManager::
        GetIsolatedWebAppInstallSourceFromCommandLine(
            CreateCommandLine(proxy_flag_value, bundle_flag_value),
            future.GetCallback());
    return future.Take();
  }
};

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagAbsent) {
  EXPECT_THAT(ParseCommandLine(std::nullopt, std::nullopt),
              ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagEmpty) {
  EXPECT_THAT(
      ParseCommandLine(std::nullopt, base::FilePath::FromUTF8Unsafe("")),
      ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagInvalid) {
  EXPECT_THAT(ParseCommandLine(std::nullopt, base::FilePath::FromUTF8Unsafe(
                                                 "does_not_exist.wbn")),
              ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagIsDirectory) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(ParseCommandLine(std::nullopt, cwd.directory()),
              ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(
      ParseCommandLine(std::nullopt, cwd.existing_file_name()),
      ValueIs(Optional(Property(
          &IsolatedWebAppInstallSource::source,
          Property(&IwaSourceWithModeAndFileOp::variant,
                   VariantWith<IwaSourceBundleWithModeAndFileOp>(
                       Eq(IwaSourceBundleWithModeAndFileOp(
                           cwd.existing_file_path(),
                           IwaSourceBundleModeAndFileOp::kDevModeCopy))))))));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       InstallsAppWhenProxyFlagAbsentAndBundleFlagValidAndAbsolute) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(
      ParseCommandLine(std::nullopt, cwd.existing_file_path()),
      ValueIs(Optional(Property(
          &IsolatedWebAppInstallSource::source,
          Property(&IwaSourceWithModeAndFileOp::variant,
                   VariantWith<IwaSourceBundleWithModeAndFileOp>(
                       Eq(IwaSourceBundleWithModeAndFileOp(
                           cwd.existing_file_path(),
                           IwaSourceBundleModeAndFileOp::kDevModeCopy))))))));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagAbsent) {
  EXPECT_THAT(ParseCommandLine("", std::nullopt), ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagEmpty) {
  EXPECT_THAT(ParseCommandLine("", base::FilePath::FromUTF8Unsafe("")),
              ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagEmptyAndBundleFlagInvalid) {
  EXPECT_THAT(ParseCommandLine(
                  "", base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
              ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       InstallsAppWhenProxyFlagEmptyAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(
      ParseCommandLine("", cwd.existing_file_name()),
      ValueIs(Optional(Property(
          &IsolatedWebAppInstallSource::source,
          Property(&IwaSourceWithModeAndFileOp::variant,
                   VariantWith<IwaSourceBundleWithModeAndFileOp>(
                       Eq(IwaSourceBundleWithModeAndFileOp(
                           cwd.existing_file_path(),
                           IwaSourceBundleModeAndFileOp::kDevModeCopy))))))));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagAbsent) {
  EXPECT_THAT(ParseCommandLine("invalid", std::nullopt),
              ErrorIs(HasSubstr("Proxy URL must be HTTP or HTTPS")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagEmpty) {
  EXPECT_THAT(ParseCommandLine("invalid", base::FilePath::FromUTF8Unsafe("")),
              ErrorIs(HasSubstr("Proxy URL must be HTTP or HTTPS")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagInvalid) {
  EXPECT_THAT(ParseCommandLine("invalid", base::FilePath::FromUTF8Unsafe(
                                              "does_not_exist.wbn")),
              ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(ParseCommandLine("invalid", cwd.existing_file_name()),
              ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagAbsent) {
  constexpr std::string_view kUrl = "https://example.com";
  EXPECT_THAT(ParseCommandLine(kUrl, std::nullopt),
              ValueIs(Optional(
                  Property(&IsolatedWebAppInstallSource::source,
                           Property(&IwaSourceWithModeAndFileOp::variant,
                                    VariantWith<IwaSourceProxy>(Property(
                                        "proxy_url", &IwaSourceProxy::proxy_url,
                                        IsSameOriginWith(GURL(kUrl)))))))));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       InstallsAppWhenProxyFlagWithPortValidAndBundleFlagAbsent) {
  constexpr std::string_view kUrl = "https://example.com:12345";
  EXPECT_THAT(ParseCommandLine(kUrl, std::nullopt),
              ValueIs(Optional(
                  Property(&IsolatedWebAppInstallSource::source,
                           Property(&IwaSourceWithModeAndFileOp::variant,
                                    VariantWith<IwaSourceProxy>(Property(
                                        "proxy_url", &IwaSourceProxy::proxy_url,
                                        IsSameOriginWith(GURL(kUrl)))))))));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagHasPathAndBundleFlagInValid) {
  EXPECT_THAT(ParseCommandLine("https://example.com/path", std::nullopt),
              ErrorIs(HasSubstr("Non-origin URL provided")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       InstallsAppWhenProxyFlagValidAndBundleFlagEmpty) {
  constexpr std::string_view kUrl = "https://example.com";
  EXPECT_THAT(ParseCommandLine(kUrl, base::FilePath::FromUTF8Unsafe("")),
              ValueIs(Optional(
                  Property(&IsolatedWebAppInstallSource::source,
                           Property(&IwaSourceWithModeAndFileOp::variant,
                                    VariantWith<IwaSourceProxy>(Property(
                                        "proxy_url", &IwaSourceProxy::proxy_url,
                                        IsSameOriginWith(GURL(kUrl)))))))));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagValidAndBundleFlagInvalid) {
  EXPECT_THAT(
      ParseCommandLine("https://example.com",
                       base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppInstallationManagerCommandLineTest,
       ErrorWhenProxyFlagValidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(ParseCommandLine("https://example.com", cwd.existing_file_name()),
              ErrorIs(HasSubstr("cannot both be provided")));
}

}  // namespace web_app
