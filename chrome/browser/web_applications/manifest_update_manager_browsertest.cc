// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_manager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_sync_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_registry.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "base/command_line.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/url_handler_manager_impl.h"
#endif

namespace web_app {

namespace {

constexpr char kUpdateHistogramName[] = "Webapp.Update.ManifestUpdateResult";

constexpr char kInstallableIconList[] = R"(
  [
    {
      "src": "launcher-icon-4x.png",
      "sizes": "192x192",
      "type": "image/png"
    }
  ]
)";
constexpr SkColor kInstallableIconTopLeftColor =
    SkColorSetRGB(0x15, 0x96, 0xE0);
constexpr SkColor kBasicIconTopLeftColor = SkColorSetRGB(0x55, 0x55, 0x55);

constexpr char kAnotherInstallableIconList[] = R"(
  [
    {
      "src": "/banners/image-512px.png",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
)";

constexpr char kAnotherShortcutsItemName[] = "Timeline";
constexpr char16_t kAnotherShortcutsItemName16[] = u"Timeline";
constexpr char kAnotherShortcutsItemUrl[] = "/shortcut";
constexpr char kAnotherShortcutsItemShortName[] = "H";
constexpr char kAnotherShortcutsItemDescription[] = "Navigate home";
constexpr char kAnotherIconSrc[] = "/launcher-icon-4x.png";
constexpr int kAnotherIconSize = 192;

constexpr char kShortcutsItem[] = R"(
  [
    {
      "name": "Home",
      "short_name": "HM",
      "description": "Go home",
      "url": ".",
      "icons": [
        {
          "src": "/banners/image-512px.png",
          "sizes": "512x512",
          "type": "image/png"
        }
      ]
    }
  ]
)";

constexpr char kShortcutsItems[] = R"(
  [
    {
      "name": "Home",
      "short_name": "HM",
      "description": "Go home",
      "url": ".",
      "icons": [
        {
          "src": "/banners/image-512px.png",
          "sizes": "512x512",
          "type": "image/png"
        }
      ]
    },
    {
      "name": "Settings",
      "short_name": "ST",
      "description": "App settings",
      "url": "/settings",
      "icons": [
        {
          "src": "launcher-icon-4x.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  ]
)";

constexpr SkColor kAnotherInstallableIconTopLeftColor =
    SkColorSetRGB(0x5C, 0x5C, 0x5C);

ManifestUpdateManager& GetManifestUpdateManager(Browser* browser) {
  return WebAppProvider::GetForTest(browser->profile())
      ->manifest_update_manager();
}

class UpdateCheckResultAwaiter {
 public:
  explicit UpdateCheckResultAwaiter(Browser* browser, const GURL& url)
      : browser_(browser), url_(url) {
    SetCallback();
  }

  void SetCallback() {
    GetManifestUpdateManager(browser_).SetResultCallbackForTesting(
        base::BindOnce(&UpdateCheckResultAwaiter::OnResult,
                       base::Unretained(this)));
  }

  ManifestUpdateResult AwaitNextResult() && {
    run_loop_.Run();
    return *result_;
  }

  void OnResult(const GURL& url, ManifestUpdateResult result) {
    if (url != url_) {
      SetCallback();
      return;
    }
    result_ = result;
    run_loop_.Quit();
  }

 private:
  Browser* browser_ = nullptr;
  const GURL& url_;
  base::RunLoop run_loop_;
  absl::optional<ManifestUpdateResult> result_;
};

}  // namespace

class ManifestUpdateManagerBrowserTest : public InProcessBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest() = default;
  ManifestUpdateManagerBrowserTest(const ManifestUpdateManagerBrowserTest&) =
      delete;
  ManifestUpdateManagerBrowserTest& operator=(
      const ManifestUpdateManagerBrowserTest&) = delete;

  ~ManifestUpdateManagerBrowserTest() override = default;

  void SetUp() override {
    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    http_server_.RegisterRequestHandler(base::BindRepeating(
        &ManifestUpdateManagerBrowserTest::RequestHandlerOverride,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_.Start());
    // Suppress globally to avoid OS hooks deployed for system web app during
    // WebAppProvider setup.
    os_hooks_suppress_ =
        OsIntegrationManager::ScopedSuppressOsHooksForTesting();
    chrome::SetAutoAcceptAppIdentityUpdateForTesting(false);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Cannot construct RunLoop in constructor due to threading restrictions.
    shortcut_run_loop_.emplace();
  }

  void OnShortcutInfoRetrieved(std::unique_ptr<ShortcutInfo> shortcut_info) {
    if (shortcut_info) {
      updated_shortcut_top_left_color_ =
          shortcut_info->favicon.begin()->AsBitmap().getColor(0, 0);
    }
    shortcut_run_loop_->Quit();
  }

  void CheckShortcutInfoUpdated(const AppId& app_id, SkColor top_left_color) {
    GetProvider().os_integration_manager().GetShortcutInfoForApp(
        app_id, base::BindOnce(
                    &ManifestUpdateManagerBrowserTest::OnShortcutInfoRetrieved,
                    base::Unretained(this)));
    shortcut_run_loop_->Run();
    EXPECT_EQ(updated_shortcut_top_left_color_, top_left_color);
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    if (request_override_)
      return request_override_.Run(request);
    return nullptr;
  }

  void OverrideManifest(const char* manifest_template,
                        const std::vector<std::string>& substitutions) {
    std::string content = base::ReplaceStringPlaceholders(
        manifest_template, substitutions, nullptr);
    request_override_ = base::BindLambdaForTesting(
        [this, content = std::move(content)](
            const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL() != GetManifestURL())
            return nullptr;
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(net::HTTP_FOUND);
          http_response->set_content(content);
          return std::move(http_response);
        });
  }

  GURL GetAppURL() const {
    return http_server_.GetURL("/banners/manifest_test_page.html");
  }

  GURL GetManifestURL() const {
    return http_server_.GetURL("/banners/manifest.json");
  }

  AppId InstallWebApp() {
    GURL app_url = GetAppURL();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

    AppId app_id;
    base::RunLoop run_loop;
    GetProvider().install_manager().InstallWebAppFromManifestWithFallback(
        browser()->tab_strip_model()->GetActiveWebContents(),
        /*force_shortcut_app=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting(
            [&](const AppId& new_app_id, InstallResultCode code) {
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = new_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();
    return app_id;
  }

  AppId InstallDefaultApp() {
    const GURL app_url = GetAppURL();
    base::RunLoop run_loop;
    ExternalInstallOptions install_options(
        app_url, DisplayMode::kStandalone,
        ExternalInstallSource::kInternalDefault);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.install_placeholder = true;
    GetProvider().externally_managed_app_manager().Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& installed_app_url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(installed_app_url, app_url);
              EXPECT_EQ(result.code, InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();
    return GetProvider().registrar().LookupExternalAppId(app_url).value();
  }

  AppId InstallPolicyApp() {
    const GURL app_url = GetAppURL();
    base::RunLoop run_loop;
    ExternalInstallOptions install_options(
        app_url, DisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.install_placeholder = true;
    GetProvider().externally_managed_app_manager().Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& installed_app_url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(installed_app_url, app_url);
              EXPECT_EQ(result.code, InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();
    return GetProvider().registrar().LookupExternalAppId(app_url).value();
  }

  AppId InstallWebAppFromSync(const GURL& start_url) {
    const AppId app_id =
        GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

    std::vector<std::unique_ptr<WebApp>> add_synced_apps_data;
    {
      auto synced_specifics_data = std::make_unique<WebApp>(app_id);
      synced_specifics_data->SetStartUrl(start_url);

      synced_specifics_data->AddSource(Source::kSync);
      synced_specifics_data->SetUserDisplayMode(DisplayMode::kBrowser);
      synced_specifics_data->SetName("Name From Sync");

      WebApp::SyncFallbackData sync_fallback_data;
      sync_fallback_data.name = "Name From Sync";
      sync_fallback_data.theme_color = SK_ColorMAGENTA;
      sync_fallback_data.scope = GURL("https://example.com/sync_scope");

      apps::IconInfo apps_icon_info = CreateIconInfo(
          /*icon_base_url=*/start_url, IconPurpose::MONOCHROME, 64);
      sync_fallback_data.icon_infos.push_back(std::move(apps_icon_info));

      synced_specifics_data->SetSyncFallbackData(std::move(sync_fallback_data));

      add_synced_apps_data.push_back(std::move(synced_specifics_data));
    }

    WebAppTestInstallObserver observer(browser()->profile());

    GetProvider().sync_bridge().set_disable_checks_for_testing(true);

    sync_bridge_test_utils::AddApps(GetProvider().sync_bridge(),
                                    add_synced_apps_data);

    return observer.BeginListeningAndWait({app_id});
  }

  // Simulates what AppLauncherHandler::HandleInstallAppLocally() does.
  void InstallAppLocally(const WebApp* web_app) {
    // Doesn't call GetProvider().os_integration_manager().InstallOsHooks() to
    // suppress OS hooks.
    GetProvider().sync_bridge().SetAppIsLocallyInstalled(web_app->app_id(),
                                                         true);
    GetProvider().sync_bridge().SetAppInstallTime(web_app->app_id(),
                                                  base::Time::Now());
  }

  void SetTimeOverride(base::Time time_override) {
    GetManifestUpdateManager(browser()).set_time_override_for_testing(
        time_override);
  }

  ManifestUpdateResult GetResultAfterPageLoad(const GURL& url) {
    UpdateCheckResultAwaiter awaiter(browser(), url);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return std::move(awaiter).AwaitNextResult();
  }

  WebAppProvider& GetProvider() {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  SkColor ReadAppIconPixel(const AppId& app_id,
                           SquareSizePx size,
                           int x = 0,
                           int y = 0) {
    return IconManagerReadAppIconPixel(
        WebAppProvider::GetForTest(browser()->profile())->icon_manager(),
        app_id, size, x, y);
  }

 protected:
  net::EmbeddedTestServer::HandleRequestCallback request_override_;

  base::HistogramTester histogram_tester_;

  net::EmbeddedTestServer http_server_;

 private:
  absl::optional<base::RunLoop> shortcut_run_loop_;
  absl::optional<SkColor> updated_shortcut_top_left_color_;
  ScopedOsHooksSuppress os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckOutOfScopeNavigation) {
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kNoAppInScope);

  AppId app_id = InstallWebApp();

  EXPECT_EQ(GetResultAfterPageLoad(GURL("http://example.org")),
            ManifestUpdateResult::kNoAppInScope);

  histogram_tester_.ExpectTotalCount(kUpdateHistogramName, 0);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest, CheckIsThrottled) {
  constexpr base::TimeDelta kDelayBetweenChecks = base::Days(1);
  base::Time time_override = base::Time::UnixEpoch();
  SetTimeOverride(time_override);

  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks * 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kThrottled, 2);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 3);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByWebContentsDestroyed) {
  AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()).hang_update_checks_for_testing();

  GURL url = GetAppURL();
  UpdateCheckResultAwaiter awaiter(browser(), url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  chrome::CloseTab(browser());
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kWebContentsDestroyed);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kWebContentsDestroyed, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByAppUninstalled) {
  AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()).hang_update_checks_for_testing();

  GURL url = GetAppURL();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::RunLoop run_loop;
  UpdateCheckResultAwaiter awaiter(browser(), url);
  GetProvider().install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_TRUE(uninstalled);
        run_loop.Quit();
      }));

  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUninstalling);

  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppUninstalling, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresWhitespaceDifferences) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
      $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, ""});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "\n\n\n\n"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNameChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresShortNameChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "short_name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {"Short test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {"Different short test app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckNameUpdatesForDefaultApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  AppId app_id = InstallDefaultApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id),
            "Different app name");
}

