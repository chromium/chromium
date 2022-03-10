// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_task.h"

#include <array>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_install_finalizer.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#endif

namespace web_app {

namespace {

WebAppInstallParams MakeParams(
    DisplayMode display_mode = DisplayMode::kUndefined) {
  WebAppInstallParams params;
  params.fallback_start_url = GURL("https://example.com/fallback");
  params.user_display_mode = display_mode;
  return params;
}

}  // namespace

class WebAppInstallTaskTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());

    install_manager_ = std::make_unique<WebAppInstallManager>(profile());

    file_utils_ = base::MakeRefCounted<TestFileUtils>();
    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), file_utils_);

    policy_manager_ = std::make_unique<WebAppPolicyManager>(profile());

    ui_manager_ = std::make_unique<FakeWebAppUiManager>();

    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(profile());

    icon_manager_->SetSubsystems(&registrar(), &install_manager());

    install_finalizer_->SetSubsystems(
        &install_manager(), &registrar(), ui_manager_.get(),
        &fake_registry_controller_->sync_bridge(),
        &fake_os_integration_manager(), icon_manager_.get(),
        policy_manager_.get(),
        &fake_registry_controller_->translation_manager());

    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever_ = data_retriever.get();

    install_task_ = std::make_unique<WebAppInstallTask>(
        profile(), &install_manager(), install_finalizer_.get(),
        std::move(data_retriever), &registrar());

    url_loader_ = std::make_unique<TestWebAppUrlLoader>();
    controller().Init();
    install_finalizer_->Start();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc_test_.SetUp(profile());

    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();
    fake_intent_helper_host_ = std::make_unique<arc::FakeIntentHelperHost>(
        arc_bridge_service->intent_helper());
    fake_intent_helper_instance_ =
        std::make_unique<arc::FakeIntentHelperInstance>();
    arc_bridge_service->intent_helper()->SetInstance(
        fake_intent_helper_instance_.get());
    WaitForInstanceReady(arc_bridge_service->intent_helper());
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc_test_.arc_service_manager()
        ->arc_bridge_service()
        ->intent_helper()
        ->CloseInstance(fake_intent_helper_instance_.get());
    fake_intent_helper_instance_.reset();
    fake_intent_helper_host_.reset();
    arc_test_.TearDown();
