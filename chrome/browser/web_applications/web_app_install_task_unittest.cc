// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_task.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#endif

namespace web_app {

namespace {

WebAppInstallManager::InstallParams MakeParams(
    web_app::DisplayMode display_mode = DisplayMode::kUndefined) {
  WebAppInstallManager::InstallParams params;
  params.fallback_start_url = GURL("https://example.com/fallback");
  params.user_display_mode = display_mode;
  return params;
}

}  // namespace

class WebAppInstallTaskTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), registrar(),
                                                        std::move(file_utils));

    ui_manager_ = std::make_unique<TestWebAppUiManager>();

    install_finalizer_ =
        std::make_unique<WebAppInstallFinalizer>(profile(), icon_manager_.get(),
                                                 /*legacy_finalizer=*/nullptr);
    os_integration_manager_ = std::make_unique<TestOsIntegrationManager>(
        profile(), /*app_shortcut_manager=*/nullptr,
        /*file_handler_manager=*/nullptr);

    install_finalizer_->SetSubsystems(
        &registrar(), ui_manager_.get(),
        &test_registry_controller_->sync_bridge());

    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever_ = data_retriever.get();

    install_task_ = std::make_unique<WebAppInstallTask>(
        profile(), os_integration_manager_.get(), install_finalizer_.get(),
        std::move(data_retriever));

    url_loader_ = std::make_unique<TestWebAppUrlLoader>();
    controller().Init();
    install_finalizer_->Start();

#if defined(OS_CHROMEOS)
    arc_test_.SetUp(profile());

    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();
    intent_helper_bridge_ = std::make_unique<arc::ArcIntentHelperBridge>(
        profile(), arc_bridge_service);
    fake_intent_helper_instance_ =
        std::make_unique<arc::FakeIntentHelperInstance>();
    arc_bridge_service->intent_helper()->SetInstance(
        fake_intent_helper_instance_.get());
    WaitForInstanceReady(arc_bridge_service->intent_helper());
