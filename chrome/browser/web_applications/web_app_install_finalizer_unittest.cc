// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <initializer_list>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/traits_bag.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
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
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/browser/install_result_code.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

struct FinalizeInstallResult {
  AppId installed_app_id;
  webapps::InstallResultCode code;
  OsHooksErrors os_hooks_errors;
};

}  // namespace

class TestInstallManagerObserver : public WebAppInstallManagerObserver {
 public:
  explicit TestInstallManagerObserver(WebAppInstallManager* install_manager) {
    install_manager_observation_.Observe(install_manager);
  }

  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) override {
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

class WebAppInstallFinalizerUnitTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  WebAppInstallFinalizerUnitTest() {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kOsIntegrationSubManagers, {{"stage", "write_config"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kOsIntegrationSubManagers});
    }
  }
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
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code,
                                       OsHooksErrors os_hooks_errors) {
          result.installed_app_id = installed_app_id;
          result.code = code;
          result.os_hooks_errors = os_hooks_errors;
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
  FakeOsIntegrationManager& os_integration_manager() {
    return static_cast<FakeOsIntegrationManager&>(
        provider().os_integration_manager());
  }
  std::unique_ptr<TestInstallManagerObserver> install_manager_observer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppInstallFinalizerUnitTest, BasicInstallSucceeds) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));
  EXPECT_EQ(0u, os_integration_manager().num_register_run_on_os_login_calls());
}

TEST_P(WebAppInstallFinalizerUnitTest, ConcurrentInstallSucceeds) {
  auto info1 = std::make_unique<WebAppInstallInfo>();
  info1->start_url = GURL("https://foo1.example");
  info1->title = u"Foo1 Title";

  auto info2 = std::make_unique<WebAppInstallInfo>();
  info2->start_url = GURL("https://foo2.example");
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
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code,
                                       OsHooksErrors os_hooks_errors) {
          EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(
              installed_app_id,
              GenerateAppId(/*manifest_id=*/absl::nullopt, info1->start_url));
          EXPECT_TRUE(os_hooks_errors.none());
          callback1_called = true;
          if (callback2_called)
            run_loop.Quit();
        }));
  }

  // Start install finalization for the 2nd app.
  {
    finalizer().FinalizeInstall(
        *info2, options,
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code,
                                       OsHooksErrors os_hooks_errors) {
          EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(
              installed_app_id,
              GenerateAppId(/*manifest_id=*/absl::nullopt, info2->start_url));
          EXPECT_TRUE(os_hooks_errors.none());
          callback2_called = true;
          if (callback1_called)
            run_loop.Quit();
        }));
  }

  run_loop.Run();

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
}

TEST_P(WebAppInstallFinalizerUnitTest, InstallStoresLatestWebAppInstallSource) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::WebappInstallSource::INTERNAL_DEFAULT,
            *registrar().GetLatestAppInstallSource(result.installed_app_id));
}

TEST_P(WebAppInstallFinalizerUnitTest, OnWebAppManifestUpdatedTriggered) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);
  base::RunLoop runloop;
  finalizer().FinalizeUpdate(
      *info, base::BindLambdaForTesting(
                 [&](const AppId& app_id, webapps::InstallResultCode code,
                     OsHooksErrors os_hooks_errors) { runloop.Quit(); }));
  runloop.Run();
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());
}

TEST_P(WebAppInstallFinalizerUnitTest,
       NonLocalThenLocalInstallSetsInstallTime) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);
  options.locally_installed = false;
  // OS Hooks must be disabled for non-locally installed app.
  options.add_to_applications_menu = false;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_FALSE(installed_app->is_locally_installed());
    EXPECT_TRUE(installed_app->install_time().is_null());
  }

  options.locally_installed = true;

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_TRUE(installed_app->is_locally_installed());
    EXPECT_FALSE(installed_app->install_time().is_null());
  }
}

TEST_P(WebAppInstallFinalizerUnitTest, InstallNoDesktopShortcut) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  EXPECT_EQ(1u, os_integration_manager().num_create_shortcuts_calls());
  EXPECT_FALSE(os_integration_manager().did_add_to_desktop().value());
  EXPECT_EQ(1u,
            os_integration_manager().num_add_app_to_quick_launch_bar_calls());
}

TEST_P(WebAppInstallFinalizerUnitTest, InstallNoQuickLaunchBarShortcut) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_quick_launch_bar = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  EXPECT_EQ(1u, os_integration_manager().num_create_shortcuts_calls());
  EXPECT_TRUE(os_integration_manager().did_add_to_desktop().value());
  EXPECT_EQ(0u,
            os_integration_manager().num_add_app_to_quick_launch_bar_calls());
}