#endif
    url_loader_.reset();
    install_task_.reset();
    install_finalizer_.reset();
    ui_manager_.reset();
    policy_manager_.reset();
    icon_manager_.reset();

    fake_registry_controller_.reset();
    install_manager_.reset();

    WebAppTest::TearDown();
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string& name,
                             const std::string& description,
                             const GURL& scope,
                             absl::optional<SkColor> theme_color,
                             DisplayMode user_display_mode) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();

    web_app_info->start_url = url;
    web_app_info->title = base::UTF8ToUTF16(name);
    web_app_info->description = base::UTF8ToUTF16(description);
    web_app_info->scope = scope;
    web_app_info->theme_color = theme_color;
    web_app_info->user_display_mode = user_display_mode;

    data_retriever_->SetRendererWebAppInstallInfo(std::move(web_app_info));
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string& name,
                             const std::string& description) {
    CreateRendererAppInfo(url, name, description, GURL(), absl::nullopt,
                          /*user_display_mode=*/DisplayMode::kStandalone);
  }

  void ResetInstallTask() {
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever_ = static_cast<FakeDataRetriever*>(data_retriever.get());

    install_task_ = std::make_unique<WebAppInstallTask>(
        profile(), &install_manager(), install_finalizer_.get(),
        std::move(data_retriever), &registrar());
  }

  void SetInstallFinalizerForTesting() {
    auto fake_install_finalizer = std::make_unique<FakeInstallFinalizer>();
    fake_install_finalizer_ = fake_install_finalizer.get();
    install_finalizer_ = std::move(fake_install_finalizer);
    install_task_->SetInstallFinalizerForTesting(fake_install_finalizer_);
  }

  void CreateDefaultDataToRetrieve(const GURL& url, const GURL& scope) {
    DCHECK(data_retriever_);
    data_retriever_->BuildDefaultDataToRetrieve(url, scope);
  }

  void CreateDefaultDataToRetrieve(const GURL& url) {
    CreateDefaultDataToRetrieve(url, GURL{});
  }

  void CreateDataToRetrieve(const GURL& url, DisplayMode user_display_mode) {
    DCHECK(data_retriever_);

    auto renderer_web_app_info = std::make_unique<WebAppInstallInfo>();
    renderer_web_app_info->user_display_mode = user_display_mode;
    data_retriever_->SetRendererWebAppInstallInfo(
        std::move(renderer_web_app_info));

    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = url;
    manifest->short_name = u"Manifest Name";
    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    data_retriever_->SetIcons(IconsMap{});
  }

  FakeInstallFinalizer& fake_install_finalizer() {
    DCHECK(fake_install_finalizer_);
    return *fake_install_finalizer_;
  }

  // Sets IconsMap, IconsDownloadedResult and corresponding `http_status_codes`
  // to populate DownloadedIconsHttpResults.
  void SetIconsMapToRetrieve(IconsMap icons_map,
                             IconsDownloadedResult result,
                             const std::vector<int>& http_status_codes) {
    DCHECK_EQ(icons_map.size(), http_status_codes.size());
    DCHECK(data_retriever_);

    data_retriever_->SetIconsDownloadedResult(result);

    int icon_index = 0;
    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = http_status_codes[icon_index];
      ++icon_index;
    }
    data_retriever_->SetDownloadedIconsHttpResults(std::move(http_results));

    // Moves `icons_map` last.
    data_retriever_->SetIcons(std::move(icons_map));
  }

  struct InstallResult {
    AppId app_id;
    webapps::InstallResultCode code;
    OsHooksErrors os_hooks_errors;
  };

  InstallResult InstallWebAppFromManifestWithFallbackAndGetResults() {
    InstallResult result;
    base::RunLoop run_loop;
    const bool force_shortcut_app = false;
    install_task_->InstallWebAppFromManifestWithFallback(
        web_contents(), force_shortcut_app,
        webapps::WebappInstallSource::MENU_BROWSER_TAB,
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
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
        url, web_contents(), &url_loader(), webapps::WebappInstallSource::SYNC,
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          result.app_id = installed_app_id;
          result.code = code;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  std::unique_ptr<WebAppInstallInfo> LoadAndRetrieveWebAppInstallInfoWithIcons(
      const GURL& url) {
    std::unique_ptr<WebAppInstallInfo> result;
    base::RunLoop run_loop;
    install_task_->LoadAndRetrieveWebAppInstallInfoWithIcons(
        url, &url_loader(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<WebAppInstallInfo> web_app_info) {
              result = std::move(web_app_info);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  AppId InstallWebAppFromManifestWithFallback() {
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    DCHECK_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    return result.app_id;
  }

  AppId InstallWebAppWithParams(const WebAppInstallParams& params) {
    AppId app_id;
    base::RunLoop run_loop;
    install_task_->InstallWebAppWithParams(
        web_contents(), params, webapps::WebappInstallSource::EXTERNAL_DEFAULT,
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
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

    data_retriever_->SetIconsDownloadedResult(
        IconsDownloadedResult::kPrimaryPageChanged);
    data_retriever_->SetDownloadedIconsHttpResults(
        DownloadedIconsHttpResults{});
    data_retriever_->SetIcons(IconsMap{});
  }

 protected:
  WebAppInstallTask& install_task() { return *install_task_; }
  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }
  FakeOsIntegrationManager& fake_os_integration_manager() {
    return controller().os_integration_manager();
  }
  TestWebAppUrlLoader& url_loader() { return *url_loader_; }
  FakeDataRetriever& data_retriever() {
    DCHECK(data_retriever_);
    return *data_retriever_;
  }

  WebAppInstallManager& install_manager() const { return *install_manager_; }

  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppPolicyManager> policy_manager_;
  std::unique_ptr<WebAppInstallTask> install_task_;
  std::unique_ptr<FakeWebAppUiManager> ui_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  scoped_refptr<TestFileUtils> file_utils_;

  // Owned by install_task_:
  raw_ptr<FakeDataRetriever> data_retriever_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ArcAppTest arc_test_;
  std::unique_ptr<arc::FakeIntentHelperHost> fake_intent_helper_host_;
  std::unique_ptr<arc::FakeIntentHelperInstance> fake_intent_helper_instance_;
#endif

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<TestWebAppUrlLoader> url_loader_;
  raw_ptr<FakeInstallFinalizer> fake_install_finalizer_ = nullptr;
  base::HistogramTester histogram_tester_;
};

class WebAppInstallTaskWithRunOnOsLoginTest : public WebAppInstallTaskTest {
 public:
  WebAppInstallTaskWithRunOnOsLoginTest() {
    scoped_feature_list_.InitWithFeatures({features::kDesktopPWAsRunOnOsLogin},
                                          {});
  }

  ~WebAppInstallTaskWithRunOnOsLoginTest() override = default;

  void CreateRendererAppInfo(const GURL& url,
                             const std::string& name,
                             const std::string& description,
                             const GURL& scope,
                             absl::optional<SkColor> theme_color,
                             DisplayMode user_display_mode) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();

    web_app_info->start_url = url;
    web_app_info->title = base::UTF8ToUTF16(name);
    web_app_info->description = base::UTF8ToUTF16(description);
    web_app_info->scope = scope;
    web_app_info->theme_color = theme_color;
    web_app_info->user_display_mode = user_display_mode;

    data_retriever_->SetRendererWebAppInstallInfo(std::move(web_app_info));
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
  const absl::optional<SkColor> theme_color = 0xFFAABBCC;

  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  CreateRendererAppInfo(url, "Renderer Name", description, /*scope*/ GURL{},
                        theme_color,
                        /*user_display_mode=*/DisplayMode::kStandalone);
  {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = url;
    manifest->scope = scope;
    manifest->short_name = base::ASCIIToUTF16(manifest_name);

    data_retriever().SetManifest(std::move(manifest), /*is_installable=*/true);
  }

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
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
  EXPECT_EQ(0u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
}

TEST_F(WebAppInstallTaskTest, ForceReinstall) {
  const GURL url = GURL("https://example.com/path");

  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Renderer Name", "Renderer Description");

  const AppId installed_web_app = InstallWebAppFromManifestWithFallback();
  EXPECT_EQ(app_id, installed_web_app);
  ResetInstallTask();

  // Force reinstall:
  CreateRendererAppInfo(url, "Renderer Name2", "Renderer Description2");
  {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = url;
    manifest->scope = url;
    manifest->short_name = u"Manifest Name2";

    data_retriever().SetManifest(std::move(manifest), /*is_installable=*/true);
  }

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting([&](const AppId& force_installed_app_id,
                                     webapps::InstallResultCode code) {
        EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
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

TEST_F(WebAppInstallTaskTest, GetWebAppInstallInfoFailed) {
  // data_retriever_ with empty info means an error.

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kGetWebAppInstallInfoFailed,
                      code);
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
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kWebContentsDestroyed, code);
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

TEST_F(WebAppInstallTaskTest, InstallTaskDestroyed) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kInstallTaskDestroyed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));

  // Destroy install task.
  install_task_.reset();

  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallTaskTest, InstallableCheck) {
  const std::string renderer_description = "RendererDescription";
  CreateRendererAppInfo(GURL("https://renderer.com/path"), "RendererName",
                        renderer_description,
                        GURL("https://renderer.com/scope"), 0x00,
                        /*user_display_mode=*/DisplayMode::kStandalone);

  const GURL manifest_start_url = GURL("https://example.com/start");
  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, manifest_start_url);
  const std::string manifest_name = "Name from Manifest";
  const GURL manifest_scope = GURL("https://example.com/scope");
  const SkColor manifest_theme_color = 0xAABBCCDD;
  const absl::optional<SkColor> expected_theme_color = 0xFFBBCCDD;  // Opaque.
  const auto display_mode = DisplayMode::kMinimalUi;

  {
    auto manifest = blink::mojom::Manifest::New();
    manifest->short_name = u"Short Name from Manifest";
    manifest->name = base::ASCIIToUTF16(manifest_name);
    manifest->start_url = manifest_start_url;
    manifest->scope = manifest_scope;
    manifest->has_theme_color = true;
    manifest->theme_color = manifest_theme_color;
    manifest->display = display_mode;

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);
  }

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
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

  SetIconsMapToRetrieve(std::move(icons_map), IconsDownloadedResult::kCompleted,
                        {net::HttpStatusCode::HTTP_OK});

  InstallWebAppFromManifestWithFallback();

  std::unique_ptr<WebAppInstallInfo> web_app_info =
      fake_install_finalizer().web_app_info();

  // Make sure that icons have been generated for all sub sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info->icon_bitmaps.any));

  // Generated icons are not considered part of the manifest icons.
  EXPECT_TRUE(web_app_info->manifest_icons.empty());

  // Generated icons are not considered part of the manifest shortcut icons.
  EXPECT_TRUE(web_app_info->shortcuts_menu_item_infos.empty());

  const int http_code_class_ok = 2;  // HTTP_OK is 200.
  histogram_tester().ExpectUniqueSample(
      "WebApp.Icon.HttpStatusCodeClassOnCreate", http_code_class_ok, 1);
  histogram_tester().ExpectTotalCount("WebApp.Icon.HttpStatusCodeClassOnSync",
                                      0);

  histogram_tester().ExpectBucketCount("WebApp.Icon.DownloadedResultOnCreate",
                                       IconsDownloadedResult::kCompleted, 1);

  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate",
      net::HttpStatusCode::HTTP_OK, 1);
}

TEST_F(WebAppInstallTaskTest, GetIcons_PrimaryPageChanged) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  SetInstallFinalizerForTesting();

  IconsMap icons_map;
  SetIconsMapToRetrieve(std::move(icons_map),
                        IconsDownloadedResult::kPrimaryPageChanged,
                        /*http_status_codes=*/{});

  InstallWebAppFromManifestWithFallback();

  std::unique_ptr<WebAppInstallInfo> web_app_info =
      fake_install_finalizer().web_app_info();

  // Make sure that icons have been generated for all sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info->icon_bitmaps.any));

  // Generated icons are not considered part of the manifest icons.
  EXPECT_TRUE(web_app_info->manifest_icons.empty());

  // Generated icons are not considered part of the manifest shortcut icons.
  EXPECT_TRUE(web_app_info->shortcuts_menu_item_infos.empty());

  histogram_tester().ExpectTotalCount("WebApp.Icon.HttpStatusCodeClassOnCreate",
                                      0);
  histogram_tester().ExpectTotalCount("WebApp.Icon.HttpStatusCodeClassOnSync",
                                      0);

  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.DownloadedResultOnCreate",
      IconsDownloadedResult::kPrimaryPageChanged, 1);
  histogram_tester().ExpectTotalCount("WebApp.Icon.DownloadedResultOnSync", 0);

  histogram_tester().ExpectTotalCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate", 0);
  histogram_tester().ExpectTotalCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnSync", 0);
}