#endif
  }

  void TearDown() override {
#if defined(OS_CHROMEOS)
    arc_test_.arc_service_manager()
        ->arc_bridge_service()
        ->intent_helper()
        ->CloseInstance(fake_intent_helper_instance_.get());
    fake_intent_helper_instance_.reset();
    intent_helper_bridge_.reset();
    arc_test_.TearDown();
#endif
    url_loader_.reset();
    install_task_.reset();
    install_finalizer_.reset();
    ui_manager_.reset();
    icon_manager_.reset();

    test_registry_controller_.reset();

    WebAppTest::TearDown();
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string name,
                             const std::string description,
                             const GURL& scope,
                             base::Optional<SkColor> theme_color,
                             bool open_as_window) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();

    web_app_info->start_url = url;
    web_app_info->title = base::UTF8ToUTF16(name);
    web_app_info->description = base::UTF8ToUTF16(description);
    web_app_info->scope = scope;
    web_app_info->theme_color = theme_color;
    web_app_info->open_as_window = open_as_window;

    data_retriever_->SetRendererWebApplicationInfo(std::move(web_app_info));
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string name,
                             const std::string description) {
    CreateRendererAppInfo(url, name, description, GURL(), base::nullopt,
                          /*open_as_window*/ true);
  }

  void ResetInstallTask() {
    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever_ = static_cast<TestDataRetriever*>(data_retriever.get());

    install_task_ = std::make_unique<WebAppInstallTask>(
        profile(), os_integration_manager_.get(), install_finalizer_.get(),
        std::move(data_retriever));
  }

  void SetInstallFinalizerForTesting() {
    auto test_install_finalizer = std::make_unique<TestInstallFinalizer>();
    test_install_finalizer_ = test_install_finalizer.get();
    install_finalizer_ = std::move(test_install_finalizer);
    install_task_->SetInstallFinalizerForTesting(test_install_finalizer_);
  }

  void CreateDefaultDataToRetrieve(const GURL& url, const GURL& scope) {
    DCHECK(data_retriever_);
    data_retriever_->BuildDefaultDataToRetrieve(url, scope);
  }

  void CreateDefaultDataToRetrieve(const GURL& url) {
    CreateDefaultDataToRetrieve(url, GURL{});
  }

  void CreateDataToRetrieve(const GURL& url, bool open_as_window) {
    DCHECK(data_retriever_);

    auto renderer_web_app_info = std::make_unique<WebApplicationInfo>();
    renderer_web_app_info->open_as_window = open_as_window;
    data_retriever_->SetRendererWebApplicationInfo(
        std::move(renderer_web_app_info));

    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = url;
    manifest->short_name = base::ASCIIToUTF16("Manifest Name");
    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    data_retriever_->SetIcons(IconsMap{});
  }

  TestInstallFinalizer& test_install_finalizer() {
    DCHECK(test_install_finalizer_);
    return *test_install_finalizer_;
  }

  void SetIconsMapToRetrieve(IconsMap icons_map) {
    DCHECK(data_retriever_);
    data_retriever_->SetIcons(std::move(icons_map));
  }

  struct InstallResult {
    AppId app_id;
    InstallResultCode code;
  };

  InstallResult InstallWebAppFromManifestWithFallbackAndGetResults() {
    InstallResult result;
    base::RunLoop run_loop;
    const bool force_shortcut_app = false;
    install_task_->InstallWebAppFromManifestWithFallback(
        web_contents(), force_shortcut_app,
        WebappInstallSource::MENU_BROWSER_TAB,
        base::BindOnce(TestAcceptDialogCallback),
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              result.app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  InstallResult LoadAndInstallWebAppFromManifestWithFallback(const GURL& url) {
    InstallResult result;
    base::RunLoop run_loop;
    install_task_->LoadAndInstallWebAppFromManifestWithFallback(
        url, web_contents(), &url_loader(), WebappInstallSource::SYNC,
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              result.app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  std::unique_ptr<WebApplicationInfo>
  LoadAndRetrieveWebApplicationInfoWithIcons(const GURL& url) {
    std::unique_ptr<WebApplicationInfo> result;
    base::RunLoop run_loop;
    install_task_->LoadAndRetrieveWebApplicationInfoWithIcons(
        url, &url_loader(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<WebApplicationInfo> web_app_info) {
              result = std::move(web_app_info);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  AppId InstallWebAppFromManifestWithFallback() {
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    DCHECK_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    return result.app_id;
  }

  AppId InstallWebAppWithParams(
      const WebAppInstallManager::InstallParams& params) {
    AppId app_id;
    base::RunLoop run_loop;
    install_task_->InstallWebAppWithParams(
        web_contents(), params, WebappInstallSource::EXTERNAL_DEFAULT,
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              ASSERT_EQ(InstallResultCode::kSuccessNewInstall, code);
              app_id = installed_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();
    return app_id;
  }

  void PrepareTestAppInstall() {
    const GURL url{"https://example.com/path"};
    CreateDefaultDataToRetrieve(url);
    CreateRendererAppInfo(url, "Name", "Description");

    SetInstallFinalizerForTesting();

    IconsMap icons_map;
    SetIconsMapToRetrieve(std::move(icons_map));
  }

 protected:
  WebAppInstallTask& install_task() { return *install_task_; }
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }
  TestOsIntegrationManager& test_os_integration_manager() {
    return *os_integration_manager_;
  }
  TestWebAppUrlLoader& url_loader() { return *url_loader_; }
  TestDataRetriever& data_retriever() {
    DCHECK(data_retriever_);
    return *data_retriever_;
  }

  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppInstallTask> install_task_;
  std::unique_ptr<TestWebAppUiManager> ui_manager_;
  std::unique_ptr<InstallFinalizer> install_finalizer_;
  std::unique_ptr<TestOsIntegrationManager> os_integration_manager_;

  // Owned by icon_manager_:
  TestFileUtils* file_utils_ = nullptr;
  // Owned by install_task_:
  TestDataRetriever* data_retriever_ = nullptr;

#if defined(OS_CHROMEOS)
  ArcAppTest arc_test_;
  std::unique_ptr<arc::ArcIntentHelperBridge> intent_helper_bridge_;
  std::unique_ptr<arc::FakeIntentHelperInstance> fake_intent_helper_instance_;
#endif

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<TestWebAppUrlLoader> url_loader_;
  TestInstallFinalizer* test_install_finalizer_ = nullptr;
};

class WebAppInstallTaskWithRunOnOsLoginTest : public WebAppInstallTaskTest {
 public:
  WebAppInstallTaskWithRunOnOsLoginTest() {
    scoped_feature_list_.InitWithFeatures({features::kDesktopPWAsRunOnOsLogin},
                                          {});
  }

  ~WebAppInstallTaskWithRunOnOsLoginTest() override = default;

  void CreateRendererAppInfo(const GURL& url,
                             const std::string name,
                             const std::string description,
                             const GURL& scope,
                             base::Optional<SkColor> theme_color,
                             bool open_as_window,
                             bool run_on_os_login) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();

    web_app_info->start_url = url;
    web_app_info->title = base::UTF8ToUTF16(name);
    web_app_info->description = base::UTF8ToUTF16(description);
    web_app_info->scope = scope;
    web_app_info->theme_color = theme_color;
    web_app_info->open_as_window = open_as_window;
    web_app_info->run_on_os_login = run_on_os_login;

    data_retriever_->SetRendererWebApplicationInfo(std::move(web_app_info));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebAppInstallTaskTest, InstallFromWebContents) {
  EXPECT_TRUE(AreWebAppsUserInstallable(profile()));

  const GURL url = GURL("https://example.com/scope/path");
  const std::string manifest_name = "Manifest Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xFFAABBCC;

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateRendererAppInfo(url, "Renderer Name", description, /*scope*/ GURL{},
                        theme_color,
                        /*open_as_window*/ true);
  {
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = url;
    manifest->scope = scope;
    manifest->short_name = base::ASCIIToUTF16(manifest_name);

    data_retriever().SetManifest(std::move(manifest), /*is_installable=*/true);
  }

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  const WebApp* web_app = registrar().GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(manifest_name, web_app->name());
  EXPECT_EQ(description, web_app->description());
  EXPECT_EQ(url, web_app->start_url());
  EXPECT_EQ(scope, web_app->scope());
  EXPECT_EQ(theme_color, web_app->theme_color());
}

TEST_F(WebAppInstallTaskTest, ForceReinstall) {
  const GURL url = GURL("https://example.com/path");

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Renderer Name", "Renderer Description");

  const AppId installed_web_app = InstallWebAppFromManifestWithFallback();
  EXPECT_EQ(app_id, installed_web_app);
  ResetInstallTask();

  // Force reinstall:
  CreateRendererAppInfo(url, "Renderer Name2", "Renderer Description2");
  {
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = url;
    manifest->scope = url;
    manifest->short_name = base::ASCIIToUTF16("Manifest Name2");

    data_retriever().SetManifest(std::move(manifest), /*is_installable=*/true);
  }

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& force_installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, force_installed_app_id);
            const WebApp* web_app = registrar().GetAppById(app_id);
            EXPECT_EQ(web_app->name(), "Manifest Name2");
            EXPECT_EQ(web_app->description(), "Renderer Description2");
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallTaskTest, GetWebApplicationInfoFailed) {
  // data_retriever_ with empty info means an error.

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kGetWebApplicationInfoFailed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallTaskTest, WebContentsDestroyed) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kWebContentsDestroyed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));

  // Destroy WebContents.
  DeleteContents();
  EXPECT_EQ(nullptr, web_contents());

  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallTaskTest, InstallableCheck) {
  const std::string renderer_description = "RendererDescription";
  CreateRendererAppInfo(
      GURL("https://renderer.com/path"), "RendererName", renderer_description,
      GURL("https://renderer.com/scope"), 0x00, /*open_as_window*/ true);

  const GURL manifest_start_url = GURL("https://example.com/start");
  const AppId app_id = GenerateAppIdFromURL(manifest_start_url);
  const std::string manifest_name = "Name from Manifest";
  const GURL manifest_scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> manifest_theme_color = 0xAABBCCDD;
  const base::Optional<SkColor> expected_theme_color = 0xFFBBCCDD;  // Opaque.
  const auto display_mode = DisplayMode::kMinimalUi;

  {
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->short_name = base::ASCIIToUTF16("Short Name from Manifest");
    manifest->name = base::ASCIIToUTF16(manifest_name);
    manifest->start_url = manifest_start_url;
    manifest->scope = manifest_scope;
    manifest->theme_color = manifest_theme_color;
    manifest->display = display_mode;

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);
  }

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  const WebApp* web_app = registrar().GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  // Manifest data overrides Renderer data, except |description|.
  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(manifest_name, web_app->name());
  EXPECT_EQ(manifest_start_url, web_app->start_url());
  EXPECT_EQ(renderer_description, web_app->description());
  EXPECT_EQ(manifest_scope, web_app->scope());
  EXPECT_EQ(expected_theme_color, web_app->theme_color());
  EXPECT_EQ(display_mode, web_app->display_mode());
}

TEST_F(WebAppInstallTaskTest, GetIcons) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  SetInstallFinalizerForTesting();

  const GURL icon_url = GURL("https://example.com/app.ico");
  const SkColor color = SK_ColorBLUE;

  // Generate one icon as if it was downloaded.
  IconsMap icons_map;
  AddIconToIconsMap(icon_url, icon_size::k128, color, &icons_map);

  SetIconsMapToRetrieve(std::move(icons_map));

  InstallWebAppFromManifestWithFallback();

  std::unique_ptr<WebApplicationInfo> web_app_info =
      test_install_finalizer().web_app_info();

  // Make sure that icons have been generated for all sub sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info->icon_bitmaps_any));

  // Generated icons are not considered part of the manifest icons.
  EXPECT_TRUE(web_app_info->icon_infos.empty());

  // Generated icons are not considered part of the manifest shortcut icons.
  EXPECT_TRUE(web_app_info->shortcuts_menu_item_infos.empty());
}