class ManifestUpdateManagerAppIdentityBrowserTest
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPwaUpdateDialogForNameAndIcon};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerAppIdentityBrowserTest,
                       VerifyAppIdentityUpdatesWithDlgForUserInstalledApps) {
  chrome::SetAutoAcceptAppIdentityUpdateForTesting(true);

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id),
            "Different app name");
  CheckShortcutInfoUpdated(app_id, kAnotherInstallableIconTopLeftColor);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresStartUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": "$1",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"a.html", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"b.html", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppIdMismatch);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppIdMismatch, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNoManifestChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresInvalidManifest) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, ""});
  AppId app_id = InstallWebApp();
  OverrideManifest(kManifestTemplate, {kInstallableIconList,
                                       "invalid manifest syntax !@#$%^*&()"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppNotEligible);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppNotEligible, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNonLocalApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();

  GetProvider().sync_bridge().SetAppIsLocallyInstalled(app_id, false);
  EXPECT_FALSE(GetProvider().registrar().IsLocallyInstalled(app_id));

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kNoAppInScope);
  histogram_tester_.ExpectTotalCount(kUpdateHistogramName, 0);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresPlaceholderApps) {
  // Set up app URL to redirect to force placeholder app to install.
  const GURL app_url = GetAppURL();
  request_override_ = base::BindLambdaForTesting(
      [&app_url](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL() != app_url)
          return nullptr;
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        http_response->AddCustomHeader(
            "Location", "http://other-origin.com/defaultresponse");
        http_response->set_content("redirect page");
        return std::move(http_response);
      });

  // Install via ExternallyManagedAppManager, the redirect to a different origin
  // should cause it to install a placeholder app.
  AppId app_id = InstallPolicyApp();
  EXPECT_TRUE(GetProvider().registrar().IsPlaceholderApp(app_id));

  // Manifest updating should ignore non-redirect loads for placeholder apps
  // because the ExternallyManagedAppManager will handle these.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(app_url),
            ManifestUpdateResult::kAppIsPlaceholder);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsPlaceholder, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsThemeColorChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorBLUE);

  // Check that OnWebAppInstalled and OnWebAppWillBeUninstalled are not called
  // if in-place web app update happens.
  WebAppTestRegistryObserverAdapter install_observer(
      &GetProvider().registrar());
  install_observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([](const AppId& app_id) { NOTREACHED(); }));
  install_observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([](const AppId& app_id) { NOTREACHED(); }));

  // CSS #RRGGBBAA syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "#00FF00F0"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  // Updated theme_color loses any transparency.
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0x00, 0xFF, 0x00));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsBackgroundColorChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "background_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppBackgroundColor(app_id),
            SK_ColorBLUE);

  // CSS #RRGGBBAA syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  EXPECT_EQ(GetProvider().registrar().GetAppBackgroundColor(app_id),
            SkColorSetARGB(0xFF, 0xFF, 0x00, 0x00));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsManifestUrlChange) {
  // This matches the content of chrome/test/data/banners/manifest_one_icon.json
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Manifest test app",
      "icons": [
          {
            "src": "image-512px.png",
            "sizes": "512x512",
            "type": "image/png"
          }
      ],
      "start_url": "manifest_test_page.html",
      "display": "standalone",
      "orientation": "landscape"
    }
  )";
  OverrideManifest(kManifestTemplate, {});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppManifestUrl(app_id),
            GetManifestURL());

  // Load a page which contains the same manifest content but at a new manifest
  // URL.
  url::Replacements<char> replacements;
  std::string query = "manifest=/banners/manifest_one_icon.json";
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
  GURL app_url_with_new_manifest = GetAppURL().ReplaceComponents(replacements);
  EXPECT_EQ(GetResultAfterPageLoad(app_url_with_new_manifest),
            ManifestUpdateResult::kAppUpdated);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppManifestUrl(app_id),
            http_server_.GetURL("/banners/manifest_one_icon.json"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest, CheckKeepsSameName) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2,
      "theme_color": "$3"
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {"App name 1", kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorBLUE);
  EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id), "App name 1");

  OverrideManifest(kManifestTemplate,
                   {"App name 2", kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorRED);
  // The app name must not change without user confirmation.
  EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id), "App name 1");
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesNotFindIconUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesFindIconUrlChangeForDefaultApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallDefaultApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kAnotherInstallableIconTopLeftColor);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckUpdatedPolicyAppsNotUninstallable) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "theme_color": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"blue", kInstallableIconList});
  AppId app_id = InstallPolicyApp();
  EXPECT_FALSE(
      GetProvider().install_finalizer().CanUserUninstallWebApp(app_id));

  OverrideManifest(kManifestTemplate, {"red", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  // Policy installed apps should continue to be not uninstallable by the user
  // after updating.
  EXPECT_FALSE(
      GetProvider().install_finalizer().CanUserUninstallWebApp(app_id));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsScopeChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "$1",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/banners/", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"/", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesNotApplyIconURLChange) {
  // This test changes the scope and also the icon list. The scope should update
  // but the icons should not.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "$1",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/banners/", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"/", kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // The icon should not be updated.
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesApplyIconURLChangeForDefaultApps) {
  // This test changes the scope and also the icon list. The scope should update
  // along with the icons.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "$1",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/banners/", kInstallableIconList});
  AppId app_id = InstallDefaultApp();

  OverrideManifest(kManifestTemplate, {"/", kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // The icon should have updated.
  CheckShortcutInfoUpdated(app_id, kAnotherInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

class ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate
    : public ManifestUpdateManagerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate() {
    scoped_feature_list_.InitWithFeatureState(
        features::kWebAppManifestPolicyAppIdentityUpdate, GetParam());
  }

  bool ExpectUpdateAllowed() {
    return base::FeatureList::IsEnabled(
        features::kWebAppManifestPolicyAppIdentityUpdate);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                       CheckDoesApplyIconURLChangeForPolicyAppsWithFlag) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallPolicyApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});

  if (ExpectUpdateAllowed()) {
    // The icon should have updated (because the flag is enabled).
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpdated);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 1);
    CheckShortcutInfoUpdated(app_id, kAnotherInstallableIconTopLeftColor);
  } else {
    // The icon should not have updated.
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpToDate);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 0);
    CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  }
}