TEST_F(WebAppInstallTaskTest, GetIcons_IconNotFound) {
  const GURL url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  SetInstallFinalizerForTesting();

  IconsMap icons_map;
  AddEmptyIconToIconsMap(GURL("https://example.com/app.ico"), &icons_map);
  SetIconsMapToRetrieve(std::move(icons_map), IconsDownloadedResult::kCompleted,
                        {net::HttpStatusCode::HTTP_NOT_FOUND});

  InstallWebAppFromManifestWithFallback();

  std::unique_ptr<WebAppInstallInfo> web_app_info =
      fake_install_finalizer().web_app_info();
  EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info->icon_bitmaps.any));
  EXPECT_TRUE(web_app_info->manifest_icons.empty());
  EXPECT_TRUE(web_app_info->shortcuts_menu_item_infos.empty());

  histogram_tester().ExpectBucketCount("WebApp.Icon.DownloadedResultOnCreate",
                                       IconsDownloadedResult::kCompleted, 1);
  histogram_tester().ExpectTotalCount("WebApp.Icon.DownloadedResultOnSync", 0);

  histogram_tester().ExpectBucketCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate",
      net::HttpStatusCode::HTTP_NOT_FOUND, 1);
  histogram_tester().ExpectTotalCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnSync", 0);
}