TEST_F(WebAppInstallTaskTest, GetIcons_NoIconsProvided) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  SetInstallFinalizerForTesting();

  IconsMap icons_map;
  SetIconsMapToRetrieve(std::move(icons_map));

  InstallWebAppFromManifestWithFallback();

  std::unique_ptr<WebApplicationInfo> web_app_info =
      test_install_finalizer().web_app_info();

  // Make sure that icons have been generated for all sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info->icon_bitmaps_any));

  // Generated icons are not considered part of the manifest icons.
  EXPECT_TRUE(web_app_info->icon_infos.empty());

  // Generated icons are not considered part of the manifest shortcut icons.
  EXPECT_TRUE(web_app_info->shortcuts_menu_item_infos.empty());
}

TEST_F(WebAppInstallTaskTest, WriteDataToDisk) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  // TestingProfile creates temp directory if TestingProfile::path_ is empty
  // (i.e. if TestingProfile::Builder::SetPath was not called by a test fixture)
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils_->DirectoryExists(manifest_resources_directory));

  const SkColor color = SK_ColorGREEN;
  const int original_icon_size_px = icon_size::k512;

  // Generate one icon as if it was fetched from renderer.
  AddGeneratedIcon(&data_retriever_->web_app_info().icon_bitmaps_any,
                   original_icon_size_px, color);

  const AppId app_id = InstallWebAppFromManifestWithFallback();

  EXPECT_TRUE(file_utils_->DirectoryExists(manifest_resources_directory));

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils_->DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils_->IsDirectoryEmpty(temp_dir));

  const base::FilePath app_dir =
      manifest_resources_directory.AppendASCII(app_id);
  EXPECT_TRUE(file_utils_->DirectoryExists(app_dir));

  const base::FilePath icons_dir = app_dir.AppendASCII("Icons");
  EXPECT_TRUE(file_utils_->DirectoryExists(icons_dir));

  std::set<int> written_sizes_px;

  base::FileEnumerator enumerator(icons_dir, true, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    EXPECT_TRUE(path.MatchesExtension(FILE_PATH_LITERAL(".png")));

    SkBitmap bitmap;
    EXPECT_TRUE(ReadBitmap(file_utils_, path, &bitmap));

    EXPECT_EQ(bitmap.width(), bitmap.height());

    const int size_px = bitmap.width();
    EXPECT_EQ(0UL, written_sizes_px.count(size_px));

    base::FilePath size_file_name;
    size_file_name =
        size_file_name.AppendASCII(base::StringPrintf("%i.png", size_px));
    EXPECT_EQ(size_file_name, path.BaseName());

    written_sizes_px.insert(size_px);

    EXPECT_EQ(color, bitmap.getColor(0, 0));
  }

  EXPECT_EQ(GetIconSizes().size() + 1UL, written_sizes_px.size());

  for (int size_px : GetIconSizes())
    written_sizes_px.erase(size_px);
  written_sizes_px.erase(original_icon_size_px);

  EXPECT_TRUE(written_sizes_px.empty());
}