// This test ensures app name cannot be changed for policy apps (without a flag
// allowing it).
IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                       CheckNameUpdatesForPolicyApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  AppId app_id = InstallPolicyApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});

  if (ExpectUpdateAllowed()) {
    // Name should have updated (because the flag is enabled).
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpdated);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 1);
    EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id),
              "Different app name");
  } else {
    // Name should not have updated (because the flag is missing).
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpToDate);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 0);
    EXPECT_EQ(GetProvider().registrar().GetAppShortName(app_id),
              "Test app name");
  }
}

INSTANTIATE_TEST_SUITE_P(PolicyAppParameterizedTest,
                         ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                         ::testing::Values(true, false));

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDisplayChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"minimal-ui", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"standalone", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppDisplayMode(app_id),
            DisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDisplayBrowserChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"standalone", kInstallableIconList});
  AppId app_id = InstallWebApp();
  GetProvider().sync_bridge().SetAppUserDisplayMode(
      app_id, DisplayMode::kStandalone, /*is_user_action=*/false);

  OverrideManifest(kManifestTemplate, {"browser", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppDisplayMode(app_id),
            DisplayMode::kBrowser);

  // We don't touch the user's launch preference even if the app display mode
  // changes. Instead the effective display mode changes.
  EXPECT_EQ(GetProvider().registrar().GetAppUserDisplayMode(app_id),
            DisplayMode::kStandalone);
  EXPECT_EQ(GetProvider().registrar().GetAppEffectiveDisplayMode(app_id),
            DisplayMode::kMinimalUi);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDisplayOverrideChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": $1,
      "icons": $2
    }
  )";

  OverrideManifest(kManifestTemplate,
                   {R"([ "fullscreen", "standalone" ])", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {R"([ "fullscreen", "minimal-ui" ])", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(2u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kFullscreen, app_display_mode_override[0]);
  EXPECT_EQ(DisplayMode::kMinimalUi, app_display_mode_override[1]);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsNewDisplayOverride) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      $1
      "icons": $2
    }
  )";

  // No display_override in manifest
  OverrideManifest(kManifestTemplate, {"", kInstallableIconList});
  AppId app_id = InstallWebApp();

  // Add display_override field
  OverrideManifest(kManifestTemplate,
                   {R"("display_override": [ "minimal-ui", "standalone" ],)",
                    kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(2u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, app_display_mode_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, app_display_mode_override[1]);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDeletedDisplayOverride) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      $1
      "icons": $2
    }
  )";

  // Ensure display_override exists in initial manifest
  OverrideManifest(kManifestTemplate,
                   {R"("display_override": [ "fullscreen", "minimal-ui" ],)",
                    kInstallableIconList});
  AppId app_id = InstallWebApp();

  // Remove display_override from manifest
  OverrideManifest(kManifestTemplate, {"", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(0u, app_display_mode_override.size());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsInvalidDisplayOverride) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": $1,
      "icons": $2
    }
  )";

  OverrideManifest(kManifestTemplate,
                   {R"([ "browser", "fullscreen" ])", kInstallableIconList});
  AppId app_id = InstallWebApp();

  ASSERT_EQ(2u,
            GetProvider().registrar().GetAppDisplayModeOverride(app_id).size());

  // display_override contains only invalid values
  OverrideManifest(kManifestTemplate,
                   {R"( [ "invalid", 7 ])", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(0u, app_display_mode_override.size());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresDisplayOverrideInvalidChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      $1
      "icons": $2
    }
  )";

  // No display_override in manifest
  OverrideManifest(kManifestTemplate, {"", kInstallableIconList});
  AppId app_id = InstallWebApp();

  // display_override contains only invalid values
  OverrideManifest(
      kManifestTemplate,
      {R"("display_override": [ "invalid", 7 ],)", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresDisplayOverrideChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": $1,
      "icons": $2
    }
  )";

  OverrideManifest(kManifestTemplate,
                   {R"([ "standard", "fullscreen" ])", kInstallableIconList});
  AppId app_id = InstallWebApp();

  // display_override contains an additional invalid value
  OverrideManifest(
      kManifestTemplate,
      {R"([ "invalid", "standard", "fullscreen" ])", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesNotFindIconContentChange) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/basic-192.png?ignore",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});
  AppId app_id = InstallWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
  CheckShortcutInfoUpdated(app_id, kBasicIconTopLeftColor);

  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/192), SK_ColorBLACK);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesNotUpdateGeneratedIcons) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {"[]"});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
}

