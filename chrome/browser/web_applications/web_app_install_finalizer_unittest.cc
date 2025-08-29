// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/traits_bag.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using testing::_;

struct FinalizeInstallResult {
  webapps::AppId installed_app_id;
  webapps::InstallResultCode code;
};

}  // namespace

class TestInstallManagerObserver : public WebAppInstallManagerObserver {
 public:
  explicit TestInstallManagerObserver(WebAppInstallManager* install_manager) {
    install_manager_observation_.Observe(install_manager);
  }

  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override {
    web_app_manifest_updated_called_ = true;
  }

  bool web_app_manifest_updated_called() const {
    return web_app_manifest_updated_called_;
  }

 private:
  bool web_app_manifest_updated_called_ = false;
  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};
};

class WebAppInstallFinalizerUnitTest : public WebAppTest {
 public:
  WebAppInstallFinalizerUnitTest() = default;
  WebAppInstallFinalizerUnitTest(const WebAppInstallFinalizerUnitTest&) =
      delete;
  WebAppInstallFinalizerUnitTest& operator=(
      const WebAppInstallFinalizerUnitTest&) = delete;
  ~WebAppInstallFinalizerUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    auto install_manager = std::make_unique<WebAppInstallManager>(profile());
    install_manager_observer_ =
        std::make_unique<TestInstallManagerObserver>(install_manager.get());
    provider->SetInstallManager(std::move(install_manager));
    provider->SetInstallFinalizer(
        std::make_unique<WebAppInstallFinalizer>(profile()));
    provider->SetOriginAssociationManager(
        std::make_unique<FakeWebAppOriginAssociationManager>());

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    install_manager_observer_.reset();
    WebAppTest::TearDown();
  }

  // Synchronous version of FinalizeInstall.
  FinalizeInstallResult AwaitFinalizeInstall(
      const WebAppInstallInfo& info,
      const WebAppInstallFinalizer::FinalizeOptions& options) {
    FinalizeInstallResult result{};
    base::RunLoop run_loop;
    finalizer().FinalizeInstall(
        info, options,
        base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          result.installed_app_id = installed_app_id;
          result.code = code;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void AddFileHandler(
      std::vector<blink::mojom::ManifestFileHandlerPtr>* file_handlers) {
    auto file_handler = blink::mojom::ManifestFileHandler::New();
    file_handler->action = GURL("https://example.com/action");
    file_handler->name = u"Test handler";
    file_handler->accept[u"application/pdf"].emplace_back(u".pdf");
    file_handlers->push_back(std::move(file_handler));
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
  WebAppInstallFinalizer& finalizer() { return provider().install_finalizer(); }
  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }
  WebAppInstallManager& install_manager() {
    return provider().install_manager();
  }

 protected:
  std::unique_ptr<TestInstallManagerObserver> install_manager_observer_;

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(WebAppInstallFinalizerUnitTest, BasicInstallSucceeds) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));
}

TEST_F(WebAppInstallFinalizerUnitTest, ConcurrentInstallSucceeds) {
  auto info1 = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo1.example"));
  info1->title = u"Foo1 Title";

  auto info2 = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo2.example"));
  info2->title = u"Foo2 Title";

  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  base::RunLoop run_loop;
  bool callback1_called = false;
  bool callback2_called = false;

  // Start install finalization for the 1st app.
  {
    finalizer().FinalizeInstall(
        *info1, options,
        base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(
              installed_app_id,
              GenerateAppId(/*manifest_id=*/std::nullopt, info1->start_url()));
          callback1_called = true;
          if (callback2_called)
            run_loop.Quit();
        }));
  }

  // Start install finalization for the 2nd app.
  {
    finalizer().FinalizeInstall(
        *info2, options,
        base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(
              installed_app_id,
              GenerateAppId(/*manifest_id=*/std::nullopt, info2->start_url()));
          callback2_called = true;
          if (callback1_called)
            run_loop.Quit();
        }));
  }

  run_loop.Run();

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallStoresLatestWebAppInstallSource) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::WebappInstallSource::INTERNAL_DEFAULT,
            *registrar().GetLatestAppInstallSource(result.installed_app_id));
}

TEST_F(WebAppInstallFinalizerUnitTest, OnWebAppManifestUpdatedTriggered) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      update_future;
  finalizer().FinalizeUpdate(*info, update_future.GetCallback());
  ASSERT_TRUE(update_future.Wait());
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());
}

TEST_F(WebAppInstallFinalizerUnitTest, ManifestUpdateOsIntegrationDefaultApps) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  options.install_state = proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
  options.add_to_applications_menu = false;
  options.add_to_quick_launch_bar = false;
  options.add_to_desktop = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(result.installed_app_id));

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      update_future;
  finalizer().FinalizeUpdate(*info, update_future.GetCallback());
  ASSERT_TRUE(update_future.Wait());
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());

  // Post manifest update, OS integration is not triggered for default apps.
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(result.installed_app_id));
}