TEST_F(WebAppInstallTaskTest, WriteDataToDiskFailed) {
  const GURL start_url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(start_url);
  CreateRendererAppInfo(start_url, "Name", "Description");

  IconsMap icons_map;
  AddIconToIconsMap(GURL("https://example.com/app.ico"), icon_size::k512,
                    SK_ColorBLUE, &icons_map);
  SetIconsMapToRetrieve(std::move(icons_map));

  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);

  EXPECT_TRUE(file_utils_->CreateDirectory(manifest_resources_directory));

  // Induce an error: Simulate "Disk Full" for writing icon files.
  file_utils_->SetRemainingDiskSpaceSize(1024);

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kWriteDataFailed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils_->DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils_->IsDirectoryEmpty(temp_dir));

  const AppId app_id = GenerateAppIdFromURL(start_url);
  const base::FilePath app_dir =
      manifest_resources_directory.AppendASCII(app_id);
  EXPECT_FALSE(file_utils_->DirectoryExists(app_dir));
}

TEST_F(WebAppInstallTaskTest, UserInstallDeclined) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestDeclineDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kUserInstallDeclined, code);
            EXPECT_EQ(installed_app_id, AppId());
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  const WebApp* web_app = registrar().GetAppById(app_id);
  EXPECT_EQ(nullptr, web_app);
}

TEST_F(WebAppInstallTaskTest, FinalizerMethodsCalled) {
  PrepareTestAppInstall();

  InstallWebAppFromManifestWithFallback();

  EXPECT_EQ(1u, test_os_integration_manager().num_create_shortcuts_calls());
  EXPECT_EQ(1, test_install_finalizer().num_reparent_tab_calls());

#if defined(OS_CHROMEOS)
  const size_t expected_num_add_app_to_quick_launch_bar_calls = 0;
#else
  const size_t expected_num_add_app_to_quick_launch_bar_calls = 1;
#endif

  EXPECT_EQ(
      expected_num_add_app_to_quick_launch_bar_calls,
      test_os_integration_manager().num_add_app_to_quick_launch_bar_calls());
}