TEST_F(WebAppInstallTaskTest, WriteDataToDisk) {
  const GURL url = GURL("https://example.com/path");

  struct TestIconInfo {
    IconPurpose purpose;
    std::string icon_url_name;
    SkColor color;
    std::vector<SquareSizePx> sizes_px;
    std::string dir;
  };

  const std::array<TestIconInfo, 3> purpose_infos = {
      TestIconInfo{IconPurpose::ANY,
                   "any",
                   SK_ColorGREEN,
                   {icon_size::k16, icon_size::k512},
                   "Icons"},
      TestIconInfo{IconPurpose::MONOCHROME,
                   "monochrome",
                   SkColorSetARGB(0x80, 0x00, 0x00, 0x00),
                   {icon_size::k32, icon_size::k256},
                   "Icons Monochrome"},
      TestIconInfo{IconPurpose::MASKABLE,
                   "maskable",
                   SK_ColorRED,
                   {icon_size::k64, icon_size::k96, icon_size::k128},
                   "Icons Maskable"}};

  static_assert(
      purpose_infos.size() == static_cast<int>(IconPurpose::kMaxValue) -
                                  static_cast<int>(IconPurpose::kMinValue) + 1,
      "All purposes covered");

  // Prepare all the data to be fetched or downloaded.
  {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = url;
    manifest->short_name = u"Manifest Name";

    IconsMap icons_map;

    for (const TestIconInfo& purpose_info : purpose_infos) {
      for (SquareSizePx s : purpose_info.sizes_px) {
        std::string size_str = base::NumberToString(s);
        GURL icon_url =
            url.Resolve(purpose_info.icon_url_name + size_str + ".png");

        manifest->icons.push_back(
            CreateSquareImageResource(icon_url, s, {purpose_info.purpose}));

        icons_map[icon_url] = {CreateSquareIcon(s, purpose_info.color)};
      }
    }

    data_retriever().SetEmptyRendererWebAppInstallInfo();
    data_retriever().SetManifest(std::move(manifest), /*is_installable=*/true);
    data_retriever().SetIcons(std::move(icons_map));
  }

  // TestingProfile creates temp directory if TestingProfile::path_ is empty
  // (i.e. if TestingProfile::Builder::SetPath was not called by a test fixture)
  const base::FilePath web_apps_dir = GetWebAppsRootDirectory(profile());
  const base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_dir);
  EXPECT_FALSE(file_utils_->DirectoryExists(manifest_resources_directory));

  const AppId app_id = InstallWebAppFromManifestWithFallback();

  EXPECT_TRUE(file_utils_->DirectoryExists(manifest_resources_directory));

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils_->DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils_->IsDirectoryEmpty(temp_dir));

  const base::FilePath app_dir =
      manifest_resources_directory.AppendASCII(app_id);
  EXPECT_TRUE(file_utils_->DirectoryExists(app_dir));

  for (const TestIconInfo& purpose_info : purpose_infos) {
    SCOPED_TRACE(purpose_info.purpose);

    const base::FilePath icons_dir = app_dir.AppendASCII(purpose_info.dir);
    EXPECT_TRUE(file_utils_->DirectoryExists(icons_dir));

    std::map<SquareSizePx, SkBitmap> pngs =
        ReadPngsFromDirectory(file_utils_.get(), icons_dir);

    // The install does ResizeIconsAndGenerateMissing() only for ANY icons.
    if (purpose_info.purpose == IconPurpose::ANY) {
      // Icons are generated for all mandatory sizes in GetIconSizes() in
      // addition to the input k16 and k512 sizes.
      EXPECT_EQ(GetIconSizes().size() + 2UL, pngs.size());
      // Excludes autogenerated sizes.
      for (SquareSizePx s : GetIconSizes()) {
        pngs.erase(s);
      }
    } else {
      EXPECT_EQ(purpose_info.sizes_px.size(), pngs.size());
    }

    for (SquareSizePx size_px : purpose_info.sizes_px) {
      SCOPED_TRACE(size_px);
      ASSERT_TRUE(base::Contains(pngs, size_px));

      SkBitmap icon_bitmap = pngs[size_px];
      EXPECT_EQ(icon_bitmap.width(), icon_bitmap.height());
      EXPECT_EQ(size_px, icon_bitmap.height());
      EXPECT_EQ(purpose_info.color, pngs[size_px].getColor(0, 0));
      pngs.erase(size_px);
    }

    EXPECT_TRUE(pngs.empty());
  }
}

TEST_F(WebAppInstallTaskTest, WriteDataToDiskFailed) {
  const GURL start_url = GURL("https://example.com/path");
  CreateDefaultDataToRetrieve(start_url);
  CreateRendererAppInfo(start_url, "Name", "Description");

  IconsMap icons_map;
  AddIconToIconsMap(GURL("https://example.com/app.ico"), icon_size::k512,
                    SK_ColorBLUE, &icons_map);
  SetIconsMapToRetrieve(std::move(icons_map), IconsDownloadedResult::kCompleted,
                        {net::HttpStatusCode::HTTP_OK});

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
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kWriteDataFailed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  const base::FilePath temp_dir = web_apps_dir.AppendASCII("Temp");
  EXPECT_TRUE(file_utils_->DirectoryExists(temp_dir));
  EXPECT_TRUE(file_utils_->IsDirectoryEmpty(temp_dir));

  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  const base::FilePath app_dir =
      manifest_resources_directory.AppendASCII(app_id);
  EXPECT_FALSE(file_utils_->DirectoryExists(app_dir));
}

TEST_F(WebAppInstallTaskTest, UserInstallDeclined) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  CreateDefaultDataToRetrieve(url);
  CreateRendererAppInfo(url, "Name", "Description");

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestDeclineDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kUserInstallDeclined, code);
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

  EXPECT_EQ(1, fake_install_finalizer().num_reparent_tab_calls());
}