TEST_F(WebAppInstallFinalizerUnitTest,
       NonLocalThenLocalInstallSetsBothInstallTime) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);
  options.install_state = proto::SUGGESTED_FROM_ANOTHER_DEVICE;
  // OS Hooks must be disabled for non-locally installed app.
  options.add_to_applications_menu = false;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(proto::SUGGESTED_FROM_ANOTHER_DEVICE,
              installed_app->install_state());
    EXPECT_TRUE(installed_app->first_install_time().is_null());
    EXPECT_TRUE(installed_app->latest_install_time().is_null());
  }

  options.install_state = proto::INSTALLED_WITH_OS_INTEGRATION;

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
              installed_app->install_state());
    EXPECT_FALSE(installed_app->first_install_time().is_null());
    EXPECT_FALSE(installed_app->latest_install_time().is_null());
    EXPECT_EQ(installed_app->first_install_time(),
              installed_app->latest_install_time());
  }
}

TEST_F(WebAppInstallFinalizerUnitTest,
       LatestInstallTimeAlwaysUpdatedIfReinstalled) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);
  options.add_to_applications_menu = false;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_state = proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;

  base::Time old_first_install_time;
  base::Time old_latest_install_time;

  base::SimpleTestClock test_clock;
  finalizer().SetClockForTesting(&test_clock);
  auto toProtoResolutionTime = [](base::Time time) {
    return syncer::ProtoTimeToTime(syncer::TimeToProtoTime(time));
  };
  test_clock.SetNow(toProtoResolutionTime(base::Time::Now()));

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(installed_app->install_state(),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    old_first_install_time = installed_app->first_install_time();
    old_latest_install_time = installed_app->latest_install_time();
    EXPECT_FALSE(old_first_install_time.is_null());
    EXPECT_FALSE(old_latest_install_time.is_null());
    EXPECT_EQ(old_first_install_time, old_latest_install_time);
    EXPECT_EQ(old_first_install_time, toProtoResolutionTime(test_clock.Now()));
  }
  test_clock.Advance(base::Hours(1));

  // Try reinstalling the same app again, the latest install time should be
  // updated but the first install time should still stay the same.
  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(installed_app->install_state(),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    EXPECT_FALSE(installed_app->first_install_time().is_null());
    EXPECT_FALSE(installed_app->latest_install_time().is_null());
    EXPECT_EQ(installed_app->first_install_time(), old_first_install_time);
    EXPECT_NE(installed_app->latest_install_time(), old_latest_install_time);
    EXPECT_EQ(installed_app->latest_install_time(),
              toProtoResolutionTime(test_clock.Now()));
  }
  // Reset the clock to the default clock, so raw_ptr issues don't happen.
  finalizer().SetClockForTesting(base::DefaultClock::GetInstance());
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallNoDesktopShortcut) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallNoQuickLaunchBarShortcut) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_quick_launch_bar = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));
}

TEST_F(WebAppInstallFinalizerUnitTest,
       InstallNoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallNoCreateOsShorcuts) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));
}

TEST_F(WebAppInstallFinalizerUnitTest,
       InstallOsHooksEnabledForUserInstalledApps) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallOsHooksDisabledForDefaultApps) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  options.install_state = proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
  options.add_to_applications_menu = false;
  options.add_to_quick_launch_bar = false;
  options.add_to_desktop = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));

  // Update the app, adding a file handler.
  std::vector<blink::mojom::ManifestFileHandlerPtr> file_handlers;
  AddFileHandler(&file_handlers);
  PopulateFileHandlerInfoFromManifest(file_handlers, info->start_url(),
                                      info.get());

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      update_future;
  finalizer().FinalizeUpdate(*info, update_future.GetCallback());
  auto [app_id, code] = update_future.Take();
  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled, code);
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallUrlSetInWebAppDB) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  info->install_url = GURL("https://foo.example/installer");
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));

  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  const WebApp::ExternalConfigMap& config_map =
      installed_app->management_to_external_config_map();
  auto it = config_map.find(WebAppManagement::kPolicy);
  EXPECT_NE(it, config_map.end());
  EXPECT_EQ(1u, it->second.install_urls.size());
  EXPECT_EQ(GURL("https://foo.example/installer"),
            *it->second.install_urls.begin());
}