TEST_F(WebAppInstallTaskTest, FinalizerMethodsNotCalled) {
  PrepareTestAppInstall();
  test_install_finalizer().SetNextFinalizeInstallResult(
      AppId(), InstallResultCode::kBookmarkExtensionInstallError);

  InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();

  EXPECT_TRUE(result.app_id.empty());
  EXPECT_EQ(InstallResultCode::kBookmarkExtensionInstallError, result.code);

  EXPECT_EQ(0u, test_os_integration_manager().num_create_shortcuts_calls());
  EXPECT_EQ(0, test_install_finalizer().num_reparent_tab_calls());
  EXPECT_EQ(
      0u,
      test_os_integration_manager().num_add_app_to_quick_launch_bar_calls());
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromManifest_Success) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = url;
  manifest->short_name = base::ASCIIToUTF16("Server Name");

  data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromInfo_Success) {
  SetInstallFinalizerForTesting();

  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = url;
  web_app_info->open_as_window = true;
  web_app_info->title = base::ASCIIToUTF16("App Name");

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes,
      WebappInstallSource::MENU_BROWSER_TAB,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);

            std::unique_ptr<WebApplicationInfo> final_web_app_info =
                test_install_finalizer().web_app_info();
            EXPECT_TRUE(final_web_app_info->open_as_window);

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromInfo_GenerateIcons) {
  SetInstallFinalizerForTesting();

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = GURL("https://example.com/path");
  web_app_info->open_as_window = false;
  web_app_info->title = base::ASCIIToUTF16("App Name");

  // Add square yellow icon.
  AddGeneratedIcon(&web_app_info->icon_bitmaps_any, icon_size::k256,
                   SK_ColorYELLOW);

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes,
      WebappInstallSource::ARC,
      base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                     InstallResultCode code) {
        std::unique_ptr<WebApplicationInfo> final_web_app_info =
            test_install_finalizer().web_app_info();

        // Make sure that icons have been generated for all sub sizes.
        EXPECT_TRUE(
            ContainsOneIconOfEachSize(final_web_app_info->icon_bitmaps_any));

        // Make sure they're all derived from the yellow icon.
        for (const std::pair<const SquareSizePx, SkBitmap>& icon :
             final_web_app_info->icon_bitmaps_any) {
          EXPECT_FALSE(icon.second.drawsNothing());
          EXPECT_EQ(SK_ColorYELLOW, icon.second.getColor(0, 0));
        }

        EXPECT_FALSE(final_web_app_info->open_as_window);

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromManifestWithFallback_NoIcons) {
  SetInstallFinalizerForTesting();

  const GURL url{"https://example.com/path"};
  CreateDefaultDataToRetrieve(url);

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), /*force_shortcut_app=*/true,
      WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                     InstallResultCode code) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);

        std::unique_ptr<WebApplicationInfo> final_web_app_info =
            test_install_finalizer().web_app_info();
        // Make sure that icons have been generated for all sub sizes.
        EXPECT_TRUE(
            ContainsOneIconOfEachSize(final_web_app_info->icon_bitmaps_any));
        for (const std::pair<const SquareSizePx, SkBitmap>& icon :
             final_web_app_info->icon_bitmaps_any) {
          EXPECT_FALSE(icon.second.drawsNothing());
        }

        EXPECT_TRUE(final_web_app_info->icon_infos.empty());

        run_loop.Quit();
      }));

  run_loop.Run();
}

#if defined(OS_CHROMEOS)
TEST_F(WebAppInstallTaskTest, IntentToPlayStore) {
  arc_test_.app_instance()->set_is_installable(true);

  const GURL url("https://example.com/scope/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;

  CreateRendererAppInfo(url, name, description, /*scope*/ GURL{}, theme_color,
                        /*open_as_window*/ true);
  {
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = url;
    manifest->scope = scope;
    blink::Manifest::RelatedApplication related_app;
    related_app.platform = base::ASCIIToUTF16("chromeos_play");
    related_app.id = base::ASCIIToUTF16("com.app.id");
    manifest->related_applications.push_back(std::move(related_app));

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    data_retriever_->SetIcons(IconsMap{});
  }

  base::RunLoop run_loop;
  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), /*force_shortcut_app=*/false,
      WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kIntentToPlayStore, code);
            EXPECT_EQ(AppId(), installed_app_id);
            run_loop.Quit();
          }));
  run_loop.Run();
}
#endif

