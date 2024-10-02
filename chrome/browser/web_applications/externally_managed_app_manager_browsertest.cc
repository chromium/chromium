// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/external_app_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/public/cpp/notification.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/message_center.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::Property;
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

class ExternallyManagedAppManagerBrowserTest : public WebAppBrowserTestBase {
 public:
  std::unique_ptr<net::test_server::HttpResponse> SimulateRedirectHandler(
      const net::test_server::HttpRequest& request) {
    if (!simulate_redirect_) {
      // Fall back to default handlers.
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.GetURL().spec().find("redirected") != std::string::npos) {
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->set_content("Redirect successful");
      return response;
    }

    std::string destination = request.GetURL().spec() + "/redirected";
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", destination);
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(base::StringPrintf(
        "<!doctype html><p>Redirecting to %s", destination.c_str()));
    return response;
  }

 protected:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    // Allow different origins to be handled by the embedded_test_server.
    host_resolver()->AddRule("*", "127.0.0.1");
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider());
  }

  Profile* profile() { return browser()->profile(); }

  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  ExternallyManagedAppManager& externally_managed_app_manager() {
    return provider()->externally_managed_app_manager();
  }

  void InstallApp(ExternalInstallOptions install_options) {
    auto result = ExternallyManagedAppManagerInstall(
        profile(), std::move(install_options));
    result_code_ = result.code;
  }

  void CheckServiceWorkerStatus(const GURL& url,
                                content::ServiceWorkerCapability status) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile()));
    content::StoragePartition* storage_partition =
        web_contents->GetBrowserContext()->GetStoragePartition(
            web_contents->GetSiteInstance());
    test::CheckServiceWorkerStatus(url, storage_partition, status);
  }

  std::optional<webapps::InstallResultCode> result_code_;
  bool simulate_redirect_ = false;
};

// Basic integration test to make sure the whole flow works. Each step in the
// flow is unit tested separately.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
}

// If install URL redirects, install should still succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallSucceedsWithRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL start_url =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  GURL install_url =
      embedded_test_server()->GetURL("/server-redirect?" + start_url.spec());
  ExternalInstallOptions install_options(
      install_url, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  InstallApp(install_options);
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
  // Same AppID should be in the registrar using start_url from the manifest.
  // TODO(crbug.com/340952100): Change this to `GetInstallState` and
  // `kInstalledWithOsIntegration` after this install isn't forced to skip OS
  // integration in the finalizer.
  EXPECT_TRUE(registrar().IsInstallState(
      app_id.value(), {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                       proto::INSTALLED_WITH_OS_INTEGRATION}));
  std::optional<webapps::AppId> opt_app_id =
      registrar().FindAppWithUrlInScope(start_url);
  EXPECT_TRUE(opt_app_id.has_value());
  EXPECT_EQ(*opt_app_id, app_id);
}

