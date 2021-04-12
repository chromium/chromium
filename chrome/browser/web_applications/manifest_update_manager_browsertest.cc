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
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkColor.h"

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "base/command_line.h"
#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
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
  return WebAppProviderBase::GetProviderBase(browser->profile())
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
  base::Optional<ManifestUpdateResult> result_;
};

}  // namespace

class ManifestUpdateManagerBrowserTest : public InProcessBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest() {}
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
    ui_test_utils::NavigateToURL(browser(), app_url);

    AppId app_id;
    base::RunLoop run_loop;
    GetProvider().install_manager().InstallWebAppFromManifestWithFallback(
        browser()->tab_strip_model()->GetActiveWebContents(),
        /*force_shortcut_app=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindOnce(TestAcceptDialogCallback),
        base::BindLambdaForTesting(
            [&](const AppId& new_app_id, InstallResultCode code) {
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = new_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();
    return app_id;
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
    GetProvider().pending_app_manager().Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& installed_app_url,
                PendingAppManager::InstallResult result) {
              EXPECT_EQ(installed_app_url, app_url);
              EXPECT_EQ(result.code, InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();
    return GetProvider().registrar().LookupExternalAppId(app_url).value();
  }

  void SetTimeOverride(base::Time time_override) {
    GetManifestUpdateManager(browser()).set_time_override_for_testing(
        time_override);
  }

  ManifestUpdateResult GetResultAfterPageLoad(const GURL& url,
                                              const AppId* app_id) {
    UpdateCheckResultAwaiter awaiter(browser(), url);
    ui_test_utils::NavigateToURL(browser(), url);
    return std::move(awaiter).AwaitNextResult();
  }

  WebAppProviderBase& GetProvider() {
    return *WebAppProviderBase::GetProviderBase(browser()->profile());
  }

 protected:
  net::EmbeddedTestServer::HandleRequestCallback request_override_;

  base::HistogramTester histogram_tester_;

  net::EmbeddedTestServer http_server_;

 private:
  base::Optional<base::RunLoop> shortcut_run_loop_;
  base::Optional<SkColor> updated_shortcut_top_left_color_;
  ScopedOsHooksSuppress os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckOutOfScopeNavigation) {
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), nullptr),
            ManifestUpdateResult::kNoAppInScope);

  AppId app_id = InstallWebApp();

  EXPECT_EQ(GetResultAfterPageLoad(GURL("http://example.org"), nullptr),
            ManifestUpdateResult::kNoAppInScope);

  histogram_tester_.ExpectTotalCount(kUpdateHistogramName, 0);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest, CheckIsThrottled) {
  constexpr base::TimeDelta kDelayBetweenChecks = base::TimeDelta::FromDays(1);
  base::Time time_override = base::Time::UnixEpoch();
  SetTimeOverride(time_override);

  AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks * 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  ui_test_utils::NavigateToURL(browser(), url);
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
  UpdateCheckResultAwaiter awaiter(browser(), url);
  ui_test_utils::NavigateToURL(browser(), url);
  GetProvider().install_finalizer().UninstallExternalAppByUser(
      app_id, base::DoNothing());
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUninstalled);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUninstalled, 1);
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

  GetProvider().registry_controller().SetAppIsLocallyInstalled(app_id, false);
  EXPECT_FALSE(GetProvider().registrar().IsLocallyInstalled(app_id));

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

  // Install via PendingAppManager, the redirect to a different origin should
  // cause it to install a placeholder app.
  AppId app_id = InstallPolicyApp();
  EXPECT_TRUE(GetProvider().registrar().IsPlaceholderApp(app_id));

  // Manifest updating should ignore non-redirect loads for placeholder apps
  // because the PendingAppManager will handle these.
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
  EXPECT_EQ(GetResultAfterPageLoad(app_url, &app_id),
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
  WebAppInstallObserver install_observer(&GetProvider().registrar());
  install_observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([](const AppId& app_id) { NOTREACHED(); }));
  install_observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([](const AppId& app_id) { NOTREACHED(); }));

  // CSS #RRGGBBAA syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "#00FF00F0"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  // Updated theme_color loses any transparency.
  EXPECT_EQ(GetProvider().registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0x00, 0xFF, 0x00));
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
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
      GetProvider().install_finalizer().CanUserUninstallExternalApp(app_id));

  OverrideManifest(kManifestTemplate, {"red", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);

  // Policy installed apps should continue to be not uninstallable by the user
  // after updating.
  EXPECT_FALSE(
      GetProvider().install_finalizer().CanUserUninstallExternalApp(app_id));
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // The icon should not be updated.
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  GetProvider().registry_controller().SetAppUserDisplayMode(
      app_id, DisplayMode::kStandalone, /*is_user_action=*/false);

  OverrideManifest(kManifestTemplate, {"browser", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
  CheckShortcutInfoUpdated(app_id, kBasicIconTopLeftColor);

  EXPECT_EQ(ReadAppIconPixel(browser()->profile(), app_id, /*size=*/192,
                             /*x=*/0, /*y=*/0),
            SK_ColorBLACK);
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
}

class ManifestUpdateManagerCaptureLinksBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerCaptureLinksBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableLinkCapturing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  EXPECT_EQ(GetProvider().registrar().GetAppCaptureLinks(app_id),
            blink::mojom::CaptureLinks::kNewClient);
}

class ManifestUpdateManagerSystemAppBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerSystemAppBrowserTest()
      : system_app_(
            TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp()) {}

  void SetUpOnMainThread() override { system_app_->WaitForAppInstall(); }

 protected:
  std::unique_ptr<TestSystemWebAppInstallation> system_app_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerSystemAppBrowserTest,
                       CheckUpdateSkipped) {
  AppId app_id = system_app_->GetAppId();
  EXPECT_EQ(GetResultAfterPageLoad(system_app_->GetAppUrl(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
  EXPECT_FALSE(web_app->share_target().has_value());
}

// Functional tests. More tests for detecting file handler updates are
// available in unit tests at ManifestUpdateTaskTest.
class ManifestUpdateManagerBrowserTestWithFileHandling
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTestWithFileHandling() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kFileHandlingAPI);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
            GetResultAfterPageLoad(GetAppURL(), &app_id));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
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
            GetResultAfterPageLoad(GetAppURL(), &app_id));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);

  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
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
  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
  const auto& old_file_handler = web_app->file_handlers()[0];
  EXPECT_EQ(1u, old_file_handler.accept.size());
  auto old_extensions = old_file_handler.accept[0].file_extensions;
  EXPECT_EQ(1u, old_extensions.size());
  EXPECT_TRUE(base::Contains(old_extensions, ".txt"));

  OverrideManifest(kFileHandlerManifestTemplate, {".md", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL(), &app_id));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const auto& new_file_handler = web_app->file_handlers()[0];
  EXPECT_EQ(1u, new_file_handler.accept.size());
  auto new_extensions = new_file_handler.accept[0].file_extensions;
  EXPECT_EQ(1u, new_extensions.size());
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       AllowedPermissionResetToAskOnUpdate) {
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
  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
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
  // Update manifest.
  OverrideManifest(kFileHandlerManifestTemplate, {".md", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(url, &app_id));
  const auto& new_file_handler = web_app->file_handlers()[0];
  auto new_extensions = new_file_handler.accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));

  // Confirm permission is downgraded to ASK after manifest update.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
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
            GetResultAfterPageLoad(GetAppURL(), &app_id));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app =
      GetProvider().registrar().AsWebAppRegistrar()->GetAppById(app_id);
  EXPECT_TRUE(web_app->file_handlers().empty());
}

class ManifestUpdateManagerBrowserTestWithShortcutsMenu
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTestWithShortcutsMenu() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsAppIconShortcutsMenu);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(
      GetProvider().registrar().GetAppShortcutsMenuItemInfos(app_id).size(),
      2u);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(
      GetProvider().registrar().GetAppShortcutsMenuItemInfos(app_id)[0].name,
      base::UTF8ToUTF16(kAnotherShortcutsItemName));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(
      GetProvider().registrar().GetAppShortcutsMenuItemInfos(app_id)[0].url,
      http_server_.GetURL(kAnotherShortcutsItemUrl));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  // The icon should not be updated.
  CheckShortcutInfoUpdated(app_id, kInstallableIconTopLeftColor);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithShortcutsMenu,
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

class ManifestUpdateManagerIconUpdatingBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerIconUpdatingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebAppManifestIconUpdating);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // The icon should have changed, as the file has been updated (but the url is
  // the same).
  CheckShortcutInfoUpdated(app_id, SK_ColorBLUE);

  EXPECT_EQ(ReadAppIconPixel(browser()->profile(), app_id, /*size=*/192,
                             /*x=*/0, /*y=*/0),
            SK_ColorBLUE);
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
            ManifestUpdateResult::kIconDownloadFailed);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kIconDownloadFailed, 1);

  // Since one request failed, none of the icons should be updated. So the '192'
  // size here is not updated to blue.
  EXPECT_EQ(ReadAppIconPixel(browser()->profile(), app_id, /*size=*/48, /*x=*/0,
                             /*y=*/0),
            SK_ColorBLACK);
  EXPECT_EQ(ReadAppIconPixel(browser()->profile(), app_id, /*size=*/192,
                             /*x=*/0, /*y=*/0),
            SK_ColorBLACK);
}

class ManifestUpdateManagerBrowserTest_UrlHandlers
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest_UrlHandlers() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableUrlHandlers);
  }

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL(), &app_id),
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

}  // namespace web_app