// Default apps should be installable for guest profiles.
TEST_F(WebAppInstallTaskTest, InstallWebAppWithParams_GuestProfile) {
  SetInstallFinalizerForTesting();

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  Profile* guest_profile = profile_manager.CreateGuestProfile();

  const GURL start_url("https://example.com/path");
  auto data_retriever = std::make_unique<TestDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(start_url,
                                             /*scope=*/GURL{});

  auto install_task = std::make_unique<WebAppInstallTask>(
      guest_profile, os_integration_manager_.get(), install_finalizer_.get(),
      std::move(data_retriever));

  base::RunLoop run_loop;
  install_task->InstallWebAppWithParams(
      web_contents(), MakeParams(), WebappInstallSource::EXTERNAL_DEFAULT,
      base::BindLambdaForTesting(
          [&](const AppId& app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppWithParams_DisplayMode) {
  {
    CreateDataToRetrieve(GURL("https://example.com/"),
                         /*open_as_window*/ false);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kUndefined));

    EXPECT_EQ(DisplayMode::kBrowser,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  ResetInstallTask();
  {
    CreateDataToRetrieve(GURL("https://example.org/"), /*open_as_window*/ true);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kUndefined));

    EXPECT_EQ(DisplayMode::kStandalone,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  ResetInstallTask();
  {
    CreateDataToRetrieve(GURL("https://example.au/"), /*open_as_window*/ true);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kBrowser));

    EXPECT_EQ(DisplayMode::kBrowser,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  ResetInstallTask();
  {
    CreateDataToRetrieve(GURL("https://example.app/"),
                         /*open_as_window*/ false);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kStandalone));

    EXPECT_EQ(DisplayMode::kStandalone,
              registrar().GetAppById(app_id)->user_display_mode());
  }
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromManifest_ExpectAppId) {
  const auto url1 = GURL("https://example.com/");
  const auto url2 = GURL("https://example.org/");
  const AppId app_id1 = GenerateAppIdFromURL(url1);
  const AppId app_id2 = GenerateAppIdFromURL(url2);
  ASSERT_NE(app_id1, app_id2);
  {
    CreateDefaultDataToRetrieve(url1);
    install_task().ExpectAppId(app_id1);
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id1, result.app_id);
    EXPECT_TRUE(registrar().GetAppById(app_id1));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url2);
    install_task().ExpectAppId(app_id1);
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    EXPECT_EQ(InstallResultCode::kExpectedAppIdCheckFailed, result.code);
    EXPECT_EQ(app_id2, result.app_id);
    EXPECT_FALSE(registrar().GetAppById(app_id2));
  }
}

TEST_F(WebAppInstallTaskTest, LoadAndInstallWebAppFromManifestWithFallback) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(InstallResultCode::kInstallURLRedirected, result.code);
    EXPECT_TRUE(result.app_id.empty());
    EXPECT_FALSE(registrar().GetAppById(app_id));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kFailedPageTookTooLong);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(InstallResultCode::kInstallURLLoadTimeOut, result.code);
    EXPECT_TRUE(result.app_id.empty());
    EXPECT_FALSE(registrar().GetAppById(app_id));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
    EXPECT_TRUE(registrar().GetAppById(app_id));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
    EXPECT_TRUE(registrar().GetAppById(app_id));
  }
}

TEST_F(WebAppInstallTaskTest, LoadAndRetrieveWebApplicationInfoWithIcons) {
  const GURL url = GURL("https://example.com/path");
  const GURL start_url = GURL("https://example.com/start");
  const std::string name = "Name";
  const std::string description = "Description";
  const AppId app_id = GenerateAppIdFromURL(url);
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    std::unique_ptr<WebApplicationInfo> result =
        LoadAndRetrieveWebApplicationInfoWithIcons(url);
    EXPECT_FALSE(result);
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kFailedPageTookTooLong);

    std::unique_ptr<WebApplicationInfo> result =
        LoadAndRetrieveWebApplicationInfoWithIcons(url);
    EXPECT_FALSE(result);
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(start_url);
    CreateRendererAppInfo(url, name, description);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    std::unique_ptr<WebApplicationInfo> result =
        LoadAndRetrieveWebApplicationInfoWithIcons(url);
    EXPECT_TRUE(result);
    EXPECT_EQ(result->start_url, start_url);
    EXPECT_TRUE(result->icon_infos.empty());
    EXPECT_FALSE(result->icon_bitmaps_any.empty());
  }
  ResetInstallTask();
  {
    // Verify the callback is always called.
    base::RunLoop run_loop;
    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, GURL{});
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    auto task = std::make_unique<WebAppInstallTask>(
        profile(), os_integration_manager_.get(), install_finalizer_.get(),
        std::move(data_retriever));

    std::unique_ptr<WebApplicationInfo> info;
    task->LoadAndRetrieveWebApplicationInfoWithIcons(
        url, &url_loader(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<WebApplicationInfo> app_info) {
              info = std::move(app_info);
              run_loop.Quit();
            }));
    task.reset();
    run_loop.Run();
  }
}