enum class UpdateDialogParam {
  kDisabled = 0,
  kEnabled = 1,
};

class ManifestUpdateManagerBrowserTest_UpdateDialog
    : public ManifestUpdateManagerBrowserTest,
      public testing::WithParamInterface<UpdateDialogParam> {
 public:
  ManifestUpdateManagerBrowserTest_UpdateDialog() {
    if (IsUpdateDialogEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPwaUpdateDialogForNameAndIcon);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kPwaUpdateDialogForNameAndIcon);
    }
  }

  bool IsUpdateDialogEnabled() const {
    return GetParam() == UpdateDialogParam::kEnabled;
  }

  static std::string ParamToString(
      testing::TestParamInfo<UpdateDialogParam> param) {
    switch (param.param) {
      case UpdateDialogParam::kDisabled:
        return "UpdateDialogDisabled";
      case UpdateDialogParam::kEnabled:
        return "UpdateDialogEnabled";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_UpdateDialog,
                       CheckDoesNotUpdateGeneratedIcons_SyncFailure) {
  // The first "name" character is used to generate icons. Make it like a space
  // to probe the background color at the center. Spaces are trimmed by the
  // parser.
  constexpr char kManifest[] = R"(
    {
      "name": "_Test App Name",
      "start_url": "manifest_test_page.html",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/blue-192.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});

  AppId app_id;

  // Make blue-192.png fail to download for the first sync install..
  {
    std::unique_ptr<content::URLLoaderInterceptor> url_interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            http_server_.GetURL("/web_apps/blue-192.png"),
            net::Error::ERR_FILE_NOT_FOUND);

    app_id = InstallWebAppFromSync(GetAppURL());
  }

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_TRUE(web_app->is_generated_icon());

  // ManifestUpdateManager updates only locally installed apps. Installs web app
  // locally on Win/Mac/Linux.
  if (!web_app->is_locally_installed())
    InstallAppLocally(web_app);

  // Autogenerated icons in `ResizeIconsAndGenerateMissing()` use hardcoded dark
  // gray color as background.
  EXPECT_EQ(6u, web_app->downloaded_icon_sizes(IconPurpose::ANY).size());
  for (SquareSizePx size_px :
       web_app->downloaded_icon_sizes(IconPurpose::ANY)) {
    SCOPED_TRACE(size_px);
    EXPECT_EQ(color_utils::SkColorToRgbaString(ReadAppIconPixel(
                  app_id, size_px, /*x=*/size_px / 2, /*y=*/size_px / 2)),
              color_utils::SkColorToRgbaString(SK_ColorDKGRAY));
  }

  if (IsUpdateDialogEnabled())
    chrome::SetAutoAcceptAppIdentityUpdateForTesting(true);

  OverrideManifest(kManifest, {});

  ManifestUpdateResult update_result = GetResultAfterPageLoad(GetAppURL());

  EXPECT_EQ(update_result, ManifestUpdateResult::kAppUpToDate);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);

  ASSERT_EQ(web_app, GetProvider().registrar().GetAppById(app_id));
  // Still autogenerated icons, no change.
  EXPECT_TRUE(web_app->is_generated_icon());
  // Not 7u, no non-generated icon added.
  EXPECT_EQ(6u, web_app->downloaded_icon_sizes(IconPurpose::ANY).size());
  // Not SK_ColorBLUE for blue-192.png.
  for (SquareSizePx size_px :
       web_app->downloaded_icon_sizes(IconPurpose::ANY)) {
    SCOPED_TRACE(size_px);
    EXPECT_EQ(color_utils::SkColorToRgbaString(ReadAppIconPixel(
                  app_id, size_px, /*x=*/size_px / 2, /*y=*/size_px / 2)),
              color_utils::SkColorToRgbaString(SK_ColorDKGRAY));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManifestUpdateManagerBrowserTest_UpdateDialog,
    ::testing::Values(UpdateDialogParam::kEnabled,
                      UpdateDialogParam::kDisabled),
    ManifestUpdateManagerBrowserTest_UpdateDialog::ParamToString);