TEST_F(WebAppInstallTaskTest, FinalizerMethodsNotCalled) {
  PrepareTestAppInstall();
  fake_install_finalizer().SetNextFinalizeInstallResult(
      AppId(), webapps::InstallResultCode::kInstallURLLoadTimeOut);

  InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();

  EXPECT_TRUE(result.app_id.empty());
  EXPECT_EQ(webapps::InstallResultCode::kInstallURLLoadTimeOut, result.code);

  EXPECT_EQ(0, fake_install_finalizer().num_reparent_tab_calls());
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromManifest_Success) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  auto manifest = blink::mojom::Manifest::New();
  manifest->start_url = url;
  manifest->short_name = u"Server Name";

  data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromInfo_Success) {
  SetInstallFinalizerForTesting();

  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = url;
  web_app_info->user_display_mode = DisplayMode::kStandalone;
  web_app_info->title = u"App Name";

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfo(
      std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
      ForInstallableSite::kYes, webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);

            std::unique_ptr<WebAppInstallInfo> final_web_app_info =
                fake_install_finalizer().web_app_info();
            EXPECT_EQ(final_web_app_info->user_display_mode,
                      DisplayMode::kStandalone);

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromInfo_GenerateIcons) {
  SetInstallFinalizerForTesting();

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = GURL("https://example.com/path");
  web_app_info->user_display_mode = DisplayMode::kBrowser;
  web_app_info->title = u"App Name";

  // Add square yellow icon.
  AddGeneratedIcon(&web_app_info->icon_bitmaps.any, icon_size::k256,
                   SK_ColorYELLOW);

  base::RunLoop run_loop;

  install_task_->InstallWebAppFromInfo(
      std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
      ForInstallableSite::kYes, webapps::WebappInstallSource::ARC,
      base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                     webapps::InstallResultCode code) {
        std::unique_ptr<WebAppInstallInfo> final_web_app_info =
            fake_install_finalizer().web_app_info();

        // Make sure that icons have been generated for all sub sizes.
        EXPECT_TRUE(
            ContainsOneIconOfEachSize(final_web_app_info->icon_bitmaps.any));

        // Make sure they're all derived from the yellow icon.
        for (const std::pair<const SquareSizePx, SkBitmap>& icon :
             final_web_app_info->icon_bitmaps.any) {
          EXPECT_FALSE(icon.second.drawsNothing());
          EXPECT_EQ(SK_ColorYELLOW, icon.second.getColor(0, 0));
        }

        EXPECT_EQ(final_web_app_info->user_display_mode, DisplayMode::kBrowser);

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
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                     webapps::InstallResultCode code) {
        EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);

        std::unique_ptr<WebAppInstallInfo> final_web_app_info =
            fake_install_finalizer().web_app_info();
        // Make sure that icons have been generated for all sub sizes.
        EXPECT_TRUE(
            ContainsOneIconOfEachSize(final_web_app_info->icon_bitmaps.any));
        for (const std::pair<const SquareSizePx, SkBitmap>& icon :
             final_web_app_info->icon_bitmaps.any) {
          EXPECT_FALSE(icon.second.drawsNothing());
        }

        EXPECT_TRUE(final_web_app_info->manifest_icons.empty());

        run_loop.Quit();
      }));

  run_loop.Run();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebAppInstallTaskTest, IntentToPlayStore) {
  arc_test_.app_instance()->set_is_installable(true);

  const GURL url("https://example.com/scope/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope("https://example.com/scope");
  const absl::optional<SkColor> theme_color = 0xAABBCCDD;

  CreateRendererAppInfo(url, name, description, /*scope*/ GURL{}, theme_color,
                        /*user_display_mode=*/DisplayMode::kStandalone);
  {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = url;
    manifest->scope = scope;
    blink::Manifest::RelatedApplication related_app;
    related_app.platform = u"chromeos_play";
    related_app.id = u"com.app.id";
    manifest->related_applications.push_back(std::move(related_app));

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    data_retriever_->SetIcons(IconsMap{});
  }

  base::RunLoop run_loop;
  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), /*force_shortcut_app=*/false,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kIntentToPlayStore, code);
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
  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(start_url,
                                             /*scope=*/GURL{});

  auto install_task = std::make_unique<WebAppInstallTask>(
      guest_profile, &install_manager(), install_finalizer_.get(),
      std::move(data_retriever), &registrar());

  base::RunLoop run_loop;
  install_task->InstallWebAppWithParams(
      web_contents(), MakeParams(),
      webapps::WebappInstallSource::EXTERNAL_DEFAULT,
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(WebAppInstallTaskTest, InstallWebAppWithParams_DisplayMode) {
  {
    CreateDataToRetrieve(GURL("https://example.com/"),
                         /*user_display_mode=*/DisplayMode::kBrowser);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kUndefined));

    EXPECT_EQ(DisplayMode::kBrowser,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  ResetInstallTask();
  {
    CreateDataToRetrieve(GURL("https://example.org/"),
                         /*user_display_mode=*/DisplayMode::kStandalone);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kUndefined));

    EXPECT_EQ(DisplayMode::kStandalone,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  ResetInstallTask();
  {
    CreateDataToRetrieve(GURL("https://example.au/"),
                         /*user_display_mode=*/DisplayMode::kStandalone);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kBrowser));

    EXPECT_EQ(DisplayMode::kBrowser,
              registrar().GetAppById(app_id)->user_display_mode());
  }
  ResetInstallTask();
  {
    CreateDataToRetrieve(GURL("https://example.app/"),
                         /*user_display_mode=*/DisplayMode::kBrowser);

    auto app_id = InstallWebAppWithParams(MakeParams(DisplayMode::kStandalone));

    EXPECT_EQ(DisplayMode::kStandalone,
              registrar().GetAppById(app_id)->user_display_mode());
  }
}

TEST_F(WebAppInstallTaskTest, InstallWebAppFromManifest_ExpectAppId) {
  const auto url1 = GURL("https://example.com/");
  const auto url2 = GURL("https://example.org/");
  const AppId app_id1 = GenerateAppId(/*manifest_id=*/absl::nullopt, url1);
  const AppId app_id2 = GenerateAppId(/*manifest_id=*/absl::nullopt, url2);
  ASSERT_NE(app_id1, app_id2);
  {
    CreateDefaultDataToRetrieve(url1);
    install_task().ExpectAppId(app_id1);
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id1, result.app_id);
    EXPECT_TRUE(registrar().GetAppById(app_id1));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url2);
    install_task().ExpectAppId(app_id1);
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    EXPECT_EQ(webapps::InstallResultCode::kExpectedAppIdCheckFailed,
              result.code);
    EXPECT_EQ(app_id2, result.app_id);
    EXPECT_FALSE(registrar().GetAppById(app_id2));
  }
}

TEST_F(WebAppInstallTaskTest, LoadAndInstallWebAppFromManifestWithFallback) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(webapps::InstallResultCode::kInstallURLRedirected, result.code);
    EXPECT_TRUE(result.app_id.empty());
    EXPECT_FALSE(registrar().GetAppById(app_id));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kFailedPageTookTooLong);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(webapps::InstallResultCode::kInstallURLLoadTimeOut, result.code);
    EXPECT_TRUE(result.app_id.empty());
    EXPECT_FALSE(registrar().GetAppById(app_id));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
    EXPECT_TRUE(registrar().GetAppById(app_id));
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    InstallResult result = LoadAndInstallWebAppFromManifestWithFallback(url);
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
    EXPECT_TRUE(registrar().GetAppById(app_id));
  }
}