// If install URL redirects, install should still succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallSucceedsWithRedirectNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL final_url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  GURL install_url =
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec());
  InstallApp(CreateInstallOptions(install_url));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ("Web app banner test page",
            registrar().GetAppShortName(app_id.value()));
  std::optional<webapps::AppId> opt_app_id =
      registrar().FindAppWithUrlInScope(final_url);
  ASSERT_TRUE(opt_app_id.has_value());
  EXPECT_TRUE(registrar().IsInstallState(
      opt_app_id.value(), {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                           proto::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_EQ(*opt_app_id, app_id);
  EXPECT_EQ(registrar().GetAppStartUrl(*opt_app_id), final_url);
}

// Installing a placeholder app with shortcuts should succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PlaceholderInstallSucceedsWithShortcuts) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL(
      "other.origin.com", "/banners/manifest_test_page.html");
  // Add a redirect to a different origin, so a placeholder is installed.
  GURL url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));

  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       UpdatePlaceholderSucceedsSameAppId) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL url = embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));

  simulate_redirect_ = false;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> final_app_id =
      registrar().LookupExternalAppId(url);
  ASSERT_TRUE(final_app_id.has_value());
  EXPECT_FALSE(registrar().IsPlaceholderApp(final_app_id.value(),
                                            WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       UpdatePlaceholderSucceedsDifferentAppIdFomStartUrl) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL install_url = embedded_test_server()->GetURL(
      "/banners/manifest_with_start_url_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(install_url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  const webapps::AppId placeholder_app_id =
      GenerateAppId(std::nullopt, install_url);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ(placeholder_app_id, *app_id);
  EXPECT_TRUE(registrar().IsPlaceholderApp(*app_id, WebAppManagement::kPolicy));

  simulate_redirect_ = false;
  InstallApp(options);

  GURL start_url = embedded_test_server()->GetURL(
      "/banners/different_manifest_test_page.html");
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  const webapps::AppId new_app_id = GenerateAppId(std::nullopt, start_url);

  EXPECT_NE(new_app_id, placeholder_app_id);
  EXPECT_FALSE(registrar().IsInstalled(placeholder_app_id));
  EXPECT_TRUE(registrar().IsInstalled(new_app_id));
  EXPECT_FALSE(
      registrar().IsPlaceholderApp(new_app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       UpdatePlaceholderSucceedsDifferentAppIdFomManifestId) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL install_url = embedded_test_server()->GetURL(
      "/banners/manifest_with_id_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(install_url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  const webapps::AppId placeholder_app_id =
      GenerateAppId(std::nullopt, install_url);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ(placeholder_app_id, *app_id);
  EXPECT_TRUE(registrar().IsPlaceholderApp(*app_id, WebAppManagement::kPolicy));

  simulate_redirect_ = false;
  InstallApp(options);

  GURL start_url = embedded_test_server()->GetURL("/banners/start");
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  const webapps::AppId new_app_id = GenerateAppId("some_id", start_url);

  EXPECT_NE(new_app_id, placeholder_app_id);
  EXPECT_FALSE(registrar().IsInstalled(placeholder_app_id));
  EXPECT_TRUE(registrar().IsInstalled(new_app_id));
  EXPECT_FALSE(
      registrar().IsPlaceholderApp(new_app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

#if BUILDFLAG(IS_CHROMEOS)
// Installing a placeholder app with a custom name should succeed.
// This feature is ChromeOS-only.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PlaceholderInstallSucceedsWithCustomName) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL(
      "other.origin.com", "/banners/manifest_test_page.html");
  // Add a redirect to a different origin, so a placeholder is installed.
  GURL url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));
  const std::string CUSTOM_NAME = "CUSTOM_NAME";

  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  options.override_name = CUSTOM_NAME;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));
  EXPECT_EQ(CUSTOM_NAME,
            registrar().GetAppById(app_id.value())->untranslated_name());
}

// Installing a placeholder app with a custom icon should succeed.
// This feature is ChromeOS-only.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PlaceholderInstallSucceedsWithCustomIcon) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL(
      "other.origin.com", "/banners/manifest_test_page.html");
  // Add a redirect to a different origin, so a placeholder is installed.
  GURL app_url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));
  // 192 is chosen to not be part of web_app_icon_generator.h:SizesToGenerate().
  GURL icon_url = embedded_test_server()->GetURL("/banners/192x192-green.png");
  const SquareSizePx kIconSize = 192;
  const SkColor kIconColor = SK_ColorGREEN;
  const auto kGeneratedSizes = SizesToGenerate();
  EXPECT_TRUE(kGeneratedSizes.find(kIconSize) == kGeneratedSizes.end());

  ExternalInstallOptions options =
      CreateInstallOptions(app_url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  options.override_icon_url = icon_url;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(app_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));
  SortedSizesPx downloaded_sizes =
      registrar().GetAppDownloadedIconSizesAny(app_id.value());
  EXPECT_EQ(1u + kGeneratedSizes.size(), downloaded_sizes.size());
  EXPECT_TRUE(downloaded_sizes.find(kIconSize) != downloaded_sizes.end());
  EXPECT_EQ(kIconColor,
            IconManagerReadAppIconPixel(provider()->icon_manager(),
                                        app_id.value(), kIconSize, 0, 0));
}

