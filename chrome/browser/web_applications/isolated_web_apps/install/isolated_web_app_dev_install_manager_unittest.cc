// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_dev_install_manager.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

MATCHER_P(IsSameOriginWith, url, "") {
  *result_listener << "where it is not same origin with " << url.spec();
  return arg.IsSameOriginWith(url);
}

using MaybeInstallIsolatedWebAppCommandSuccess =
    IsolatedWebAppDevInstallManager::MaybeInstallIsolatedWebAppCommandSuccess;
using MaybeIwaInstallSource =
    IsolatedWebAppDevInstallManager::MaybeIwaInstallSource;

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

class IsolatedWebAppDevInstallManagerTest : public WebAppTest {
 public:
  IsolatedWebAppDevInstallManagerTest()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory()) {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void SetUpFakeProxyInstallPage() {
    auto& web_contents_manager = static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
    web_contents_manager.TrackLoadUrlCalls(base::BindLambdaForTesting(
        [&](content::NavigationController::LoadURLParams& load_url_params,
            content::WebContents*, webapps::WebAppUrlLoader::UrlComparison) {
          ASSERT_OK_AND_ASSIGN(auto url_info, IsolatedWebAppUrlInfo::Create(
                                                  load_url_params.url));
          GURL icon_url = url_info.origin().GetURL().Resolve("/icon.png");
          auto& icon_state =
              web_contents_manager.GetOrCreateIconState(icon_url);
          icon_state.bitmaps = {CreateSquareIcon(256, SK_ColorBLUE)};

          auto& page_state =
              web_contents_manager.GetOrCreatePageState(load_url_params.url);
          page_state.url_load_result =
              webapps::WebAppUrlLoaderResult::kUrlLoaded;
          page_state.error_code =
              webapps::InstallableStatusCode::NO_ERROR_DETECTED;
          page_state.manifest_url = url_info.origin().GetURL().Resolve(
              "/.well-known/manifest.webmanifest");
          page_state.valid_manifest_for_web_app = true;
          page_state.opt_manifest =
              ManifestBuilder()
                  .SetVersion("1.0.0")
                  .AddIcon("/icon.png", gfx::Size(256, 256), "image/png")
                  .ToBlinkManifest(url_info.origin());
        }));
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile()->GetTestingPrefService();
  }

  IsolatedWebAppDevInstallManager& manager() {
    return IsolatedWebAppDevInstallManager::From(fake_provider());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(IsolatedWebAppDevInstallManagerTest, NoInstallationWhenFeatureDisabled) {
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

TEST_F(IsolatedWebAppDevInstallManagerTest,
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

TEST_F(IsolatedWebAppDevInstallManagerTest,
       InstallFromSecureRemoteProxySucceeds) {
  SetUpFakeProxyInstallPage();
  const GURL kProxyUrl("https://example.com");

  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      kProxyUrl, IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
      future.GetCallback());

  ASSERT_OK_AND_ASSIGN(InstallIsolatedWebAppCommandSuccess result,
                       future.Take());
  EXPECT_EQ(result.installed_version, *IwaVersion::Create("1.0.0"));
  EXPECT_THAT(
      result.location.variant(),
      VariantWith<IwaStorageProxy>(Property(
          &IwaStorageProxy::proxy_url, Eq(url::Origin::Create(kProxyUrl)))));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(result.url_info.app_id());
  ASSERT_NE(web_app, nullptr);
  EXPECT_EQ(web_app->isolation_data()->version(), *IwaVersion::Create("1.0.0"));
  EXPECT_THAT(
      web_app->isolation_data()->location().variant(),
      VariantWith<IwaStorageProxy>(Property(
          &IwaStorageProxy::proxy_url, Eq(url::Origin::Create(kProxyUrl)))));
}

TEST_F(IsolatedWebAppDevInstallManagerTest,
       InstallFromInsecureRemoteProxyFails) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("http://example.com"),
      IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(), ErrorIs(HasSubstr("Proxy URL not trustworthy")));
}

TEST_F(IsolatedWebAppDevInstallManagerTest,
       InstallFromHttpLocalhostProxySucceeds) {
  SetUpFakeProxyInstallPage();
  const GURL kProxyUrl("http://localhost:8080");

  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      kProxyUrl, IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
      future.GetCallback());

  ASSERT_OK_AND_ASSIGN(InstallIsolatedWebAppCommandSuccess result,
                       future.Take());
  EXPECT_EQ(result.installed_version, *IwaVersion::Create("1.0.0"));
  EXPECT_THAT(
      result.location.variant(),
      VariantWith<IwaStorageProxy>(Property(
          &IwaStorageProxy::proxy_url, Eq(url::Origin::Create(kProxyUrl)))));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(result.url_info.app_id());
  ASSERT_NE(web_app, nullptr);
  EXPECT_EQ(web_app->isolation_data()->version(), *IwaVersion::Create("1.0.0"));
  EXPECT_THAT(
      web_app->isolation_data()->location().variant(),
      VariantWith<IwaStorageProxy>(Property(
          &IwaStorageProxy::proxy_url, Eq(url::Origin::Create(kProxyUrl)))));
}

TEST_F(IsolatedWebAppDevInstallManagerTest,
       InstallFromProxyWithBadSchemeFails) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("file://hi"),
      IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(),
              ErrorIs(HasSubstr("Proxy URL must be HTTP or HTTPS")));
}

