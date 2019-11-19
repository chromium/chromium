// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_task.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_app_shortcut_manager.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
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
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
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

bool ReadBitmap(FileUtilsWrapper* utils,
                const base::FilePath& file_path,
                SkBitmap* bitmap) {
  std::string icon_data;
  if (!utils->ReadFileToString(file_path, &icon_data))
    return false;

  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(icon_data.c_str()),
      icon_data.size(), bitmap);
}

constexpr int kIconSizes[] = {
    icon_size::k32, icon_size::k64,  icon_size::k48,
    icon_size::k96, icon_size::k128, icon_size::k256,
};

bool ContainsOneIconOfEachSize(const WebApplicationInfo& web_app_info) {
  for (int size_px : kIconSizes) {
    int num_icons_for_size =
        std::count_if(web_app_info.icons.begin(), web_app_info.icons.end(),
                      [&size_px](const WebApplicationIconInfo& icon) {
                        return icon.width == size_px && icon.height == size_px;
                      });
    if (num_icons_for_size != 1)
      return false;
  }

  return true;
}

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback), true /*accept*/,
                                std::move(web_app_info)));
}

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback),
                                false /*accept*/, std::move(web_app_info)));
}

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

    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        &controller().sync_bridge(), icon_manager_.get());
    shortcut_manager_ = std::make_unique<TestAppShortcutManager>(profile());

    install_finalizer_->SetSubsystems(&registrar(), ui_manager_.get());
    shortcut_manager_->SetSubsystems(&registrar());

    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever_ = data_retriever.get();

    install_task_ = std::make_unique<WebAppInstallTask>(
        profile(), shortcut_manager_.get(), install_finalizer_.get(),
        std::move(data_retriever));

    url_loader_ = std::make_unique<TestWebAppUrlLoader>();
    controller().Init();

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
    shortcut_manager_.reset();

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

    web_app_info->app_url = url;
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

  static base::NullableString16 ToNullableUTF16(const std::string& str) {
    return base::NullableString16(base::UTF8ToUTF16(str), false);
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

  AppId InstallWebAppFromInfoRetrieveIcons(const GURL& url,
                                           bool is_locally_installed) {
    AppId app_id;
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = url;

    base::RunLoop run_loop;
    install_task_->InstallWebAppFromInfoRetrieveIcons(
        web_contents(), std::move(web_app_info), is_locally_installed,
        WebappInstallSource::SYNC,
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
  TestAppShortcutManager& test_shortcut_manager() { return *shortcut_manager_; }
  TestWebAppUrlLoader& url_loader() { return *url_loader_; }

  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppInstallTask> install_task_;
  std::unique_ptr<TestWebAppUiManager> ui_manager_;
  std::unique_ptr<InstallFinalizer> install_finalizer_;
  std::unique_ptr<TestAppShortcutManager> shortcut_manager_;

  // Owned by install_task_:
  TestFileUtils* file_utils_ = nullptr;
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

TEST_F(WebAppInstallTaskTest, InstallFromWebContents) {
  EXPECT_TRUE(AreWebAppsUserInstallable(profile()));

  const GURL url = GURL("https://example.com/scope/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;
  const base::Optional<SkColor> expected_theme_color = 0xFFBBCCDD;  // Opaque.

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateDefaultDataToRetrieve(url, scope);
  CreateRendererAppInfo(url, name, description, /*scope*/ GURL{}, theme_color,
                        /*open_as_window*/ true);

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
  EXPECT_EQ(name, web_app->name());
  EXPECT_EQ(description, web_app->description());
  EXPECT_EQ(url, web_app->launch_url());
  EXPECT_EQ(scope, web_app->scope());
  EXPECT_EQ(expected_theme_color, web_app->theme_color());
}

TEST_F(WebAppInstallTaskTest, AlreadyInstalled) {
  const GURL url = GURL("https://example.com/path");
  const std::string name = "Name";
  const std::string description = "Description";

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, name, description);

  const AppId installed_web_app = InstallWebAppFromManifestWithFallback();
  EXPECT_EQ(app_id, installed_web_app);

  // Second attempt.
  CreateRendererAppInfo(url, name, description);

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& already_installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, code);
            EXPECT_EQ(app_id, already_installed_app_id);
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
    manifest->short_name = ToNullableUTF16("Short Name from Manifest");
    manifest->name = ToNullableUTF16(manifest_name);
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
  EXPECT_EQ(manifest_start_url, web_app->launch_url());
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
  EXPECT_TRUE(ContainsOneIconOfEachSize(*web_app_info));

  for (const WebApplicationIconInfo& icon : web_app_info->icons) {
    EXPECT_FALSE(icon.data.drawsNothing());
    EXPECT_EQ(color, icon.data.getColor(0, 0));

    // All icons should have an empty url except the original one:
    if (icon.url != icon_url) {
      EXPECT_EQ(GURL(), icon.url);
    }
  }
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
  EXPECT_TRUE(ContainsOneIconOfEachSize(*web_app_info));

  for (const WebApplicationIconInfo& icon : web_app_info->icons) {
    EXPECT_FALSE(icon.data.drawsNothing());
    // Since all icons are generated, they should have an empty url.
    EXPECT_TRUE(icon.url.is_empty());
  }
}

TEST_F(WebAppInstallTaskTest, WriteDataToDisk) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  // TestingProfile creates temp directory if TestingProfile::path_ is empty
  // (i.e. if TestingProfile::Builder::SetPath was not called by a test fixture)
  const base::FilePath profile_dir = profile()->GetPath();
  const base::FilePath web_apps_dir = profile_dir.AppendASCII("WebApps");
  EXPECT_FALSE(file_utils_->DirectoryExists(web_apps_dir));

  const SkColor color = SK_ColorGREEN;
  const int original_icon_size_px = icon_size::k512;

  // Generate one icon as if it was fetched from renderer.
  {
    WebApplicationIconInfo icon_info = GenerateIconInfo(
        GURL("https://example.com/app.ico"), original_icon_size_px, color);
    data_retriever_->web_app_info().icons.push_back(std::move(icon_info));
  }

  const AppId app_id = InstallWebAppFromManifestWithFallback();

  EXPECT_TRUE(file_utils_->DirectoryExists(web_apps_dir));

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils_->DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils_->IsDirectoryEmpty(temp_dir));

  const base::FilePath app_dir = web_apps_dir.AppendASCII(app_id);
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

  EXPECT_EQ(base::size(kIconSizes) + 1UL, written_sizes_px.size());

  for (int size_px : kIconSizes)
    written_sizes_px.erase(size_px);
  written_sizes_px.erase(original_icon_size_px);

  EXPECT_TRUE(written_sizes_px.empty());
}