// This RequestHandler returns HTTP_NOT_FOUND the first time a URL containing
// |relative_url| is requested, and behaves normally in all other cases.
std::unique_ptr<net::test_server::HttpResponse> FailFirstRequest(
    const std::string& relative_url,
    const net::test_server::HttpRequest& request) {
  static bool first_run = true;
  if (first_run &&
      request.GetURL().spec().find(relative_url) != std::string::npos) {
    first_run = false;
    auto not_found_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    not_found_response->set_code(net::HTTP_NOT_FOUND);
    return std::move(not_found_response);
  }
  // Return nullptr to use the default handlers.
  return nullptr;
}

// Installing a placeholder app with a custom icon should succeed, even we have
// to retry fetching the icon once.
// This feature is ChromeOS-only.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PlaceholderInstallSucceedsWithCustomIconAfterRetry) {
  // Fail the first time that this URL is loaded.
  std::string kIconRelativeUrl = "/banners/192x192-green.png";
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&FailFirstRequest, kIconRelativeUrl));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL(
      "other.origin.com", "/banners/manifest_test_page.html");
  // Add a redirect to a different origin, so a placeholder is installed.
  GURL app_url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));
  // 192 is chosen to not be part of web_app_icon_generator.h:SizesToGenerate().
  GURL icon_url = embedded_test_server()->GetURL(kIconRelativeUrl);

  const SquareSizePx kIconSize = 192;
  const SkColor kIconColor = SK_ColorGREEN;
  const auto kGeneratedSizes = SizesToGenerate();
  EXPECT_TRUE(kGeneratedSizes.find(kIconSize) == kGeneratedSizes.end());

  ExternalInstallOptions options =
      CreateInstallOptions(app_url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  options.override_icon_url = icon_url;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(app_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));
  SortedSizesPx downloaded_sizes =
      registrar().GetAppDownloadedIconSizesAny(app_id.value());
  EXPECT_EQ(1u + kGeneratedSizes.size(), downloaded_sizes.size());
  EXPECT_TRUE(downloaded_sizes.find(kIconSize) != downloaded_sizes.end());
  EXPECT_EQ(kIconColor,
            IconManagerReadAppIconPixel(provider()->icon_manager(),
                                        app_id.value(), kIconSize, 0, 0));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

