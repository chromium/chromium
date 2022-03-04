// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

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
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};
};

class WebAppInstallFinalizerUnitTest : public WebAppTest {
 public:
  WebAppInstallFinalizerUnitTest() {
    scoped_feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI},
                                          {});
  }
  WebAppInstallFinalizerUnitTest(const WebAppInstallFinalizerUnitTest&) =
      delete;
  WebAppInstallFinalizerUnitTest& operator=(
      const WebAppInstallFinalizerUnitTest&) = delete;
  ~WebAppInstallFinalizerUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    install_manager_observer_ =
        std::make_unique<TestInstallManagerObserver>(install_manager_.get());
    fake_registry_controller_->SetUp(profile());
    icon_manager_ = std::make_unique<WebAppIconManager>(
        profile(), base::MakeRefCounted<FileUtilsWrapper>());
    policy_manager_ = std::make_unique<WebAppPolicyManager>(profile());
    ui_manager_ = std::make_unique<FakeWebAppUiManager>();
    finalizer_ = std::make_unique<WebAppInstallFinalizer>(profile());

    icon_manager_->SetSubsystems(&registrar(), &install_manager());
    finalizer_->SetSubsystems(
        &install_manager(), &registrar(), ui_manager_.get(),
        &fake_registry_controller_->sync_bridge(),
        &fake_registry_controller_->os_integration_manager(),
        icon_manager_.get(), policy_manager_.get(),
        &fake_registry_controller_->translation_manager());
    fake_registry_controller_->Init();
    finalizer_->Start();
  }

  void TearDown() override {
    install_manager_observer_.reset();
    finalizer_.reset();
    ui_manager_.reset();
    policy_manager_.reset();
    icon_manager_.reset();
    fake_registry_controller_.reset();
    install_manager_.reset();
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

  WebAppInstallFinalizer& finalizer() { return *finalizer_; }
  WebAppRegistrar& registrar() {
    return fake_registry_controller_->registrar();
  }
  WebAppInstallManager& install_manager() const { return *install_manager_; }

 protected:
  FakeOsIntegrationManager& os_integration_manager() {
    return fake_registry_controller_->os_integration_manager();
  }
  std::unique_ptr<WebAppInstallFinalizer> finalizer_;
  std::unique_ptr<TestInstallManagerObserver> install_manager_observer_;

 private:
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppPolicyManager> policy_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebAppInstallFinalizerUnitTest, BasicInstallSucceeds) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));
  EXPECT_EQ(0u, os_integration_manager().num_register_run_on_os_login_calls());
}

TEST_F(WebAppInstallFinalizerUnitTest, ConcurrentInstallSucceeds) {
  auto info1 = std::make_unique<WebAppInstallInfo>();
  info1->start_url = GURL("https://foo1.example");
  info1->title = u"Foo1 Title";

  auto info2 = std::make_unique<WebAppInstallInfo>();
  info2->start_url = GURL("https://foo2.example");
  info2->title = u"Foo2 Title";

  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

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

TEST_F(WebAppInstallFinalizerUnitTest, InstallStoresLatestWebAppInstallSource) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(
      webapps::WebappInstallSource::INTERNAL_DEFAULT,
      *registrar().GetAppInstallSourceForMetrics(result.installed_app_id));
}

TEST_F(WebAppInstallFinalizerUnitTest, OnWebAppManifestUpdatedTriggered) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::EXTERNAL_POLICY;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);
  base::RunLoop runloop;
  finalizer_->FinalizeUpdate(
      *info,
      base::BindLambdaForTesting(
          [&](const web_app::AppId& app_id, webapps::InstallResultCode code,
              web_app::OsHooksErrors os_hooks_errors) { runloop.Quit(); }));
  runloop.Run();
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallNoDesktopShortcut) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;
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

TEST_F(WebAppInstallFinalizerUnitTest, InstallNoQuickLaunchBarShortcut) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;
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

TEST_F(WebAppInstallFinalizerUnitTest,
       InstallNoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;
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

TEST_F(WebAppInstallFinalizerUnitTest, InstallNoCreateOsShorcuts) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  os_integration_manager().set_can_create_shortcuts(false);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  EXPECT_EQ(0u, os_integration_manager().num_create_shortcuts_calls());
}

TEST_F(WebAppInstallFinalizerUnitTest,
       InstallOsHooksEnabledForUserInstalledApps) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));

  EXPECT_EQ(1u, os_integration_manager().num_create_file_handlers_calls());
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallOsHooksDisabledForDefaultApps) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::EXTERNAL_DEFAULT;

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
  finalizer_->FinalizeUpdate(
      *info,
      base::BindLambdaForTesting([&](const web_app::AppId& app_id,
                                     webapps::InstallResultCode code,
                                     web_app::OsHooksErrors os_hooks_errors) {
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

}  // namespace web_app