class ManifestUpdateManagerCaptureLinksBrowserTest
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppEnableLinkCapturing};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerCaptureLinksBrowserTest,
                       CheckFindsCaptureLinksChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "capture_links": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "none"});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppCaptureLinks(app_id),
            blink::mojom::CaptureLinks::kNone);

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "new-client"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppCaptureLinks(app_id),
            blink::mojom::CaptureLinks::kNewClient);
}

class ManifestUpdateManagerLaunchHandlerBrowserTest
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppEnableLaunchHandler};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerLaunchHandlerBrowserTest,
                       CheckFindsLaunchHandlerChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "launch_handler": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "null"});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppById(app_id)->launch_handler(),
            absl::nullopt);

  OverrideManifest(kManifestTemplate, {kInstallableIconList, R"({
    "route_to": "existing-client",
    "navigate_existing_client": "never"
  })"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppById(app_id)->launch_handler(),
            (LaunchHandler{LaunchHandler::RouteTo::kExistingClient,
                           LaunchHandler::NavigateExistingClient::kNever}));
}

class ManifestUpdateManagerSystemAppBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerSystemAppBrowserTest()
      : system_app_(
            TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    EnableSystemWebAppsInLacrosForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void SetUpOnMainThread() override { system_app_->WaitForAppInstall(); }

 protected:
  std::unique_ptr<TestSystemWebAppInstallation> system_app_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerSystemAppBrowserTest,
                       CheckUpdateSkipped) {
  AppId app_id = system_app_->GetAppId();
  EXPECT_EQ(GetResultAfterPageLoad(system_app_->GetAppUrl()),
            ManifestUpdateResult::kAppIsSystemWebApp);

  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsSystemWebApp, 1);
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id), SK_ColorGREEN);
}

using ManifestUpdateManagerWebAppsBrowserTest =
    ManifestUpdateManagerBrowserTest;

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerWebAppsBrowserTest,
                       CheckFindsAddedShareTarget) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kShareTargetManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "share_target": {
        "action": "/web_share_target/share.html",
        "method": "GET",
        "params": {
          "url": "link"
        }
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kShareTargetManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_TRUE(web_app->share_target().has_value());
  EXPECT_EQ(web_app->share_target()->method, apps::ShareTarget::Method::kGet);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerWebAppsBrowserTest,
                       CheckFindsShareTargetChange) {
  constexpr char kShareTargetManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "share_target": {
        "action": "/web_share_target/share.html",
        "method": "$1",
        "params": {
          "url": "link"
        }
      },
      "icons": $2
    }
  )";
  OverrideManifest(kShareTargetManifestTemplate, {"GET", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kShareTargetManifestTemplate,
                   {"POST", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_TRUE(web_app->share_target().has_value());
  EXPECT_EQ(web_app->share_target()->method, apps::ShareTarget::Method::kPost);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerWebAppsBrowserTest,
                       CheckFindsDeletedShareTarget) {
  constexpr char kShareTargetManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "share_target": {
        "action": "/web_share_target/share.html",
        "method": "GET",
        "params": {
          "url": "link"
        }
      },
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kShareTargetManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_FALSE(web_app->share_target().has_value());
}

// Functional tests. More tests for detecting file handler updates are
// available in unit tests at ManifestUpdateTaskTest.
class ManifestUpdateManagerBrowserTestWithFileHandling
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kFileHandlingAPI};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFindsAddedFileHandler) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_FALSE(web_app->file_handlers().empty());
  const auto& file_handler = web_app->file_handlers()[0];
  EXPECT_EQ("plaintext", file_handler.action.query());
  EXPECT_EQ(1u, file_handler.accept.size());
  EXPECT_EQ("text/plain", file_handler.accept[0].mime_type);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckIgnoresUnchangedFileHandler) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_FALSE(web_app->file_handlers().empty());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFindsChangedFileExtension) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": ["$1"]
          }
        }
      ],
      "icons": $2
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate,
                   {".txt", kInstallableIconList});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  const auto& old_file_handler = web_app->file_handlers()[0];
  EXPECT_EQ(1u, old_file_handler.accept.size());
  auto old_extensions = old_file_handler.accept[0].file_extensions;
  EXPECT_EQ(1u, old_extensions.size());
  EXPECT_TRUE(base::Contains(old_extensions, ".txt"));

  OverrideManifest(kFileHandlerManifestTemplate, {".md", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const auto& new_file_handler = web_app->file_handlers()[0];
  EXPECT_EQ(1u, new_file_handler.accept.size());
  auto new_extensions = new_file_handler.accept[0].file_extensions;
  EXPECT_EQ(1u, new_extensions.size());
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       FileHandlingPermissionResetsOnUpdate) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": ["$1"]
          }
        }
      ],
      "icons": [
        {
          "src": "launcher-icon-4x.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ],
      "theme_color": "$2"
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {".txt", "red"});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  const auto& old_file_handler = web_app->file_handlers()[0];
  auto old_extensions = old_file_handler.accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(old_extensions, ".txt"));
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  const GURL url = GetAppURL();
  const GURL origin = url.GetOrigin();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  // Set permission to ALLOW.
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  // Update manifest, adding an extension to the file handler. Permission should
  // be downgraded to ASK. The time override is necessary to make sure the
  // manifest update isn't skipped due to throttling.
  base::Time time_override = base::Time::Now();
  SetTimeOverride(time_override);
  OverrideManifest(kFileHandlerManifestTemplate, {".md\", \".txt", "red"});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  auto new_extensions = web_app->file_handlers()[0].accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));
  EXPECT_TRUE(base::Contains(new_extensions, ".txt"));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Set permission to ALLOW.
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);

  // Update manifest, but keep same file handlers. Permission should be left on
  // ALLOW.
  time_override += base::Days(10);
  SetTimeOverride(time_override);
  OverrideManifest(kFileHandlerManifestTemplate, {".md\", \".txt", "blue"});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  new_extensions = web_app->file_handlers()[0].accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));
  EXPECT_TRUE(base::Contains(new_extensions, ".txt"));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Update manifest, asking for /fewer/ file types. Permission should be left
  // on ALLOW.
  time_override += base::Days(10);
  SetTimeOverride(time_override);
  OverrideManifest(kFileHandlerManifestTemplate, {".txt", "blue"});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  new_extensions = web_app->file_handlers()[0].accept[0].file_extensions;
  EXPECT_FALSE(base::Contains(new_extensions, ".md"));
  EXPECT_TRUE(base::Contains(new_extensions, ".txt"));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