TEST_F(WebAppInstallFinalizerUnitTest, IsolationDataSetInWebAppDB) {
  IwaVersion version = *IwaVersion::Create("1.2.3");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("isolated-app://random_app"));
  info->title = u"Foo Title";
  info->set_isolated_web_app_version(version);

  const IsolatedWebAppStorageLocation location(
      IwaStorageUnownedBundle{base::FilePath(FILE_PATH_LITERAL("p"))});
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  auto integrity_block_data =
      IsolatedWebAppIntegrityBlockData(test::CreateSignatures());
  options.iwa_options = WebAppInstallFinalizer::FinalizeOptions::IwaOptions(
      location, integrity_block_data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/std::nullopt, info->start_url()));

  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_THAT(
      installed_app,
      test::IwaIs(_, test::IsolationDataIs(location, version,
                                           /*controlled_frame_partiions=*/_,
                                           /*pending_update_info=*/std::nullopt,
                                           integrity_block_data)));
}

TEST_F(WebAppInstallFinalizerUnitTest, PopUpContentSettingsGrantedForIwa) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle();
  app->TrustSigningKey();
  const web_app::IsolatedWebAppUrlInfo url_info =
      app->InstallChecked(profile());
  scoped_refptr<HostContentSettingsMap> host_content_settings_map(
      HostContentSettingsMapFactory::GetForProfile(profile()));

  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url_info.origin().GetURL(), url_info.origin().GetURL(),
                ContentSettingsType::POPUPS));
}

TEST_F(WebAppInstallFinalizerUnitTest, ValidateOriginAssociationsApproved) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  auto scope_extension =
      ScopeExtensionInfo::CreateForScope(start_url,
                                         /*has_origin_wildcard=*/true);
  CHECK(!scope_extension.origin.opaque());
  info->scope_extensions = {scope_extension};

  // Set data such that scope_extension will be returned in validated data.
  std::map<ScopeExtensionInfo, ScopeExtensionInfo> data = {
      {scope_extension, scope_extension}};
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetData(data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(installed_app->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(ScopeExtensions({scope_extension}),
            installed_app->validated_scope_extensions());
}

TEST_F(WebAppInstallFinalizerUnitTest, ValidateOriginAssociationsDenied) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  auto scope_extension =
      ScopeExtensionInfo::CreateForScope(start_url,
                                         /*has_origin_wildcard=*/true);
  info->scope_extensions = {scope_extension};

  // Set data such that scope_extension will not be returned in validated data.
  std::map<ScopeExtensionInfo, ScopeExtensionInfo> data;
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetData(data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(installed_app->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(ScopeExtensions(), installed_app->validated_scope_extensions());
}

class WebAppInstallFinalizerUnitTestQueriesAndFragments
    : public WebAppInstallFinalizerUnitTest,
      public testing::WithParamInterface<std::tuple<std::string, std::string>> {
 public:
  WebAppInstallFinalizerUnitTestQueriesAndFragments() = default;
  WebAppInstallFinalizerUnitTestQueriesAndFragments(
      const WebAppInstallFinalizerUnitTestQueriesAndFragments&) = delete;
  WebAppInstallFinalizerUnitTestQueriesAndFragments& operator=(
      const WebAppInstallFinalizerUnitTestQueriesAndFragments&) = delete;
  ~WebAppInstallFinalizerUnitTestQueriesAndFragments() override = default;
};

TEST_P(WebAppInstallFinalizerUnitTestQueriesAndFragments,
       ValidateOriginAssociationsDropQueriesAndFragments) {
  std::string start_url_str, expected_sanitized_start_url_str;
  std::tie(start_url_str, expected_sanitized_start_url_str) = GetParam();
  GURL start_url(start_url_str);
  GURL expected_sanitized_start_url(expected_sanitized_start_url_str);
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  auto scope_extension =
      ScopeExtensionInfo::CreateForScope(start_url,
                                         /*has_origin_wildcard=*/true);
  CHECK(!scope_extension.origin.opaque());
  info->scope_extensions = {scope_extension};

  // Set data such that scope_extension will be returned in validated data.
  std::map<ScopeExtensionInfo, ScopeExtensionInfo> data = {
      {scope_extension, scope_extension}};
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetData(data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(installed_app->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(ScopeExtensions({scope_extension}),
            installed_app->validated_scope_extensions());
  for (const auto& scope_ext_info :
       installed_app->validated_scope_extensions()) {
    ASSERT_EQ(expected_sanitized_start_url, scope_ext_info.scope);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebAppInstallFinalizerUnitTestQueriesAndFragments,
    testing::Values(
        std::tuple<std::string, std::string>("https://foo.example/path",
                                             "https://foo.example/path"),
        std::tuple<std::string, std::string>(
            "https://foo.example/search?q=querystring",
            "https://foo.example/search"),
        std::tuple<std::string, std::string>("https://foo.example/#hello",
                                             "https://foo.example"),
        std::tuple<std::string, std::string>(
            "https://foo.example/search?q=querystring#hello",
            "https://foo.example/search")));
}  // namespace web_app