TEST_P(WebAppInstallFinalizerUnitTest,
       InstallNoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  EXPECT_EQ(1u, os_integration_manager().num_create_shortcuts_calls());
  EXPECT_FALSE(os_integration_manager().did_add_to_desktop().value());
  EXPECT_EQ(0u,
            os_integration_manager().num_add_app_to_quick_launch_bar_calls());
}

TEST_P(WebAppInstallFinalizerUnitTest, InstallNoCreateOsShorcuts) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  os_integration_manager().set_can_create_shortcuts(false);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  EXPECT_EQ(0u, os_integration_manager().num_create_shortcuts_calls());
}

TEST_P(WebAppInstallFinalizerUnitTest,
       InstallOsHooksEnabledForUserInstalledApps) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  EXPECT_EQ(1u, os_integration_manager().num_create_file_handlers_calls());
}

TEST_P(WebAppInstallFinalizerUnitTest, InstallOsHooksDisabledForDefaultApps) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

#if BUILDFLAG(IS_CHROMEOS)
  // OS integration is always enabled in ChromeOS
  EXPECT_EQ(1u, os_integration_manager().num_create_file_handlers_calls());
#else
  EXPECT_EQ(0u, os_integration_manager().num_create_file_handlers_calls());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Update the app, adding a file handler.
  std::vector<blink::mojom::ManifestFileHandlerPtr> file_handlers;
  AddFileHandler(&file_handlers);
  info->file_handlers =
      CreateFileHandlersFromManifest(file_handlers, info->start_url);

  base::RunLoop runloop;
  finalizer().FinalizeUpdate(
      *info, base::BindLambdaForTesting([&](const AppId& app_id,
                                            webapps::InstallResultCode code,
                                            OsHooksErrors os_hooks_errors) {
        EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled, code);
        EXPECT_TRUE(os_hooks_errors.none());
        runloop.Quit();
      }));
  runloop.Run();

#if BUILDFLAG(IS_CHROMEOS)
  // OS integration is always enabled in ChromeOS
  EXPECT_EQ(1u, os_integration_manager().num_update_file_handlers_calls());
#else
  EXPECT_EQ(0u, os_integration_manager().num_update_file_handlers_calls());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_P(WebAppInstallFinalizerUnitTest, InstallUrlSetInWebAppDB) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  info->install_url = GURL("https://foo.example/installer");
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  const WebApp::ExternalConfigMap& config_map =
      installed_app->management_to_external_config_map();
  auto it = config_map.find(WebAppManagement::kPolicy);
  EXPECT_NE(it, config_map.end());
  EXPECT_EQ(1u, it->second.install_urls.size());
  EXPECT_EQ(GURL("https://foo.example/installer"),
            *it->second.install_urls.begin());
}

TEST_P(WebAppInstallFinalizerUnitTest, IsolationDataSetInWebAppDB) {
  base::Version version("1.2.3");

  WebAppInstallInfo info;
  info.start_url = GURL("https://foo.example");
  info.title = u"Foo Title";
  info.isolated_web_app_version = version;

  const IsolatedWebAppLocation location =
      DevModeBundle{.path = base::FilePath(FILE_PATH_LITERAL("p"))};
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::EXTERNAL_POLICY);
  options.isolated_web_app_location = location;

  FinalizeInstallResult result = AwaitFinalizeInstall(info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info.start_url));

  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(location, installed_app->isolation_data()->location);
  EXPECT_EQ(version, installed_app->isolation_data()->version);
}

TEST_P(WebAppInstallFinalizerUnitTest, ValidateOriginAssociationsApproved) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  ScopeExtensionInfo scope_extension =
      ScopeExtensionInfo(url::Origin::Create(GURL("htps://foo.example")),
                         /*has_origin_wildcard=*/true);
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
  EXPECT_TRUE(installed_app->is_locally_installed());
  EXPECT_EQ(ScopeExtensions({scope_extension}),
            installed_app->validated_scope_extensions());
}

TEST_P(WebAppInstallFinalizerUnitTest, ValidateOriginAssociationsDenied) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  ScopeExtensionInfo scope_extension =
      ScopeExtensionInfo(url::Origin::Create(GURL("htps://foo.example")),
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
  EXPECT_TRUE(installed_app->is_locally_installed());
  EXPECT_EQ(ScopeExtensions(), installed_app->validated_scope_extensions());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppInstallFinalizerUnitTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace web_app