TEST_F(WebAppInstallTaskTest, LoadAndRetrieveWebAppInstallInfoWithIcons) {
  const GURL url = GURL("https://example.com/path");
  const GURL start_url = GURL("https://example.com/start");
  const std::string name = "Name";
  const std::string description = "Description";
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    std::unique_ptr<WebAppInstallInfo> result =
        LoadAndRetrieveWebAppInstallInfoWithIcons(url);
    EXPECT_FALSE(result);
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(url);
    url_loader().SetNextLoadUrlResult(
        url, WebAppUrlLoader::Result::kFailedPageTookTooLong);

    std::unique_ptr<WebAppInstallInfo> result =
        LoadAndRetrieveWebAppInstallInfoWithIcons(url);
    EXPECT_FALSE(result);
  }
  ResetInstallTask();
  {
    CreateDefaultDataToRetrieve(start_url);
    CreateRendererAppInfo(url, name, description);
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    std::unique_ptr<WebAppInstallInfo> result =
        LoadAndRetrieveWebAppInstallInfoWithIcons(url);
    EXPECT_TRUE(result);
    EXPECT_EQ(result->start_url, start_url);
    EXPECT_TRUE(result->manifest_icons.empty());
    EXPECT_FALSE(result->icon_bitmaps.any.empty());
  }
  ResetInstallTask();
  {
    // Verify the callback is always called.
    base::RunLoop run_loop;
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, GURL{});
    url_loader().SetNextLoadUrlResult(url, WebAppUrlLoader::Result::kUrlLoaded);

    auto task = std::make_unique<WebAppInstallTask>(
        profile(), &install_manager(), install_finalizer_.get(),
        std::move(data_retriever), &registrar());

    std::unique_ptr<WebAppInstallInfo> info;
    task->LoadAndRetrieveWebAppInstallInfoWithIcons(
        url, &url_loader(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<WebAppInstallInfo> app_info) {
              info = std::move(app_info);
              run_loop.Quit();
            }));
    task.reset();
    run_loop.Run();
  }
}

TEST_F(WebAppInstallTaskTest, StorageIsolationFlagSaved) {
  const GURL manifest_start_url = GURL("https://example.com/start");
  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, manifest_start_url);

  auto manifest = blink::mojom::Manifest::New();
  manifest->short_name = u"Short Name from Manifest";
  manifest->name = u"Name from Manifest";
  manifest->start_url = GURL("https://example.com/start");
  manifest->isolated_storage = true;

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  UpdateWebAppInfoFromManifest(*manifest, manifest_start_url,
                               web_app_info.get());

  data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);
  data_retriever_->SetRendererWebAppInstallInfo(std::move(web_app_info));

  base::RunLoop run_loop;
  bool callback_called = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), /*force_shortcut_app=*/false,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  const WebApp* web_app = registrar().GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);
  EXPECT_TRUE(web_app->IsStorageIsolated());
}