// Tests that the browser doesn't crash if it gets shutdown with a pending
// installation.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       ShutdownWithPendingInstallation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ExternalInstallOptions install_options = CreateInstallOptions(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  // Start an installation but don't wait for it to finish.
  provider()->externally_managed_app_manager().Install(
      std::move(install_options), base::DoNothing());

  // The browser should shutdown cleanly even if there is a pending
  // installation.
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest, ForceReinstall) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::optional<webapps::AppId> app_id;
  {
    GURL url(embedded_test_server()->GetURL(
        "/banners/"
        "manifest_test_page.html?manifest=manifest_short_name_only.json"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    app_id = registrar().FindAppWithUrlInScope(url);
    EXPECT_TRUE(app_id.has_value());
    EXPECT_EQ("Manifest", registrar().GetAppShortName(app_id.value()));
  }
  {
    GURL url(
        embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    std::optional<webapps::AppId> new_app_id =
        registrar().FindAppWithUrlInScope(url);
    EXPECT_TRUE(new_app_id.has_value());
    EXPECT_EQ(new_app_id, app_id);
    EXPECT_EQ("Manifest test app",
              registrar().GetAppShortName(new_app_id.value()));
  }
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PolicyAppOverridesUserInstalledApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::optional<webapps::AppId> app_id;
  {
    // Install user app
    GURL url(
        embedded_test_server()->GetURL("/banners/"
                                       "manifest_test_page.html"));
    auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    install_info->title = u"Test user app";
    app_id = test::InstallWebApp(profile(), std::move(install_info));
    ASSERT_TRUE(app_id.has_value());
    ASSERT_TRUE(registrar().WasInstalledByUser(app_id.value()));
    ASSERT_FALSE(registrar().HasExternalApp(app_id.value()));
    ASSERT_EQ("Test user app", registrar().GetAppShortName(app_id.value()));
  }
  {
    // Install policy app
    GURL url(
        embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
    std::optional<webapps::AppId> policy_app_id =
        ForceInstallWebApp(profile(), url);
    ASSERT_EQ(policy_app_id, app_id);
    ASSERT_EQ("Manifest test app",
              registrar().GetAppShortName(policy_app_id.value()));
  }
}

// Test that adding a manifest that points to a chrome:// URL does not actually
// install a web app that points to a chrome:// URL.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallChromeURLFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_chrome_url.json"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());

  // The installer falls back to installing a web app of the original URL.
  EXPECT_EQ(url, registrar().GetAppStartUrl(app_id.value()));
  EXPECT_NE(app_id,
            registrar().FindAppWithUrlInScope(GURL("chrome://settings")));
}

// Test that adding a web app without a manifest while using the
// |require_manifest| flag fails.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RequireManifestFailsIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"));
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.require_manifest = true;
  InstallApp(std::move(install_options));

  EXPECT_EQ(webapps::InstallResultCode::kNotValidManifestForWebApp,
            result_code_.value());
  std::optional<webapps::AppId> id = registrar().LookupExternalAppId(url);
  ASSERT_FALSE(id.has_value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Delay service worker registration to second load to simulate it not loading
  // during the initial install pass.
  GURL install_url(embedded_test_server()->GetURL(
      "/web_apps/service_worker_on_second_load.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  InstallApp(std::move(install_options));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  ExternalAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(install_url, RegistrationResultCode::kSuccess);
  CheckServiceWorkerStatus(
      install_url,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationAlternateUrlSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url(
      embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
  GURL registration_url =
      embedded_test_server()->GetURL("/web_apps/basic.html");

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.service_worker_registration_url = registration_url;
  InstallApp(std::move(install_options));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  ExternalAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(registration_url,
                             RegistrationResultCode::kSuccess);
  CheckServiceWorkerStatus(
      install_url,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationSkipped) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Delay service worker registration to second load to simulate it not loading
  // during the initial install pass.
  GURL install_url(embedded_test_server()->GetURL(
      "/web_apps/service_worker_on_second_load.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.load_and_await_service_worker_registration = false;
  ExternalAppRegistrationWaiter waiter(&externally_managed_app_manager());
  InstallApp(std::move(install_options));
  waiter.AwaitRegistrationsComplete();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  CheckServiceWorkerStatus(install_url,
                           content::ServiceWorkerCapability::NO_SERVICE_WORKER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       AlreadyRegistered) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure service worker registered for http://embedded_test_server/web_apps/.
  // We don't need to be installing a web app here but it's convenient just to
  // await the service worker registration.
  {
    GURL install_url(embedded_test_server()->GetURL("/web_apps/basic.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(install_url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
              result_code_.value());
    ExternalAppRegistrationWaiter(&externally_managed_app_manager())
        .AwaitNextNonFailedRegistration(install_url);
    CheckServiceWorkerStatus(
        embedded_test_server()->GetURL("/web_apps/basic.html"),
        content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
  }

  // With the service worker registered we install a page that doesn't register
  // a service worker to check that the existing service worker is seen by our
  // service worker registration step.
  {
    GURL install_url(
        embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(install_url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
              result_code_.value());
    ExternalAppRegistrationWaiter(&externally_managed_app_manager())
        .AwaitNextRegistration(install_url,
                               RegistrationResultCode::kAlreadyRegistered);
  }
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       CannotFetchManifest) {
  // With a flaky network connection, clients may request an app whose manifest
  // cannot currently be retrieved. The app display mode is then assumed to be
  // 'browser'.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=does_not_exist.json"));

  std::vector<ExternalInstallOptions> desired_apps_install_options;
  {
    ExternalInstallOptions install_options(
        app_url, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.require_manifest = false;
    desired_apps_install_options.push_back(std::move(install_options));
  }

  base::RunLoop run_loop;
  externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting(
          [&run_loop, &app_url](
              std::map<GURL, ExternallyManagedAppManager::InstallResult>
                  install_results,
              std::map<GURL, webapps::UninstallResultCode> uninstall_results) {
            EXPECT_TRUE(uninstall_results.empty());
            EXPECT_EQ(install_results.size(), 1U);
            EXPECT_EQ(install_results[app_url].code,
                      webapps::InstallResultCode::kSuccessNewInstall);
            run_loop.Quit();
          }));
  run_loop.Run();

  std::optional<webapps::AppId> app_id =
      registrar().FindAppWithUrlInScope(app_url);
  DCHECK(app_id.has_value());
  EXPECT_EQ(registrar().GetAppDisplayMode(*app_id), DisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(*app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppEffectiveDisplayMode(*app_id),
            DisplayMode::kMinimalUi);
  EXPECT_FALSE(registrar().GetAppThemeColor(*app_id).has_value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  CheckServiceWorkerStatus(url,
                           content::ServiceWorkerCapability::NO_SERVICE_WORKER);

  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.service_worker_registration_timeout = base::Seconds(0);
  InstallApp(std::move(install_options));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  ExternalAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(url, RegistrationResultCode::kTimeout);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       ReinstallPolicyAppWithLocallyInstalledApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  // Install user app
  auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(url);
  install_info->title = u"Test user app";
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(install_info));
  ASSERT_TRUE(registrar().WasInstalledByUser(app_id));
  ASSERT_FALSE(registrar().HasExternalApp(app_id));

  // Install policy app
  std::optional<webapps::AppId> policy_app_id =
      ForceInstallWebApp(profile(), url);
  ASSERT_EQ(policy_app_id, app_id);

  // Uninstall policy app
  std::vector<ExternalInstallOptions> desired_apps_install_options;
  base::RunLoop run_loop;
  externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting(
          [&run_loop, &url](
              std::map<GURL, ExternallyManagedAppManager::InstallResult>
                  install_results,
              std::map<GURL, webapps::UninstallResultCode> uninstall_results) {
            EXPECT_TRUE(install_results.empty());
            EXPECT_EQ(uninstall_results.size(), 1U);
            EXPECT_EQ(uninstall_results[url],
                      webapps::UninstallResultCode::kInstallSourceRemoved);
            run_loop.Quit();
          }));
  run_loop.Run();
  ASSERT_FALSE(registrar().GetAppById(app_id)->IsPolicyInstalledApp());

  // Reinstall policy app
  ForceInstallWebApp(profile(), url);
  ASSERT_TRUE(registrar().GetAppById(app_id)->IsPolicyInstalledApp());
}

class ExternallyManagedAppManagerBrowserTestShortcut
    : public ExternallyManagedAppManagerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExternallyManagedAppManagerBrowserTestShortcut() = default;
};

// Tests behavior when ExternalInstallOptions.install_as_shortcut is enabled
IN_PROC_BROWSER_TEST_P(ExternallyManagedAppManagerBrowserTestShortcut,
                       InstallAsShortcut) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL install_url(
      embedded_test_server()->GetURL("/web_apps/different_start_url.html"));
  GURL manifest_start_url(
      embedded_test_server()->GetURL("/web_apps/basic.html"));

  ExternalInstallOptions options =
      CreateInstallOptions(install_url, ExternalInstallSource::kExternalPolicy);
  options.install_as_shortcut = GetParam();

  InstallApp(options);
  ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  // The main difference between a normal web app installation and a shortcut
  // creation is that in the latter the start_url field of the page's manifest
  // is ignored. Thus the installation URL is always used even when the
  // manifest tells otherwise, as in the test page used here.

  const bool startUrlIsInstallUrl =
      registrar().GetAppByStartUrl(install_url) != nullptr;
  const bool startUrlFromManifest =
      registrar().GetAppByStartUrl(manifest_start_url) != nullptr;
  EXPECT_NE(startUrlIsInstallUrl, startUrlFromManifest);

  EXPECT_EQ(options.install_as_shortcut, startUrlIsInstallUrl);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExternallyManagedAppManagerBrowserTestShortcut,
                         ::testing::Bool());

#if BUILDFLAG(IS_CHROMEOS)
class PlaceholderUpdateRelaunchBrowserTest
    : public ExternallyManagedAppManagerBrowserTest,
      public NotificationDisplayService::Observer {
 public:
  ~PlaceholderUpdateRelaunchBrowserTest() override {
    notification_observation_.Reset();
  }

  // NotificationDisplayService::Observer:
  MOCK_METHOD(void,
              OnNotificationDisplayed,
              (const message_center::Notification&,
               const NotificationCommon::Metadata* const),
              (override));
  MOCK_METHOD(void,
              OnNotificationClosed,
              (const std::string& notification_id),
              (override));

  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {
    notification_observation_.Reset();
  }

  void AddForceInstalledApp(const std::string& manifest_id,
                            const std::string& app_name) {
    base::test::TestFuture<void> app_sync_future;
    provider()
        ->policy_manager()
        .SetOnAppsSynchronizedCompletedCallbackForTesting(
            app_sync_future.GetCallback());
    PrefService* prefs = profile()->GetPrefs();
    base::Value::List install_force_list =
        prefs->GetList(prefs::kWebAppInstallForceList).Clone();
    install_force_list.Append(
        base::Value::Dict()
            .Set(kUrlKey, manifest_id)
            .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue)
            .Set(kFallbackAppNameKey, app_name));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(install_force_list));
    EXPECT_TRUE(app_sync_future.Wait());
  }

  void AddPreventCloseToApp(const std::string& manifest_id,
                            const std::string& run_on_os_login) {
    base::test::TestFuture<void> policy_refresh_sync_future;
    provider()
        ->policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_refresh_sync_future.GetCallback());
    PrefService* prefs = profile()->GetPrefs();
    base::Value::List web_app_settings =
        prefs->GetList(prefs::kWebAppSettings).Clone();
    web_app_settings.Append(base::Value::Dict()
                                .Set(kManifestId, manifest_id)
                                .Set(kRunOnOsLogin, run_on_os_login)
                                .Set(kPreventClose, true));
    prefs->SetList(prefs::kWebAppSettings, std::move(web_app_settings));
    EXPECT_TRUE(policy_refresh_sync_future.Wait());
  }

  void WaitForNumberOfAppInstances(const webapps::AppId& app_id,
                                   size_t number_of_app_instances) {
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return provider()->ui_manager().GetNumWindowsForApp(app_id) ==
             number_of_app_instances;
    }));
  }

  auto GetAllNotifications() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
    NotificationDisplayServiceFactory::GetForProfile(profile())->GetDisplayed(
        get_displayed_future.GetCallback());
#else
    base::test::TestFuture<const std::vector<std::string>&>
        get_displayed_future;
    auto& remote = chromeos::LacrosService::Get()
                       ->GetRemote<crosapi::mojom::MessageCenter>();
    EXPECT_TRUE(remote.get());
    remote->GetDisplayedNotifications(get_displayed_future.GetCallback());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    const auto& notification_ids = get_displayed_future.Get<0>();
    EXPECT_TRUE(get_displayed_future.Wait());
    return notification_ids;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ClearAllNotifications() {
    base::test::TestFuture<const std::vector<std::string>&>
        get_displayed_future;
    auto& service = chromeos::LacrosService::Get()
                        ->GetRemote<crosapi::mojom::MessageCenter>();
    EXPECT_TRUE(service.get());
    for (const std::string& notification_id : GetAllNotifications()) {
      service->CloseNotification(notification_id);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  size_t GetDisplayedNotificationsCount() {
    return GetAllNotifications().size();
  }

  void WaitUntilDisplayNotificationCount(size_t display_count) {
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return GetDisplayedNotificationsCount() == display_count;
    }));
  }

 protected:
  base::ScopedObservation<NotificationDisplayService,
                          PlaceholderUpdateRelaunchBrowserTest>
      notification_observation_{this};
};

// TODO(b:341035409): Flaky.
IN_PROC_BROWSER_TEST_F(
    PlaceholderUpdateRelaunchBrowserTest,
    DISABLED_UpdatePlaceholderRelaunchClosePreventedAppSucceeds) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This may be needed due to side-effects previously run lacros tests.
  ClearAllNotifications();
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  notification_observation_.Observe(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL install_url = embedded_test_server()->GetURL(
      "/banners/manifest_with_id_test_page.html");

  // Force install the placeholder.
  AddForceInstalledApp(install_url.spec(), /*app_name=*/"placeholder app");

  const webapps::AppId placeholder_app_id =
      GenerateAppId(std::nullopt, install_url);

  // Enable prevent-close close for the placeholder.
  AddPreventCloseToApp(install_url.spec(), kRunWindowed);

  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ(placeholder_app_id, *app_id);
  EXPECT_TRUE(registrar().IsPlaceholderApp(*app_id, WebAppManagement::kPolicy));

  EXPECT_CALL(
      *this,
      OnNotificationDisplayed(
          AllOf(
              Property(&message_center::Notification::id,
                       Eq("web_app_relaunch_notifier:" + placeholder_app_id)),
              Property(&message_center::Notification::notifier_id,
                       Field(&message_center::NotifierId::id,
                             Eq("web_app_relaunch"))),
              Property(&message_center::Notification::title,
                       Eq(u"Restarting and updating Manifest test app with id "
                          u"specified")),
              Property(
                  &message_center::Notification::message,
                  Eq(u"Please wait while this application is being updated"))),
          _))
      .Times(1);

  // Launch the PWA so that the app relaunch is triggered on sync.
  ASSERT_TRUE(web_app::LaunchWebAppBrowser(profile(), placeholder_app_id,
                                           WindowOpenDisposition::NEW_WINDOW));
  WaitForNumberOfAppInstances(placeholder_app_id,
                              /*number_of_app_instances=*/1u);

  // Resolve the redirect (placeholder can be updated now).
  simulate_redirect_ = false;
  provider()->policy_manager().RefreshPolicyInstalledAppsForTesting(
      /*allow_close_and_relaunch=*/true);

  // Wait until the final version of the app is installed.
  const webapps::AppId final_app_id = GenerateAppId("some_id", install_url);

  // Check that the placeholder app is indeed closed.
  WaitForNumberOfAppInstances(placeholder_app_id,
                              /*number_of_app_instances=*/0u);

  // Wait for the placeholder removal task to be done.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() -> bool { return !registrar().IsInstalled(placeholder_app_id); }));

  // Check that the new app is launched.
  WaitForNumberOfAppInstances(final_app_id, /*number_of_app_instances=*/1u);

  // Make sure that the notification got cleaned up.
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);

  EXPECT_NE(final_app_id, placeholder_app_id);
  EXPECT_TRUE(registrar().IsInstalled(final_app_id));
  EXPECT_FALSE(
      registrar().IsPlaceholderApp(final_app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