#if defined(OS_LINUX)
  // Make sure that blocking the permission also unregisters the MIME type on
  // Linux.
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(base::BindLambdaForTesting(
      [](base::FilePath filename, std::string file_contents) {
        EXPECT_TRUE(file_contents.empty());
        return true;
      }));
#endif

  // Block the permission, update manifest, permission should still be block.
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_BLOCK);
  OverrideManifest(kFileHandlerManifestTemplate, {".txt", "red"});
  time_override += base::Days(10);
  SetTimeOverride(time_override);
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       BlockPermissionRemoveFileHandlers) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": ["$1"]
          }
        }
      ],
      "icons": $2
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate,
                   {".txt", kInstallableIconList});
  AppId app_id = InstallWebApp();
  WebAppRegistrar& registrar = GetProvider().registrar();
  const WebApp* web_app = registrar.GetAppById(app_id);

  EXPECT_FALSE(web_app->file_handler_permission_blocked());
  ASSERT_FALSE(web_app->file_handlers().empty());
  const auto& old_file_handler = web_app->file_handlers()[0];
  ASSERT_FALSE(old_file_handler.accept.empty());
  auto old_extensions = old_file_handler.accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(old_extensions, ".txt"));
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  const GURL url = GetAppURL();
  const GURL origin = url.GetOrigin();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  // Set permission to BLOCK.
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // App should be updated to permission blocked by
  // `WebAppInstallFinalizer::OnContentSettingChanged`.
  EXPECT_TRUE(registrar.GetAppById(app_id)->file_handler_permission_blocked());
  // Update manifest.
  OverrideManifest(kFileHandlerManifestTemplate, {".md", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  // Manifest update task should preserve the permission blocked state.
  EXPECT_TRUE(registrar.GetAppById(app_id)->file_handler_permission_blocked());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFindsDeletedFileHandler) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_TRUE(web_app->file_handlers().empty());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFileExtensionList) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  InstallWebApp();

  std::u16string associations_list =
      GetFileTypeAssociationsHandledByWebAppsForDisplay(browser()->profile(),
                                                        GetAppURL());
#if defined(OS_LINUX)
  EXPECT_EQ(u"text/plain", associations_list);
#else
  EXPECT_EQ(u"TXT", associations_list);
#endif  // defined(OS_LINUX)
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFileExtensionsList) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt", ".md"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  InstallWebApp();

  std::u16string associations_list =
      GetFileTypeAssociationsHandledByWebAppsForDisplay(browser()->profile(),
                                                        GetAppURL());
#if defined(OS_LINUX)
  EXPECT_EQ(u"text/plain", associations_list);
#else
  EXPECT_EQ(u"MD, TXT", associations_list);
#endif  // defined(OS_LINUX)
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFileExtensionsListWithTwoFileHandlers) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        },
        {
          "action": "/?longtype",
          "name": "Long Custom type",
          "accept": {
            "application/long-type": [".longtype"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  InstallWebApp();

  std::u16string associations_list =
      GetFileTypeAssociationsHandledByWebAppsForDisplay(browser()->profile(),
                                                        GetAppURL());
#if defined(OS_LINUX)
  EXPECT_EQ(u"application/long-type, text/plain", associations_list);
#else
  EXPECT_EQ(u"LONGTYPE, TXT", associations_list);
#endif  // defined(OS_LINUX)
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutsMenuUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, kShortcutsItem});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList, kShortcutsItems});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(
      GetProvider().registrar().GetAppShortcutsMenuItemInfos(app_id).size(),
      2u);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsItemNameUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "$2",
          "short_name": "HM",
          "description": "Go home",
          "url": ".",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "Home"});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, kAnotherShortcutsItemName});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(
      GetProvider().registrar().GetAppShortcutsMenuItemInfos(app_id)[0].name,
      kAnotherShortcutsItemName16);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresShortNameAndDescriptionChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "$2",
          "description": "$3",
          "url": ".",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "HM", "Go home"});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, kAnotherShortcutsItemShortName,
                    kAnotherShortcutsItemDescription});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsItemUrlUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": "$2",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "/"});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, kAnotherShortcutsItemUrl});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(
      GetProvider().registrar().GetAppShortcutsMenuItemInfos(app_id)[0].url,
      http_server_.GetURL(kAnotherShortcutsItemUrl));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutIconContentChange) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": "/",
          "icons": [
            {
              "src": "/web_apps/basic-192.png?ignore",
              "sizes": "192x192",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  // Check that the installed icon is now blue.
  base::RunLoop run_loop;
  GetProvider().icon_manager().ReadAllShortcutsMenuIcons(
      app_id,
      base::BindLambdaForTesting(
          [&run_loop](ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
            run_loop.Quit();
            EXPECT_EQ(shortcuts_menu_icon_bitmaps[0].any.at(192).getColor(0, 0),
                      SK_ColorBLUE);
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       ShortcutIconContentChangeDoesNotApplyAppIconUpdate) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": "/",
          "icons": [
            {
              "src": "/web_apps/basic-192.png?ignore",
              "sizes": "192x192",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  OverrideManifest(kManifest, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  // The icon should not be updated.
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutIconSrcUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": ".",
          "icons": [
            {
              "src": "$2",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, "/banners/image-512px.png"});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList, kAnotherIconSrc});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider()
                .registrar()
                .GetAppShortcutsMenuItemInfos(app_id)[0]
                .GetShortcutIconInfosForPurpose(IconPurpose::ANY)[0]
                .url,
            http_server_.GetURL(kAnotherIconSrc));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutIconSizesUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": ".",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "$2",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "512x512"});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList,
                    gfx::Size(kAnotherIconSize, kAnotherIconSize).ToString()});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider()
                .registrar()
                .GetAppShortcutsMenuItemInfos(app_id)[0]
                .GetShortcutIconInfosForPurpose(IconPurpose::ANY)[0]
                .square_size_px,
            kAnotherIconSize);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckUpdateTimeChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  base::Time manifest_update_time = web_app->manifest_update_time();

  // CSS #RRGGBBAA syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "#00FF00F0"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);

  // Update time is updated.
  EXPECT_LT(manifest_update_time, web_app->manifest_update_time());
}