TEST_F(IsolatedWebAppDevInstallManagerTest, InstallFromProxyWithPathFails) {
  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().InstallIsolatedWebAppFromDevModeProxy(
      GURL("http://localhost:8080/foo"),
      IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(future.Take(), ErrorIs(HasSubstr("Non-origin URL provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerTest,
       NoInstallationWhenDevModePolicyDisabled) {
  pref_service()->SetManagedPref(
      prefs::kDevToolsAvailability,
      base::Value(std::to_underlying(
          policy::DeveloperToolsAvailability::kDisallowed)));

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

TEST_F(IsolatedWebAppDevInstallManagerTest, DownloadAndInstallSucceeds) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle();
  bundle->FakeInstallPageState(profile());
  profile_url_loader_factory().AddResponse("https://example.com",
                                           bundle->GetBundleData());

  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().DownloadAndInstallIsolatedWebAppFromDevModeBundle(
      GURL{"https://example.com"},
      IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
      future.GetCallback());

  ASSERT_OK_AND_ASSIGN(InstallIsolatedWebAppCommandSuccess result,
                       future.Take());
  EXPECT_EQ(result.url_info.web_bundle_id(), bundle->web_bundle_id());
  EXPECT_EQ(result.installed_version, *IwaVersion::Create("1.0.0"));
  EXPECT_THAT(result.location.variant(),
              VariantWith<IwaStorageOwnedBundle>(
                  Property(&IwaStorageOwnedBundle::dev_mode, Eq(true))));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(result.url_info.app_id());
  ASSERT_NE(web_app, nullptr);
  EXPECT_EQ(web_app->isolation_data()->version(), *IwaVersion::Create("1.0.0"));
}

TEST_F(IsolatedWebAppDevInstallManagerTest, DownloadAndInstallFailsWrongUrl) {
  profile_url_loader_factory().AddResponse("https://example.com", "",
                                           net::HTTP_FORBIDDEN);

  base::test::TestFuture<MaybeInstallIsolatedWebAppCommandSuccess> future;
  manager().DownloadAndInstallIsolatedWebAppFromDevModeBundle(
      GURL{"https://example.com"},
      IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
      future.GetCallback());

  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Network error while downloading bundle file")));
}

class IsolatedWebAppDevInstallManagerCommandLineTest
    : public IsolatedWebAppDevInstallManagerTest {
 protected:
  MaybeIwaInstallSource ParseCommandLine(
      std::optional<std::string_view> proxy_flag_value,
      std::optional<base::FilePath> bundle_flag_value) {
    base::test::TestFuture<MaybeIwaInstallSource> future;
    IsolatedWebAppDevInstallManager::
        GetIsolatedWebAppInstallSourceFromCommandLine(
            CreateCommandLine(proxy_flag_value, bundle_flag_value),
            future.GetCallback());
    return future.Take();
  }
};

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagAbsent) {
  EXPECT_THAT(ParseCommandLine(std::nullopt, std::nullopt),
              ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       NoInstallationWhenProxyFlagAbsentAndBundleFlagEmpty) {
  EXPECT_THAT(
      ParseCommandLine(std::nullopt, base::FilePath::FromUTF8Unsafe("")),
      ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagInvalid) {
  EXPECT_THAT(ParseCommandLine(std::nullopt, base::FilePath::FromUTF8Unsafe(
                                                 "does_not_exist.wbn")),
              ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagAbsentAndBundleFlagIsDirectory) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(ParseCommandLine(std::nullopt, cwd.directory()),
              ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
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

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
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

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagAbsent) {
  EXPECT_THAT(ParseCommandLine("", std::nullopt), ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       NoInstallationWhenProxyFlagEmptyAndBundleFlagEmpty) {
  EXPECT_THAT(ParseCommandLine("", base::FilePath::FromUTF8Unsafe("")),
              ValueIs(Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagEmptyAndBundleFlagInvalid) {
  EXPECT_THAT(ParseCommandLine(
                  "", base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
              ErrorIs(HasSubstr("Invalid path provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
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

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagAbsent) {
  EXPECT_THAT(ParseCommandLine("invalid", std::nullopt),
              ErrorIs(HasSubstr("Proxy URL must be HTTP or HTTPS")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagEmpty) {
  EXPECT_THAT(ParseCommandLine("invalid", base::FilePath::FromUTF8Unsafe("")),
              ErrorIs(HasSubstr("Proxy URL must be HTTP or HTTPS")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagInvalid) {
  EXPECT_THAT(ParseCommandLine("invalid", base::FilePath::FromUTF8Unsafe(
                                              "does_not_exist.wbn")),
              ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagInvalidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(ParseCommandLine("invalid", cwd.existing_file_name()),
              ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
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

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
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

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagHasPathAndBundleFlagInValid) {
  EXPECT_THAT(ParseCommandLine("https://example.com/path", std::nullopt),
              ErrorIs(HasSubstr("Non-origin URL provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
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

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagValidAndBundleFlagInvalid) {
  EXPECT_THAT(
      ParseCommandLine("https://example.com",
                       base::FilePath::FromUTF8Unsafe("does_not_exist.wbn")),
      ErrorIs(HasSubstr("cannot both be provided")));
}

TEST_F(IsolatedWebAppDevInstallManagerCommandLineTest,
       ErrorWhenProxyFlagValidAndBundleFlagValid) {
  ScopedWorkingDirectoryWithFile cwd;
  EXPECT_THAT(ParseCommandLine("https://example.com", cwd.existing_file_name()),
              ErrorIs(HasSubstr("cannot both be provided")));
}

}  // namespace web_app