TEST_F(WebAppInstallTaskTest, WriteDataToDiskFailed) {
  const GURL app_url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(app_url);
  CreateRendererAppInfo(app_url, "Name", "Description");

  IconsMap icons_map;
  AddIconToIconsMap(GURL("https://example.com/app.ico"), icon_size::k512,
                    SK_ColorBLUE, &icons_map);
  SetIconsMapToRetrieve(std::move(icons_map));

  const base::FilePath profile_dir = profile()->GetPath();
  const base::FilePath web_apps_dir = profile_dir.AppendASCII("WebApps");

  EXPECT_TRUE(file_utils_->CreateDirectory(web_apps_dir));

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

  const AppId app_id = GenerateAppIdFromURL(app_url);
  const base::FilePath app_dir = web_apps_dir.AppendASCII(app_id);
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

  EXPECT_EQ(1u, test_shortcut_manager().num_create_shortcuts_calls());
  EXPECT_EQ(1, test_install_finalizer().num_reparent_tab_calls());
  EXPECT_EQ(1, test_install_finalizer().num_reveal_appshim_calls());
  EXPECT_EQ(1,
            test_install_finalizer().num_add_app_to_quick_launch_bar_calls());
}

TEST_F(WebAppInstallTaskTest, FinalizerMethodsNotCalled) {
  PrepareTestAppInstall();
  test_install_finalizer().SetNextFinalizeInstallResult(
      AppId(), InstallResultCode::kFailedUnknownReason);

  InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();

  EXPECT_TRUE(result.app_id.empty());
  EXPECT_EQ(InstallResultCode::kFailedUnknownReason, result.code);

  EXPECT_EQ(0u, test_shortcut_manager().num_create_shortcuts_calls());
  EXPECT_EQ(0, test_install_finalizer().num_reparent_tab_calls());
  EXPECT_EQ(0, test_install_finalizer().num_reveal_appshim_calls());
  EXPECT_EQ(0,
            test_install_finalizer().num_add_app_to_quick_launch_bar_calls());
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromManifest_Success) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(url);

  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = url;

  data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromManifest(
      web_contents(), WebappInstallSource::MENU_BROWSER_TAB,
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
  web_app_info->app_url = url;
  web_app_info->open_as_window = true;

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes,
      WebappInstallSource::MENU_BROWSER_TAB,
      base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                     InstallResultCode code) {
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
  web_app_info->app_url = GURL("https://example.com/path");
  web_app_info->open_as_window = false;

  const GURL icon_url("https://example.com/path/main.ico");

  // Add square yellow icon.
  {
    auto icon_info =
        GenerateIconInfo(icon_url, icon_size::k256, SK_ColorYELLOW);
    web_app_info->icons.push_back(std::move(icon_info));
  }

  // Add non-square red icon.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(icon_size::k256, icon_size::k128);
    bitmap.eraseColor(SK_ColorRED);

    WebApplicationIconInfo icon_info;
    icon_info.url = GURL("https://example.com/path/bad.ico");
    icon_info.width = icon_size::k256;
    icon_info.height = icon_size::k128;
    icon_info.data = bitmap;
    web_app_info->icons.push_back(std::move(icon_info));
  }

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes,
      WebappInstallSource::ARC,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            std::unique_ptr<WebApplicationInfo> final_web_app_info =
                test_install_finalizer().web_app_info();

            // Make sure that icons have been generated for all sub sizes.
            EXPECT_TRUE(ContainsOneIconOfEachSize(*final_web_app_info));

            // Make sure no red non-square icons, only square yellow ones.
            for (const WebApplicationIconInfo& icon :
                 final_web_app_info->icons) {
              EXPECT_FALSE(icon.data.drawsNothing());
              EXPECT_EQ(SK_ColorYELLOW, icon.data.getColor(0, 0));

              // All icons should have an empty url except the original one:
              if (icon.url != icon_url)
                EXPECT_TRUE(icon.url.is_empty());
            }

            EXPECT_FALSE(final_web_app_info->open_as_window);

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromInfoRetrieveIcons_TwoIcons) {
  SetInstallFinalizerForTesting();

  const GURL url{"https://example.com/path"};
  const GURL icon1_url{"https://example.com/path/icon1.png"};
  const GURL icon2_url{"https://example.com/path/icon2.png"};

  CreateDefaultDataToRetrieve(url);

  const AppId app_id = GenerateAppIdFromURL(url);

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = url;

  WebApplicationIconInfo icon1_info;
  icon1_info.url = icon1_url;
  icon1_info.width = icon_size::k128;
  web_app_info->icons.push_back(std::move(icon1_info));

  WebApplicationIconInfo icon2_info;
  icon2_info.url = icon2_url;
  icon2_info.width = icon_size::k256;
  web_app_info->icons.push_back(std::move(icon2_info));

  IconsMap icons_map;
  AddIconToIconsMap(icon1_url, icon_size::k128, SK_ColorBLUE, &icons_map);
  AddIconToIconsMap(icon2_url, icon_size::k256, SK_ColorRED, &icons_map);

  SetIconsMapToRetrieve(std::move(icons_map));

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfoRetrieveIcons(
      web_contents(), std::move(web_app_info), /*is_locally_installed=*/true,
      WebappInstallSource::SYNC,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);

            EXPECT_TRUE(test_install_finalizer()
                            .finalize_options_list()
                            .at(0)
                            .locally_installed);

            std::unique_ptr<WebApplicationInfo> final_web_app_info =
                test_install_finalizer().web_app_info();

            EXPECT_EQ(2U, final_web_app_info->icons.size());

            const auto& icon1 = final_web_app_info->icons.at(0);
            EXPECT_FALSE(icon1.data.drawsNothing());
            EXPECT_EQ(SK_ColorBLUE, icon1.data.getColor(0, 0));
            EXPECT_EQ(icon1_url, icon1.url);

            const auto& icon2 = final_web_app_info->icons.at(1);
            EXPECT_FALSE(icon2.data.drawsNothing());
            EXPECT_EQ(SK_ColorRED, icon2.data.getColor(0, 0));
            EXPECT_EQ(icon2_url, icon2.url);

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromInfoRetrieveIcons_NoIcons) {
  SetInstallFinalizerForTesting();

  const GURL url{"https://example.com/path"};
  CreateDefaultDataToRetrieve(url);
  const AppId app_id = GenerateAppIdFromURL(url);

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = url;
  // All icons will get the E letter drawn into a rounded yellow background.
  web_app_info->generated_icon_color = SK_ColorYELLOW;

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfoRetrieveIcons(
      web_contents(), std::move(web_app_info), /*is_locally_installed=*/false,
      WebappInstallSource::SYNC,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);

            EXPECT_FALSE(test_install_finalizer()
                             .finalize_options_list()
                             .at(0)
                             .locally_installed);

            std::unique_ptr<WebApplicationInfo> final_web_app_info =
                test_install_finalizer().web_app_info();
            // Make sure that icons have been generated for all sub sizes.
            EXPECT_TRUE(ContainsOneIconOfEachSize(*final_web_app_info));

            for (const WebApplicationIconInfo& icon :
                 final_web_app_info->icons) {
              EXPECT_FALSE(icon.data.drawsNothing());
              EXPECT_TRUE(icon.url.is_empty());
            }

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
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);

            std::unique_ptr<WebApplicationInfo> final_web_app_info =
                test_install_finalizer().web_app_info();
            // Make sure that icons have been generated for all sub sizes.
            EXPECT_TRUE(ContainsOneIconOfEachSize(*final_web_app_info));

            for (const WebApplicationIconInfo& icon :
                 final_web_app_info->icons) {
              EXPECT_FALSE(icon.data.drawsNothing());
              EXPECT_TRUE(icon.url.is_empty());
            }

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
    related_app.platform =
        base::NullableString16(base::ASCIIToUTF16("chromeos_play"));
    related_app.id = base::NullableString16(base::ASCIIToUTF16("com.app.id"));
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

  const GURL app_url("https://example.com/path");
  auto data_retriever = std::make_unique<TestDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(app_url,
                                             /*scope=*/GURL{});

  auto install_task = std::make_unique<WebAppInstallTask>(
      guest_profile, shortcut_manager_.get(), install_finalizer_.get(),
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
  {
    CreateDataToRetrieve(GURL("https://example.org/"), /*open_as_window*/ true);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kUndefined));

    EXPECT_EQ(DisplayMode::kStandalone,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  {
    CreateDataToRetrieve(GURL("https://example.au/"), /*open_as_window*/ true);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kBrowser));

    EXPECT_EQ(DisplayMode::kBrowser,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  {
    CreateDataToRetrieve(GURL("https://example.app/"),
                         /*open_as_window*/ false);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kStandalone));

    EXPECT_EQ(DisplayMode::kStandalone,
              registrar().GetAppById(app_id)->user_display_mode());
  }
}