TEST_F(WebAppInstallTaskWithRunOnOsLoginTest,
       InstallFromWebContentsRunOnOsLogin) {
  EXPECT_TRUE(AreWebAppsUserInstallable(profile()));

  const GURL url = GURL("https://example.com/scope/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xFFAABBCC;

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateDefaultDataToRetrieve(url, scope);
  CreateRendererAppInfo(url, name, description, /*scope=*/GURL{}, theme_color,
                        /*open_as_window=*/true, /*run_on_os_login=*/true);

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  const WebApp* web_app = registrar().GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(description, web_app->description());
  EXPECT_EQ(url, web_app->start_url());
  EXPECT_EQ(scope, web_app->scope());
  EXPECT_EQ(theme_color, web_app->theme_color());
  EXPECT_EQ(1u,
            test_os_integration_manager().num_register_run_on_os_login_calls());
}

TEST_F(WebAppInstallTaskWithRunOnOsLoginTest,
       InstallFromWebContentsNoRunOnOsLogin) {
  EXPECT_TRUE(AreWebAppsUserInstallable(profile()));

  const GURL url = GURL("https://example.com/scope/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xFFAABBCC;

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateDefaultDataToRetrieve(url, scope);
  CreateRendererAppInfo(url, name, description, /*scope=*/GURL{}, theme_color,
                        /*open_as_window=*/true, /*run_on_os_login=*/false);

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  const WebApp* web_app = registrar().GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(description, web_app->description());
  EXPECT_EQ(url, web_app->start_url());
  EXPECT_EQ(scope, web_app->scope());
  EXPECT_EQ(theme_color, web_app->theme_color());
  EXPECT_EQ(0u,
            test_os_integration_manager().num_register_run_on_os_login_calls());
}

// TODO(https://crbug.com/1096953): Move these tests out into a dedicated
// unittest file.
class WebAppInstallTaskTestWithShortcutsMenu : public WebAppInstallTaskTest {
 public:
  WebAppInstallTaskTestWithShortcutsMenu() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsAppIconShortcutsMenu}, {});
  }

  GURL ShortcutIconUrl() {
    return GURL("https://example.com/icons/shortcut_icon.png");
  }

  GURL ShortcutItemUrl() { return GURL("https://example.com/path/item"); }

  // Installs the app and validates |final_web_app_info| matches the args passed
  // in.
  InstallResult InstallWebAppWithShortcutsMenuValidateAndGetResults(
      GURL start_url,
      SkColor theme_color,
      std::string shortcut_name,
      GURL shortcut_url,
      SquareSizePx icon_size,
      GURL icon_src) {
    InstallResult result;
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = start_url;
    manifest->theme_color = theme_color;
    manifest->name = base::ASCIIToUTF16("Manifest Name");

    // Add shortcuts to manifest.
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = shortcut_url;
    blink::Manifest::ImageResource icon;
    icon.src = icon_src;
    icon.sizes.emplace_back(gfx::Size(icon_size, icon_size));
    shortcut_item.icons.emplace_back(icon);
    manifest->shortcuts.emplace_back(shortcut_item);

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    base::RunLoop run_loop;
    bool callback_called = false;

    SetInstallFinalizerForTesting();

    install_task_->InstallWebAppFromManifest(
        web_contents(), /*bypass_service_worker_check=*/false,
        WebappInstallSource::MENU_BROWSER_TAB,
        base::BindOnce(TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       InstallResultCode code) {
          result.app_id = installed_app_id;
          result.code = code;
          std::unique_ptr<WebApplicationInfo> final_web_app_info =
              test_install_finalizer().web_app_info();
          EXPECT_EQ(theme_color, final_web_app_info->theme_color);
          EXPECT_EQ(1u, final_web_app_info->shortcuts_menu_item_infos.size());
          EXPECT_EQ(base::UTF8ToUTF16(shortcut_name),
                    final_web_app_info->shortcuts_menu_item_infos[0].name);
          EXPECT_EQ(shortcut_url,
                    final_web_app_info->shortcuts_menu_item_infos[0].url);
          EXPECT_EQ(1u, final_web_app_info->shortcuts_menu_item_infos[0]
                            .shortcut_icon_infos.size());
          EXPECT_EQ(icon_size, final_web_app_info->shortcuts_menu_item_infos[0]
                                   .shortcut_icon_infos[0]
                                   .square_size_px);
          EXPECT_EQ(icon_src, final_web_app_info->shortcuts_menu_item_infos[0]
                                  .shortcut_icon_infos[0]
                                  .url);

          callback_called = true;
          run_loop.Quit();
        }));

    run_loop.Run();

    EXPECT_TRUE(callback_called);

    return result;
  }

  // Updates the app and validates |final_web_app_info| matches the args passed
  // in.
  InstallResult UpdateWebAppWithShortcutsMenuValidateAndGetResults(
      GURL url,
      SkColor theme_color,
      std::string shortcut_name,
      GURL shortcut_url,
      SquareSizePx icon_size,
      GURL icon_src) {
    InstallResult result;
    const AppId app_id = GenerateAppIdFromURL(url);

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = url;
    web_app_info->open_as_window = true;
    web_app_info->theme_color = theme_color;
    web_app_info->title = base::ASCIIToUTF16("App Name");

    WebApplicationShortcutsMenuItemInfo shortcut_item;
    WebApplicationShortcutsMenuItemInfo::Icon icon;
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = shortcut_url;

    icon.url = icon_src;
    icon.square_size_px = icon_size;
    shortcut_item.shortcut_icon_infos.emplace_back(std::move(icon));
    web_app_info->shortcuts_menu_item_infos.emplace_back(
        std::move(shortcut_item));

    base::RunLoop run_loop;
    bool callback_called = false;

    SetInstallFinalizerForTesting();

    install_task_->UpdateWebAppFromInfo(
        web_contents(), app_id, std::move(web_app_info),
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       InstallResultCode code) {
          result.app_id = installed_app_id;
          result.code = code;
          std::unique_ptr<WebApplicationInfo> final_web_app_info =
              test_install_finalizer().web_app_info();
          EXPECT_EQ(theme_color, final_web_app_info->theme_color);
          EXPECT_EQ(1u, final_web_app_info->shortcuts_menu_item_infos.size());
          EXPECT_EQ(base::UTF8ToUTF16(shortcut_name),
                    final_web_app_info->shortcuts_menu_item_infos[0].name);
          EXPECT_EQ(shortcut_url,
                    final_web_app_info->shortcuts_menu_item_infos[0].url);
          EXPECT_EQ(1u, final_web_app_info->shortcuts_menu_item_infos[0]
                            .shortcut_icon_infos.size());
          EXPECT_EQ(icon_size, final_web_app_info->shortcuts_menu_item_infos[0]
                                   .shortcut_icon_infos[0]
                                   .square_size_px);
          EXPECT_EQ(icon_src, final_web_app_info->shortcuts_menu_item_infos[0]
                                  .shortcut_icon_infos[0]
                                  .url);

          callback_called = true;
          run_loop.Quit();
        }));

    run_loop.Run();

    EXPECT_TRUE(callback_called);

    return result;
  }

  static constexpr char kShortcutItemName[] = "shortcut item";
  static constexpr SquareSizePx kIconSize = 128;
  static constexpr SkColor kInitialThemeColor = 0xFF000000;
  static constexpr SkColor kFinalThemeColor = 0xFFFFFFFF;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Declare constants needed by tests.