class ManifestUpdateManagerIconUpdatingBrowserTest
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebAppManifestIconUpdating};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerIconUpdatingBrowserTest,
                       CheckFindsIconContentChange) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/basic-192.png?ignore",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});
  AppId app_id = InstallWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // The icon should have changed, as the file has been updated (but the url is
  // the same).
  CheckShortcutInfoUpdated(app_id, SK_ColorBLUE);

  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/192), SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerIconUpdatingBrowserTest,
                       CheckFindsIconUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // The icon should have changed.
  CheckShortcutInfoUpdated(app_id, kAnotherInstallableIconTopLeftColor);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerIconUpdatingBrowserTest,
                       CheckIgnoresIconDownloadFail) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/basic-48.png?ignore",
          "sizes": "48x48",
          "type": "image/png"
        },
        {
          "src": "/web_apps/basic-192.png?ignore",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});
  AppId app_id = InstallWebApp();

  // Make basic-48.png fail to download.
  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-48.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse("malformed response", "",
                                                       params->client.get());
          return true;
        }
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kIconDownloadFailed);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kIconDownloadFailed, 1);

  // Since one request failed, none of the icons should be updated. So the '192'
  // size here is not updated to blue.
  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/48), SK_ColorBLACK);
  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/192), SK_ColorBLACK);
}

class ManifestUpdateManagerBrowserTest_UrlHandlers
    : public ManifestUpdateManagerBrowserTest {
 public:
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  void SetUpUrlHandlerManager() {
    auto url_handler_manager =
        std::make_unique<UrlHandlerManagerImpl>(browser()->profile());

    // Set up web app origin association manager and expected data.
    auto association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    url::Origin foo_origin = url::Origin::Create(GURL("https://foo.com"));
    url::Origin bar_origin = url::Origin::Create(GURL("https://bar.com"));
    std::map<apps::UrlHandlerInfo, apps::UrlHandlerInfo> data = {
        {apps::UrlHandlerInfo(foo_origin),
         apps::UrlHandlerInfo(foo_origin, false, /*paths*/ {},
                              /*exclude_paths*/ {"/exclude"})},
        {apps::UrlHandlerInfo(bar_origin),
         apps::UrlHandlerInfo(bar_origin, false, /*paths*/ {},
                              /*exclude_paths*/ {"/exclude"})},
    };
    association_manager->SetData(std::move(data));

    url_handler_manager->SetAssociationManagerForTesting(
        std::move(association_manager));
    url_handler_manager->SetSubsystems(&(GetProvider().registrar()));
    GetProvider().os_integration_manager().set_url_handler_manager(
        std::move(url_handler_manager));
  }
#endif

  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppEnableUrlHandlers};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_UrlHandlers,
                       UpdateWithDifferentUrlHandlers) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "url_handlers": [$2]
    }
  )";

  OverrideManifest(kManifestTemplate, {
                                          kInstallableIconList,
                                          R"({"origin": "https://foo.com"})",
                                      });
  AppId app_id = InstallWebApp();
  ASSERT_EQ(1u, GetProvider().registrar().GetAppUrlHandlers(app_id).size());

  OverrideManifest(
      kManifestTemplate,
      {kInstallableIconList,
       R"({"origin": "https://foo.com"}, {"origin": "https://bar.com"})"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);

  apps::UrlHandlers url_handlers =
      GetProvider().registrar().GetAppUrlHandlers(app_id);
  ASSERT_EQ(2u, url_handlers.size());
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://foo.com")))));
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://bar.com")))));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_UrlHandlers,
                       UpdateFromNoUrlHandlersToHaveUrlHandlers) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
      $2
    }
  )";

  OverrideManifest(kManifestTemplate, {
                                          kInstallableIconList,
                                          R"()",
                                      });
  AppId app_id = InstallWebApp();
  ASSERT_EQ(0u, GetProvider().registrar().GetAppUrlHandlers(app_id).size());

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList,
                    R"(,"url_handlers": [{"origin": "https://foo.com"}])"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);

  apps::UrlHandlers url_handlers =
      GetProvider().registrar().GetAppUrlHandlers(app_id);
  ASSERT_EQ(1u, url_handlers.size());
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://foo.com")))));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_UrlHandlers,
                       UpdateFromUrlHandlersToNoUrlHandlers) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
      $2
    }
  )";

  OverrideManifest(kManifestTemplate,
                   {
                       kInstallableIconList,
                       R"(,"url_handlers": [{"origin": "https://foo.com"}])",
                   });
  AppId app_id = InstallWebApp();
  apps::UrlHandlers url_handlers =
      GetProvider().registrar().GetAppUrlHandlers(app_id);
  ASSERT_EQ(1u, url_handlers.size());
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://foo.com")))));

  OverrideManifest(kManifestTemplate, {kInstallableIconList, R"()"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);

  url_handlers = GetProvider().registrar().GetAppUrlHandlers(app_id);
  ASSERT_EQ(0u, url_handlers.size());
}

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_UrlHandlers,
                       NoHandlersChangeUpdateAssociations) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "url_handlers": [
        {
          "origin": "https://foo.com"
        },
        {
          "origin": "https://bar.com"
        }
      ]
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();
  apps::UrlHandlers url_handlers =
      GetProvider().registrar().GetAppUrlHandlers(app_id);
  ASSERT_EQ(2u, url_handlers.size());
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://foo.com")))));
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://bar.com")))));

  // Prepare for association fetching at manifest update.
  SetUpUrlHandlerManager();
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppAssociationsUpdated);

  // Verify url handlers are saved to prefs.
  base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg("https://foo.com/ok");
  std::vector<UrlHandlerLaunchParams> matches =
      UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  ASSERT_EQ(matches.size(), 1u);
  // Check exclude paths came through.
  cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg("https://foo.com/exclude");
  matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  ASSERT_EQ(matches.size(), 0u);

  cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg("https://bar.com/ok");
  matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  ASSERT_EQ(matches.size(), 1u);
  // Check exclude paths came through.
  cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg("https://bar.com/exclude");
  matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  ASSERT_EQ(matches.size(), 0u);
}
#endif