TEST_F(WebAppInstallTaskTest,
       InstallWebAppFromInfoRetrieveIcons_LocallyInstallled) {
  {
    const auto url = GURL("https://example.com/");
    CreateDataToRetrieve(url, /*open_as_window*/ false);
    auto app_id =
        InstallWebAppFromInfoRetrieveIcons(url, /*is_locally_installed*/ false);
    EXPECT_FALSE(registrar().GetAppById(app_id)->is_locally_installed());
  }
  {
    const auto url = GURL("https://example.org/");
    CreateDataToRetrieve(url, /*open_as_window*/ false);
    auto app_id =
        InstallWebAppFromInfoRetrieveIcons(url, /*is_locally_installed*/ true);
    EXPECT_TRUE(registrar().GetAppById(app_id)->is_locally_installed());
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
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kFailedPageTookTooLong);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(InstallResultCode::kInstallURLLoadFailed, result.code);
    EXPECT_TRUE(result.app_id.empty());
    EXPECT_FALSE(registrar().GetAppById(app_id));
  }
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
    EXPECT_TRUE(registrar().GetAppById(app_id));
  }
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(InstallResultCode::kSuccessAlreadyInstalled, result.code);
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
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kFailedPageTookTooLong);

    std::unique_ptr<WebApplicationInfo> result =
        LoadAndRetrieveWebApplicationInfoWithIcons(url);
    EXPECT_FALSE(result);
  }
  {
    CreateDefaultDataToRetrieve(start_url);
    CreateRendererAppInfo(url, name, description);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    std::unique_ptr<WebApplicationInfo> result =
        LoadAndRetrieveWebApplicationInfoWithIcons(url);
    EXPECT_TRUE(result);
    EXPECT_EQ(result->app_url, start_url);
    EXPECT_TRUE(result->icons.size() > 0);
  }
  {
    // Verify the callback is always called.
    base::RunLoop run_loop;
    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, GURL{});
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    auto task = std::make_unique<WebAppInstallTask>(
        profile(), shortcut_manager_.get(), install_finalizer_.get(),
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

}  // namespace web_app