constexpr char WebAppInstallTaskTestWithShortcutsMenu::kShortcutItemName[];
constexpr SquareSizePx WebAppInstallTaskTestWithShortcutsMenu::kIconSize;
constexpr SkColor WebAppInstallTaskTestWithShortcutsMenu::kInitialThemeColor;
constexpr SkColor WebAppInstallTaskTestWithShortcutsMenu::kFinalThemeColor;

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       InstallWebAppFromManifest_Success) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  InstallResult result = InstallWebAppWithShortcutsMenuValidateAndGetResults(
      url, kInitialThemeColor, "shortcut",
      GURL("https://example.com/path/page"), kIconSize,
      GURL("https://example.com/icons/shortcut.png"));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);
}

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       UpdateWebAppFromInfo_AddShortcutsMenu) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  // Install the app without a shortcuts menu.
  {
    CreateDefaultDataToRetrieve(url);
    install_task().ExpectAppId(app_id);
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
  ResetInstallTask();

  // Update the installed app, adding a Shortcuts Menu in the process.
  {
    InstallResult result = UpdateWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, "shortcut",
        GURL("https://example.com/path/page"), kIconSize, ShortcutIconUrl());
    EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
}

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       UpdateWebAppFromInfo_UpdateShortcutsMenu) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  // Install the app.
  {
    InstallResult result = InstallWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, "shortcut",
        GURL("https://example.com/path/page"), 2 * kIconSize,
        GURL("https://example.com/icons/shortcut.png"));
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
  ResetInstallTask();

  // Update the installed app, Shortcuts Menu has changed.
  {
    InstallResult result = UpdateWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, kShortcutItemName, ShortcutItemUrl(),
        kIconSize, ShortcutIconUrl());
    EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
}

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       UpdateWebAppFromInfo_ShortcutsMenuNotChanged) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  // Install the app.
  {
    InstallResult result = InstallWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, kShortcutItemName, ShortcutItemUrl(),
        kIconSize, ShortcutIconUrl());
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
  ResetInstallTask();

  // Update the installed app. Only theme color changed, so Shortcuts Menu
  // should stay the same.
  {
    InstallResult result = UpdateWebAppWithShortcutsMenuValidateAndGetResults(
        url, kFinalThemeColor, kShortcutItemName, ShortcutItemUrl(), kIconSize,
        ShortcutIconUrl());
    EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
}

}  // namespace web_app