class ManifestUpdateManagerBrowserTestWithProtocolHandling
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppEnableProtocolHandlers};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithProtocolHandling,
                       CheckFindsAddedProtocolHandler) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "mailto",
          "url": "?mailto=%s"
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_FALSE(web_app->protocol_handlers().empty());
  const auto& protocol_handler = web_app->protocol_handlers()[0];
  EXPECT_EQ("mailto", protocol_handler.protocol);
  EXPECT_EQ(http_server_.GetURL("/banners/manifest.json?mailto=%s"),
            protocol_handler.url.spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithProtocolHandling,
                       CheckIgnoresUnchangedProtocolHandler) {
  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "mailto",
          "url": "?mailto=%s"
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_FALSE(web_app->protocol_handlers().empty());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithProtocolHandling,
                       CheckFindsChangedProtocolHandler) {
  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "$1",
          "url": "?$2=%s"
        }
      ],
      "icons": $3
    }
  )";

  OverrideManifest(kProtocolHandlerManifestTemplate,
                   {"mailto", "mailto", kInstallableIconList});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_EQ(1u, web_app->protocol_handlers().size());
  const auto& old_protocol_handler = web_app->protocol_handlers()[0];
  EXPECT_EQ("mailto", old_protocol_handler.protocol);
  EXPECT_EQ(http_server_.GetURL("/banners/manifest.json?mailto=%s"),
            old_protocol_handler.url.spec());

  OverrideManifest(kProtocolHandlerManifestTemplate,
                   {"web+mailto", "web+mailto", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  EXPECT_EQ(1u, web_app->protocol_handlers().size());
  const auto& new_protocol_handler = web_app->protocol_handlers()[0];
  EXPECT_EQ("web+mailto", new_protocol_handler.protocol);
  EXPECT_EQ(http_server_.GetURL("/banners/manifest.json?web+mailto=%s"),
            new_protocol_handler.url.spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithProtocolHandling,
                       CheckFindsDeletedProtocolHandler) {
  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "mailto",
          "url": "?mailto=%s"
        }
      ],
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_TRUE(web_app->protocol_handlers().empty());
}

class ManifestUpdateManagerBrowserTestWithWebAppNoteTaking
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppNoteTaking};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithWebAppNoteTaking,
                       CheckFindsAddedNewNoteUrl) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "/new"
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_TRUE(web_app->note_taking_new_note_url().is_empty());

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(http_server_.GetURL("/new"),
            web_app->note_taking_new_note_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithWebAppNoteTaking,
                       CheckIgnoresUnchangedNewNoteUrl) {
  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "/new"
      },
      "icons": $1
    }
  )";

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_EQ(http_server_.GetURL("/new"),
            web_app->note_taking_new_note_url().spec());

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
  EXPECT_EQ(http_server_.GetURL("/new"),
            web_app->note_taking_new_note_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithWebAppNoteTaking,
                       CheckFindsChangedNewNoteUrl) {
  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "$1"
      },
      "icons": $2
    }
  )";

  OverrideManifest(kNewNoteUrlManifestTemplate,
                   {"old-relative-url", kInstallableIconList});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  // URL parsed relative to manifest URL, which is in /banners/.
  EXPECT_EQ(http_server_.GetURL("/banners/old-relative-url"),
            web_app->note_taking_new_note_url().spec());

  OverrideManifest(kNewNoteUrlManifestTemplate,
                   {"/newer", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(http_server_.GetURL("/newer"),
            web_app->note_taking_new_note_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithWebAppNoteTaking,
                       CheckFindsDeletedNewNoteUrl) {
  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "/new"
      },
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar().GetAppById(app_id);
  EXPECT_FALSE(web_app->note_taking_new_note_url().is_empty());

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_TRUE(web_app->note_taking_new_note_url().is_empty());
}

class ManifestUpdateManagerBrowserTest_ManifestId
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest_ManifestId() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableManifestId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ManifestId,
                       AllowStartUrlUpdate) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": "$1",
      "scope": "/",
      "display": "minimal-ui",
      "id": "test",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/startA", kInstallableIconList});
  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar().GetAppStartUrl(app_id).path(), "/startA");

  OverrideManifest(kManifestTemplate, {"/startB", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  EXPECT_EQ(GetProvider().registrar().GetAppStartUrl(app_id).path(), "/startB");
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ManifestId,
                       CheckIgnoresIdChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "id": "$1",
      "start_url": "start",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"test", kInstallableIconList});
  AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"testb", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppIdMismatch);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppIdMismatch, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ManifestId,
                       ChecksSettingIdMatchDefault) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": "/start",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  AppId app_id = InstallWebApp();

  // manifest_id should default to start_url when it's not provided in manifest.
  EXPECT_EQ(GetProvider().registrar().GetAppById(app_id)->manifest_id().value(),
            "start");

  constexpr char kManifestTemplate2[] = R"(
    {
      "name": "Test app name",
      "id": "$1",
      "start_url": "/start",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";

  // Setting manifest id to match default value won't trigger update as the
  // parsed manifest is the same.
  OverrideManifest(kManifestTemplate2, {"start", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ManifestId,
                       AllowManifestIdUpdateWhenAppIdIsNotChanged) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "id": "$1",
      "start_url": "/start",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"start", kInstallableIconList});
  AppId app_id = InstallWebApp();
  // Manually set manifest_id to null. manifest_id can be null when the
  // kWebAppEnableManifestId is turnned off or when the app is sync installed
  // from older versions of Chromium.
  {
    ScopedRegistryUpdate update(&GetProvider().sync_bridge());
    WebApp* app = update->UpdateApp(app_id);
    app->SetManifestId(absl::nullopt);
  }
  EXPECT_FALSE(
      GetProvider().registrar().GetAppById(app_id)->manifest_id().has_value());
  // Reload page to trigger an manifest update that re-fetches the manifest with
  // id specified to be same as the default start_url.
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  EXPECT_TRUE(
      GetProvider().registrar().GetAppById(app_id)->manifest_id().has_value());
}

// Test that showing the AppIdentity update confirmation and allowing the update
// sends the right signal back.
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerAppIdentityBrowserTest,
                       VerifyCallbackUpgradeAllowed) {
  chrome::SetAutoAcceptAppIdentityUpdateForTesting(true);

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  AppId app_id = InstallWebApp();

  base::RunLoop run_loop;

  chrome::ShowWebAppIdentityUpdateDialog(
      app_id,
      /* title_change= */ true,
      /* icon_change= */ false, u"old_title", u"new_title",
      /* old_icon= */ SkBitmap(),
      /* new_icon= */ SkBitmap(),
      browser()->tab_strip_model()->GetActiveWebContents(),
      /* callback= */
      base::BindLambdaForTesting(
          [&](AppIdentityUpdate app_identity_update_allowed) {
            // This verifies that the dialog sends us the signal to update.
            DCHECK_EQ(AppIdentityUpdate::kAllowed, app_identity_update_allowed);
            run_loop.Quit();
          }));

  run_loop.Run();
}

}  // namespace web_app