TEST_F(WebAppInstallTaskWithRunOnOsLoginTest,
       InstallFromWebContentsRunOnOsLoginByPolicy) {
  EXPECT_TRUE(AreWebAppsUserInstallable(profile()));

  const GURL url = GURL("https://example.com/scope/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const absl::optional<SkColor> theme_color = 0xFFAABBCC;

  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  CreateDefaultDataToRetrieve(url, scope);
  CreateRendererAppInfo(url, name, description, /*scope=*/GURL{}, theme_color,
                        /*user_display_mode=*/DisplayMode::kStandalone);

  const char kWebAppSettingWithDefaultConfiguration[] = R"({
    "https://example.com/scope/path": {
      "run_on_os_login": "run_windowed"
    },
    "*": {
      "run_on_os_login": "blocked"
    }
  })";

  test::SetWebAppSettingsDictPref(profile(),
                                  kWebAppSettingWithDefaultConfiguration);
  controller().policy_manager().RefreshPolicySettingsForTesting();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_task_->InstallWebAppFromManifestWithFallback(
      web_contents(), force_shortcut_app,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(test::TestAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
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
  EXPECT_EQ(RunOnOsLoginMode::kNotRun, web_app->run_on_os_login_mode());
  EXPECT_EQ(1u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
}

// TODO(https://crbug.com/1096953): Move these tests out into a dedicated
// unittest file.
class WebAppInstallTaskTestWithShortcutsMenu : public WebAppInstallTaskTest {
 public:
  GURL ShortcutIconUrl() {
    return GURL("https://example.com/icons/shortcut_icon.png");
  }

  GURL ShortcutItemUrl() { return GURL("https://example.com/path/item"); }

  // Installs the app and validates |final_web_app_info| matches the args passed
  // in.
  InstallResult InstallWebAppWithShortcutsMenuValidateAndGetResults(
      const GURL& start_url,
      SkColor theme_color,
      const std::string& shortcut_name,
      const GURL& shortcut_url,
      SquareSizePx icon_size,
      const GURL& icon_src) {
    {
      auto manifest = blink::mojom::Manifest::New();
      manifest->start_url = start_url;
      manifest->has_theme_color = true;
      manifest->theme_color = theme_color;
      manifest->name = u"Manifest Name";

      // Add shortcuts to manifest.
      blink::Manifest::ShortcutItem shortcut_item;
      shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
      shortcut_item.url = shortcut_url;
      blink::Manifest::ImageResource icon;
      icon.src = icon_src;
      icon.sizes.emplace_back(icon_size, icon_size);
      icon.purpose.push_back(IconPurpose::ANY);
      shortcut_item.icons.push_back(std::move(icon));
      manifest->shortcuts.push_back(std::move(shortcut_item));

      data_retriever_->SetManifest(std::move(manifest),
                                   /*is_installable=*/true);
    }

    base::RunLoop run_loop;
    bool callback_called = false;

    SetInstallFinalizerForTesting();

    InstallResult result;
    install_task_->InstallWebAppFromManifest(
        web_contents(), /*bypass_service_worker_check=*/false,
        webapps::WebappInstallSource::MENU_BROWSER_TAB,
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          result.app_id = installed_app_id;
          result.code = code;
          std::unique_ptr<WebAppInstallInfo> final_web_app_info =
              fake_install_finalizer().web_app_info();
          EXPECT_EQ(theme_color, final_web_app_info->theme_color);
          EXPECT_EQ(1u, final_web_app_info->shortcuts_menu_item_infos.size());
          EXPECT_EQ(base::UTF8ToUTF16(shortcut_name),
                    final_web_app_info->shortcuts_menu_item_infos[0].name);
          EXPECT_EQ(shortcut_url,
                    final_web_app_info->shortcuts_menu_item_infos[0].url);
          EXPECT_EQ(1u, final_web_app_info->shortcuts_menu_item_infos[0]
                            .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                            .size());
          EXPECT_EQ(icon_size,
                    final_web_app_info->shortcuts_menu_item_infos[0]
                        .GetShortcutIconInfosForPurpose(IconPurpose::ANY)[0]
                        .square_size_px);
          EXPECT_EQ(icon_src,
                    final_web_app_info->shortcuts_menu_item_infos[0]
                        .GetShortcutIconInfosForPurpose(IconPurpose::ANY)[0]
                        .url);
          EXPECT_EQ(0u,
                    final_web_app_info->shortcuts_menu_item_infos[0]
                        .GetShortcutIconInfosForPurpose(IconPurpose::MASKABLE)
                        .size());

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
      const GURL& url,
      SkColor theme_color,
      const std::string& shortcut_name,
      const GURL& shortcut_url,
      SquareSizePx icon_size,
      const GURL& icon_src) {
    InstallResult result;
    const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = url;
    web_app_info->user_display_mode = DisplayMode::kStandalone;
    web_app_info->theme_color = theme_color;
    web_app_info->title = u"App Name";

    WebAppShortcutsMenuItemInfo shortcut_item;
    WebAppShortcutsMenuItemInfo::Icon icon;
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = shortcut_url;

    icon.url = icon_src;
    icon.square_size_px = icon_size;
    shortcut_item.SetShortcutIconInfosForPurpose(IconPurpose::MASKABLE,
                                                 {std::move(icon)});
    web_app_info->shortcuts_menu_item_infos.push_back(std::move(shortcut_item));

    base::RunLoop run_loop;
    bool callback_called = false;

    SetInstallFinalizerForTesting();

    fake_install_finalizer().FinalizeUpdate(
        *web_app_info,
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code,
                                       OsHooksErrors os_hooks_errors) {
          result.app_id = installed_app_id;
          result.code = code;
          std::unique_ptr<WebAppInstallInfo> final_web_app_info =
              fake_install_finalizer().web_app_info();
          EXPECT_EQ(theme_color, final_web_app_info->theme_color);
          EXPECT_EQ(1u, final_web_app_info->shortcuts_menu_item_infos.size());
          EXPECT_EQ(base::UTF8ToUTF16(shortcut_name),
                    final_web_app_info->shortcuts_menu_item_infos[0].name);
          EXPECT_EQ(shortcut_url,
                    final_web_app_info->shortcuts_menu_item_infos[0].url);
          EXPECT_EQ(0u, final_web_app_info->shortcuts_menu_item_infos[0]
                            .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                            .size());
          EXPECT_EQ(1u,
                    final_web_app_info->shortcuts_menu_item_infos[0]
                        .GetShortcutIconInfosForPurpose(IconPurpose::MASKABLE)
                        .size());
          EXPECT_EQ(icon_size, final_web_app_info->shortcuts_menu_item_infos[0]
                                   .GetShortcutIconInfosForPurpose(
                                       IconPurpose::MASKABLE)[0]
                                   .square_size_px);
          EXPECT_EQ(icon_src, final_web_app_info->shortcuts_menu_item_infos[0]
                                  .GetShortcutIconInfosForPurpose(
                                      IconPurpose::MASKABLE)[0]
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
};

// Declare constants needed by tests.
constexpr char WebAppInstallTaskTestWithShortcutsMenu::kShortcutItemName[];
constexpr SquareSizePx WebAppInstallTaskTestWithShortcutsMenu::kIconSize;
constexpr SkColor WebAppInstallTaskTestWithShortcutsMenu::kInitialThemeColor;
constexpr SkColor WebAppInstallTaskTestWithShortcutsMenu::kFinalThemeColor;

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       InstallWebAppFromManifest_Success) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  InstallResult result = InstallWebAppWithShortcutsMenuValidateAndGetResults(
      url, kInitialThemeColor, "shortcut",
      GURL("https://example.com/path/page"), kIconSize,
      GURL("https://example.com/icons/shortcut.png"));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);
}

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       UpdateWebAppFromInfo_AddShortcutsMenu) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  // Install the app without a shortcuts menu.
  {
    CreateDefaultDataToRetrieve(url);
    install_task().ExpectAppId(app_id);
    InstallResult result = InstallWebAppFromManifestWithFallbackAndGetResults();
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
  ResetInstallTask();

  // Update the installed app, adding a Shortcuts Menu in the process.
  {
    InstallResult result = UpdateWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, "shortcut",
        GURL("https://example.com/path/page"), kIconSize, ShortcutIconUrl());
    EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
              result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
}

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       UpdateWebAppFromInfo_UpdateShortcutsMenu) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  // Install the app.
  {
    InstallResult result = InstallWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, "shortcut",
        GURL("https://example.com/path/page"), 2 * kIconSize,
        GURL("https://example.com/icons/shortcut.png"));
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
  ResetInstallTask();

  // Update the installed app, Shortcuts Menu has changed.
  {
    InstallResult result = UpdateWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, kShortcutItemName, ShortcutItemUrl(),
        kIconSize, ShortcutIconUrl());
    EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
              result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
}

TEST_F(WebAppInstallTaskTestWithShortcutsMenu,
       UpdateWebAppFromInfo_ShortcutsMenuNotChanged) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  // Install the app.
  {
    InstallResult result = InstallWebAppWithShortcutsMenuValidateAndGetResults(
        url, kInitialThemeColor, kShortcutItemName, ShortcutItemUrl(),
        kIconSize, ShortcutIconUrl());
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
  ResetInstallTask();

  // Update the installed app. Only theme color changed, so Shortcuts Menu
  // should stay the same.
  {
    InstallResult result = UpdateWebAppWithShortcutsMenuValidateAndGetResults(
        url, kFinalThemeColor, kShortcutItemName, ShortcutItemUrl(), kIconSize,
        ShortcutIconUrl());
    EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
              result.code);
    EXPECT_EQ(app_id, result.app_id);
  }
}

class WebAppInstallTaskTestWithFileHandlers : public WebAppInstallTaskTest {
 public:
  WebAppInstallTaskTestWithFileHandlers() {
    scoped_feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI},
                                          {});
  }

  blink::mojom::ManifestPtr CreateManifest(const GURL& url) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = url;
    manifest->name = u"Manifest Name";
    return manifest;
  }

  std::unique_ptr<WebAppInstallInfo> CreateWebAppInstallInfo(const GURL& url) {
    auto app_info = std::make_unique<WebAppInstallInfo>();
    app_info->title = u"Test App";
    app_info->start_url = url;
    app_info->scope = url;
    return app_info;
  }

  void AddFileHandler(
      std::vector<blink::mojom::ManifestFileHandlerPtr>* file_handlers) {
    auto file_handler = blink::mojom::ManifestFileHandler::New();
    file_handler->action = GURL("https://example.com/action");
    file_handler->name = u"Test handler";
    file_handler->accept[u"application/pdf"].emplace_back(u".pdf");
    file_handlers->push_back(std::move(file_handler));
  }

  InstallResult InstallWebAppFromManifest(blink::mojom::ManifestPtr manifest,
                                          webapps::WebappInstallSource source) {
    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    base::RunLoop run_loop;
    bool callback_called = false;
    InstallResult result;

    install_task_->InstallWebAppFromManifest(
        web_contents(), /*bypass_service_worker_check=*/false, source,
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          result.app_id = installed_app_id;
          result.code = code;

          callback_called = true;
          run_loop.Quit();
        }));

    run_loop.Run();
    EXPECT_TRUE(callback_called);
    return result;
  }

  InstallResult UpdateWebAppFromInfo(
      const AppId& app_id,
      std::unique_ptr<WebAppInstallInfo> app_info) {
    base::RunLoop run_loop;
    bool callback_called = false;
    InstallResult result;

    install_finalizer_->FinalizeUpdate(
        *app_info,
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code,
                                       OsHooksErrors os_hooks_errors) {
          result.app_id = installed_app_id;
          result.code = code;
          result.os_hooks_errors = os_hooks_errors;

          callback_called = true;
          run_loop.Quit();
        }));

    run_loop.Run();
    EXPECT_TRUE(callback_called);
    return result;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebAppInstallTaskTestWithFileHandlers,
       UpdateWebAppFromInfo_OsIntegrationEnabledForUserInstalledApps) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  // Install the app.
  InstallResult install_result = InstallWebAppFromManifest(
      CreateManifest(url), webapps::WebappInstallSource::MENU_BROWSER_TAB);
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            install_result.code);
  EXPECT_EQ(app_id, install_result.app_id);
  EXPECT_EQ(1u, fake_os_integration_manager().num_create_file_handlers_calls());

  ResetInstallTask();

  // Update the app, adding a file handler.
  auto app_info = CreateWebAppInstallInfo(url);
  std::vector<blink::mojom::ManifestFileHandlerPtr> file_handlers;
  AddFileHandler(&file_handlers);
  app_info->file_handlers = CreateFileHandlersFromManifest(file_handlers, url);

  InstallResult update_result =
      UpdateWebAppFromInfo(app_id, std::move(app_info));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            update_result.code);
  EXPECT_EQ(app_id, update_result.app_id);
  EXPECT_EQ(1u, fake_os_integration_manager().num_update_file_handlers_calls());
}

TEST_F(WebAppInstallTaskTestWithFileHandlers,
       UpdateWebAppFromInfo_OsIntegrationDisabledForDefaultApps) {
  const GURL url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

  // Install the app.
  InstallResult install_result = InstallWebAppFromManifest(
      CreateManifest(url), webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            install_result.code);
  EXPECT_EQ(app_id, install_result.app_id);
#if BUILDFLAG(IS_CHROMEOS)
  // OS integration is always enabled in ChromeOS
  EXPECT_EQ(1u, fake_os_integration_manager().num_create_file_handlers_calls());
#else
  EXPECT_EQ(0u, fake_os_integration_manager().num_create_file_handlers_calls());
#endif  // BUILDFLAG(IS_CHROMEOS)

  ResetInstallTask();

  // Update the app, adding a file handler.
  auto app_info = CreateWebAppInstallInfo(url);
  std::vector<blink::mojom::ManifestFileHandlerPtr> file_handlers;
  AddFileHandler(&file_handlers);
  app_info->file_handlers = CreateFileHandlersFromManifest(file_handlers, url);

  InstallResult update_result =
      UpdateWebAppFromInfo(app_id, std::move(app_info));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            update_result.code);
  EXPECT_EQ(app_id, update_result.app_id);
#if BUILDFLAG(IS_CHROMEOS)
  // OS integration is always enabled in ChromeOS
  EXPECT_EQ(1u, fake_os_integration_manager().num_update_file_handlers_calls());
#else
  EXPECT_EQ(0u, fake_os_integration_manager().num_update_file_handlers_calls());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace web_app
