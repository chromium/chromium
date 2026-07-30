// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/model/app_installed_by.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_install_service_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "skia/ext/image_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace web_app {
namespace {

constexpr char kAbortError[] = "AbortError";
constexpr char kDataError[] = "DataError";
constexpr char kSecurityError[] = "SecurityError";
constexpr char kTestPageWithId[] = "/banners/manifest_with_id_test_page.html";
constexpr char kValidManifestNoId[] = "/banners/manifest.json";
constexpr char kValidManifestWithId[] = "/banners/manifest_with_id.json";
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
constexpr apps::LaunchSource kLaunchSource =
    apps::LaunchSource::kFromWebInstallApi;

// Records whether a named widget is ever shown. Construct it before firing the
// install and query `shown()` afterwards. Defaults to watching the simple
// install dialog.
class WebInstallDialogShownWatcher {
 public:
  explicit WebInstallDialogShownWatcher(
      std::string widget_name = "WebAppSimpleInstallDialog")
      : widget_name_(std::move(widget_name)) {
    observer_.set_shown_callback(
        base::BindLambdaForTesting([this](views::Widget* widget) {
          if (widget->GetName() == widget_name_) {
            shown_ = true;
          }
        }));
  }

  bool shown() const { return shown_; }

 private:
  const std::string widget_name_;
  bool shown_ = false;
  views::AnyWidgetObserver observer_{views::test::AnyWidgetTestPasskey{}};
};

// Telemetry emitted by the manifest URL install flow.
constexpr char kInstallResultUma[] = "WebApp.WebInstallApi.Result";
constexpr char kInstallTypeUma[] = "WebApp.WebInstallApi.InstallType";
constexpr char kVariantedInstallResultUma[] =
    "WebApp.WebInstallService.Api.Result";
constexpr char kVariantedInstallTypeUma[] =
    "WebApp.WebInstallService.Api.InstallType";

// Browser tests for the navigator.install({manifest: ...}) flow.
// These require a real renderer because manifest parsing uses the
// ManifestManager mojo interface via ParseManifestFromStringJob.
class WebInstallFromManifestBrowserTest : public WebAppBrowserTestBase {
 public:
  WebInstallFromManifestBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebAppInstallation},
        {features::kWebAppInstallDialog});
  }

  void SetUpOnMainThread() override {
    // Register a handler for dynamic test-specific responses (e.g., invalid
    // JSON). For recognized paths it returns a response; for everything else
    // it returns nullptr and the server falls through to serve static files.
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &WebInstallFromManifestBrowserTest::HandleDynamicRequest,
        base::Unretained(this)));
    WebAppBrowserTestBase::SetUpOnMainThread();
    // A second HTTPS origin, used to navigate the initiating page cross-origin
    // mid-install.
    secondary_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(secondary_server_.Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void NavigateToValidUrl(Browser* test_browser = nullptr) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        test_browser ? test_browser : browser(),
        embedded_https_test_server().GetURL("/simple.html")));
  }

  // Calls navigator.install({manifest: manifest_url}) with a user gesture.
  bool TryInstallFromManifest(const GURL& manifest_url,
                              content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    const std::string script = content::JsReplace(
        "navigator.install({manifest: $1})"
        ".then(result => { webInstallResult = result; })"
        ".catch(error => { webInstallError = error; });",
        manifest_url.spec());
    return content::ExecJs(wc, script);
  }

  // Calls navigator.install({manifest: manifest_url, manifestId: manifest_id})
  // with a user gesture.
  bool TryInstallFromManifestWithId(const GURL& manifest_url,
                                    const GURL& manifest_id,
                                    content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    const std::string script = content::JsReplace(
        "navigator.install({manifest: $1, manifestId: $2})"
        ".then(result => { webInstallResult = result; })"
        ".catch(error => { webInstallError = error; });",
        manifest_url.spec(), manifest_id.spec());
    return content::ExecJs(wc, script);
  }

  // Calls navigator.install({manifest: manifest_url}) without resolving the
  // returned promise. For tests that
  // interrupt the flow (navigate away, close the tab, or inspect the dialog).
  bool FireInstallFromManifestNoResolve(
      const GURL& manifest_url,
      content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    const std::string script = content::JsReplace(
        "navigator.install({manifest: $1})"
        ".then(result => { webInstallResult = result; })"
        ".catch(error => { webInstallError = error; });",
        manifest_url.spec());
    return content::ExecJs(wc, script,
                           content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);
  }

  bool ResultExists(content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    return content::ExecJs(wc, "webInstallResult");
  }

  bool ErrorExists(content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    return content::ExecJs(wc, "webInstallError");
  }

  std::string GetErrorName(content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    return content::EvalJs(wc, "webInstallError.name").ExtractString();
  }

  // Sets a dynamic manifest response for /dynamic_manifest.json.
  void SetDynamicManifestResponse(const std::string& json) {
    dynamic_manifest_json_ = json;
  }

  // When the permission prompt shows, it must be granted or denied.
  void SetPermissionResponse(bool permission_granted,
                             content::WebContents* contents = nullptr) {
    permissions::PermissionRequestManager::AutoResponseType response =
        permission_granted
            ? permissions::PermissionRequestManager::AutoResponseType::
                  ACCEPT_ALL
            : permissions::PermissionRequestManager::AutoResponseType::DENY_ALL;

    permissions::PermissionRequestManager::FromWebContents(
        contents ? contents : web_contents())
        ->set_auto_response_for_test(response);
  }

  std::deque<AppInstalledBy> GetInstalledBy(const webapps::AppId& app_id) {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    CHECK(app);
    return app->installed_by();
  }

 protected:
  net::EmbeddedTestServer secondary_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleDynamicRequest(
      const net::test_server::HttpRequest& request) {
    // A top-level document that disallows the "web-app-installation" feature
    // via its Permissions-Policy response header.
    if (request.relative_url == "/disallow_web_install.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->AddCustomHeader("Permissions-Policy",
                                "web-app-installation=()");
      response->set_content("<!doctype html><title>no install</title>");
      return response;
    }
    if (request.relative_url == "/dynamic_manifest.json") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(dynamic_manifest_json_);
      return response;
    }
    return nullptr;
  }

  std::string dynamic_manifest_json_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Valid manifest with custom id, no id option provided.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       ManifestOnly_Succeeds) {
  base::HistogramTester histograms;
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  base::AutoReset<web_app::InstallDialogTestResponse> auto_accept_pwa =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kAcceptAndLaunch);

  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifest(
      embedded_https_test_server().GetURL(kValidManifestWithId)));
  observer.Wait();

  // The permission prompt must have been surfaced before granting.
  EXPECT_TRUE(observer.request_shown());

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  // Verify the app is registered.
  GURL manifest_id = embedded_https_test_server().GetURL("/some_id");
  webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Verify the app is correctly attributed to the document that called
  // navigator.install, i.e. the current committed URL.
  std::deque<AppInstalledBy> installed_by = GetInstalledBy(app_id);
  ASSERT_EQ(installed_by.size(), 1u);
  EXPECT_FALSE(installed_by.front().install_api_call_time().is_null());
  EXPECT_EQ(installed_by.front().requesting_url(),
            web_contents()->GetLastCommittedURL());

  test::CompletePageLoadForAllWebContents();
  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample(
      "WebApp.InstallCommand.InstallFromManifestUrl.ResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);
  histograms.ExpectBucketCount("Blink.UseCounter.WebDXFeatures",
                               blink::mojom::WebDXFeature::kNavigatorInstall,
                               1);

  // Install succeeded end-to-end: type/result UMA record kSuccess.
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kSuccess, 1);
}

// Valid manifest with custom id, matching id option.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       ManifestAndId_Succeeds) {
  base::HistogramTester histograms;
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  base::AutoReset<web_app::InstallDialogTestResponse> auto_accept_pwa =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kAcceptAndLaunch);

  // kValidManifestWithId has "id": "some_id", which resolves relative to
  // the manifest URL's origin.
  GURL manifest_id = embedded_https_test_server().GetURL("/some_id");

  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifestWithId(
      embedded_https_test_server().GetURL(kValidManifestWithId), manifest_id));
  observer.Wait();

  // The permission prompt must have been surfaced before granting.
  EXPECT_TRUE(observer.request_shown());

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  // Verify the app is registered.
  webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Verify the app is correctly attributed to the document that called
  // navigator.install, i.e. the current committed URL.
  std::deque<AppInstalledBy> installed_by = GetInstalledBy(app_id);
  ASSERT_EQ(installed_by.size(), 1u);
  EXPECT_FALSE(installed_by.front().install_api_call_time().is_null());
  EXPECT_EQ(installed_by.front().requesting_url(),
            web_contents()->GetLastCommittedURL());

  test::CompletePageLoadForAllWebContents();
  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample(
      "WebApp.InstallCommand.InstallFromManifestUrl.ResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);
  histograms.ExpectBucketCount("Blink.UseCounter.WebDXFeatures",
                               blink::mojom::WebDXFeature::kNavigatorInstall,
                               1);

  // Install succeeded end-to-end: type/result UMA record kSuccess.
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kSuccess, 1);
}

// When the user denies the Web Install permission prompt, the install is
// rejected with AbortError.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       PermissionDenied_AbortError) {
  base::HistogramTester histograms;
  NavigateToValidUrl();

  SetPermissionResponse(/*permission_granted=*/false);
  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifest(
      embedded_https_test_server().GetURL(kValidManifestWithId)));
  observer.Wait();

  // The permission prompt must have been surfaced before denial.
  EXPECT_TRUE(observer.request_shown());

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  // Parse+id validation ran before the prompt; result UMA records the
  // permission-denied outcome.
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kPermissionDenied, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kPermissionDenied, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       PermissionsPolicyDisallowed_SecurityError) {
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("/disallow_web_install.html")));

  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifest(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  // No prompt should ever be surfaced; the rejection is synchronous.
  EXPECT_FALSE(observer.request_shown());

  EXPECT_FALSE(ResultExists());
  ASSERT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kSecurityError);

  // Blink rejects in the renderer before calling the browser, so no telemetry.
  histograms.ExpectTotalCount(kInstallResultUma, 0);
  histograms.ExpectTotalCount(kVariantedInstallResultUma, 0);
  histograms.ExpectTotalCount(kInstallTypeUma, 0);
  histograms.ExpectTotalCount(kVariantedInstallTypeUma, 0);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       ManifestWithoutIcons_AbortError) {
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  // Valid start_url/name and a custom id (so parsing and the id check pass),
  // but deliberately no "icons" member.
  SetDynamicManifestResponse(
      R"({"id": "/no-icons-app", "start_url": "/index.html",)"
      R"( "name": "No Icons App", "display": "standalone"})");

  GURL manifest_url =
      embedded_https_test_server().GetURL("/dynamic_manifest.json");

  permissions::PermissionRequestObserver observer(web_contents());
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallFromManifest(manifest_url));
  observer.Wait();

  // Permission is requested and granted before the icon check runs in the
  // command.
  EXPECT_TRUE(observer.request_shown());

  EXPECT_FALSE(ResultExists());
  ASSERT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  histograms.ExpectUniqueSample(
      "WebApp.InstallCommand.InstallFromManifestUrl.ResultCode",
      webapps::InstallResultCode::kNoValidIconsInManifest, 1);
}

// User declines the install dialog after permission is granted.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       InstallDialogDeclined_AbortError) {
  NavigateToValidUrl();

  base::AutoReset<web_app::InstallDialogTestResponse> auto_decline_pwa =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kDeny);
  SetPermissionResponse(/*permission_granted=*/true);
  base::HistogramTester histograms;

  ASSERT_TRUE(TryInstallFromManifest(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  histograms.ExpectUniqueSample(
      "WebApp.InstallCommand.InstallFromManifestUrl.ResultCode",
      webapps::InstallResultCode::kUserInstallDeclined, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       CommandShutdown_RecordsResultMetric) {
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);

  base::HistogramTester histograms;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppSimpleInstallDialog");

  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  // Keep the dialog unanswered so the command is still active at shutdown.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  provider().command_manager().Shutdown();

  histograms.ExpectUniqueSample(
      "WebApp.InstallCommand.InstallFromManifestUrl.ResultCode",
      webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);

  // Command shutdown does not own the UI, so close the unanswered dialog.
  views::test::CancelDialog(widget);
  destroyed.Wait();
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       ConcurrentInstalls_AbortError) {
  NavigateToValidUrl();
  GURL manifest_url = embedded_https_test_server().GetURL(kValidManifestWithId);

  SetPermissionResponse(/*permission_granted=*/true);

  // Do not wait for the promise to resolve.
  ASSERT_TRUE(FireInstallFromManifestNoResolve(manifest_url));

  // Fire install #2 — should be rejected by the concurrent install guard.
  ASSERT_TRUE(TryInstallFromManifest(manifest_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

// Closing the initiating tab while the install dialog is open must close the
// dialog and prevent installation.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       TabClosedAfterDialogShown_ClosesDialog) {
  NavigateToValidUrl();
  content::WebContents* survivor = web_contents();

  // Open a second tab that will initiate and be destroyed during the install.
  ASSERT_TRUE(AddTabAtIndex(1,
                            embedded_https_test_server().GetURL("/simple.html"),
                            ui::PAGE_TRANSITION_TYPED));
  content::WebContents* install_wc =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(install_wc, survivor);

  SetPermissionResponse(/*permission_granted=*/true, install_wc);
  const GURL manifest_id = embedded_https_test_server().GetURL("/some_id");
  const webapps::AppId app_id =
      GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppSimpleInstallDialog");

  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId), install_wc));

  // Verify the dialog is shown.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter dialog_destroyed(widget);

  // Close the initiating tab.
  content::WebContentsDestroyedWatcher destroyed_watcher(install_wc);
  const int install_tab_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(install_wc);
  ASSERT_NE(install_tab_index, TabStripModel::kNoTab);
  browser()->tab_strip_model()->CloseWebContentsAt(
      install_tab_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();
  dialog_destroyed.Wait();

  // Drain the command and verify that closing the tab prevented installation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // The browser survived: the surviving tab is still responsive.
  EXPECT_TRUE(content::ExecJs(survivor, "true"));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       TabClosedAfterAppCommitted_DoesNotLaunch) {
  NavigateToValidUrl();
  content::WebContents* survivor = web_contents();

  ASSERT_TRUE(AddTabAtIndex(1,
                            embedded_https_test_server().GetURL("/simple.html"),
                            ui::PAGE_TRANSITION_TYPED));
  content::WebContents* install_wc =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(install_wc, survivor);

  SetPermissionResponse(/*permission_granted=*/true, install_wc);
  base::AutoReset<web_app::InstallDialogTestResponse> auto_accept_pwa =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kAcceptAndLaunch);

  const GURL manifest_id = embedded_https_test_server().GetURL("/some_id");
  const webapps::AppId app_id =
      GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));
  base::HistogramTester histograms;

  // Close the initiating tab after the app is committed to the database but
  // before FinalizeInstallJob reports completion to the command.
  base::test::TestFuture<void> initiating_tab_closed;
  WebAppInstallManagerObserverAdapter install_observer(profile());
  install_observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        if (installed_app_id != app_id) {
          return;
        }
        const int index =
            browser()->tab_strip_model()->GetIndexOfWebContents(install_wc);
        if (index == TabStripModel::kNoTab) {
          ADD_FAILURE() << "Initiating tab was already detached";
          initiating_tab_closed.SetValue();
          return;
        }
        browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(index);
        initiating_tab_closed.SetValue();
      }));

  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId), install_wc));
  ASSERT_TRUE(initiating_tab_closed.Wait());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectUniqueSample(
      "WebApp.InstallCommand.InstallFromManifestUrl.ResultCode",
      webapps::InstallResultCode::kCancelledDueToMainFrameNavigation, 1);
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 0);
  EXPECT_TRUE(content::ExecJs(survivor, "true"));
}

// A same-origin, but *cross-document* navigation of the initiating page
// mid-install must cancel the install - the install command is *page* scoped,
// not origin/document scoped.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       SameOriginNavigationDuringInstall_Aborts) {
  NavigateToValidUrl();

  SetPermissionResponse(/*permission_granted=*/true);
  base::AutoReset<web_app::InstallDialogTestResponse> auto_accept_pwa =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kAcceptAndLaunch);

  const GURL manifest_url =
      embedded_https_test_server().GetURL(kValidManifestWithId);
  const GURL manifest_id = embedded_https_test_server().GetURL("/some_id");
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));

  WebInstallDialogShownWatcher dialog_watcher;
  ASSERT_TRUE(FireInstallFromManifestNoResolve(manifest_url));

  // Navigate the initiating tab to a new same-origin document while the install
  // is in flight. This is cross-document (a new Page), so it destroys the page
  // that initiated the install and the document-scoped service with it.
  const GURL same_origin_url =
      embedded_https_test_server().GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_url));
  EXPECT_EQ(url::Origin::Create(manifest_url),
            url::Origin::Create(same_origin_url));

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // No dialog was shown and no app was installed.
  EXPECT_FALSE(dialog_watcher.shown());
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // The browser survived: the current tab is still responsive.
  EXPECT_TRUE(content::ExecJs(web_contents(), "true"));
}

class WebInstallFromManifestControlledIconBrowserTest
    : public WebInstallFromManifestBrowserTest {
 public:
  void SetUpOnMainThread() override {
    icon_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &embedded_https_test_server(), "/banners/image-512px.png");
    WebInstallFromManifestBrowserTest::SetUpOnMainThread();
  }

 protected:
  net::test_server::ControllableHttpResponse& icon_response() {
    return *icon_response_;
  }

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse> icon_response_;
};

// A cross-origin navigation after the command starts but before the install
// dialog is shown must cancel the install.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestControlledIconBrowserTest,
                       CrossOriginNavigationBeforeDialog_Aborts) {
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  base::AutoReset<web_app::InstallDialogTestResponse> auto_accept_pwa =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kAcceptAndLaunch);

  const GURL manifest_url =
      embedded_https_test_server().GetURL(kValidManifestWithId);
  const GURL manifest_id = embedded_https_test_server().GetURL("/some_id");
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));

  base::HistogramTester histograms;
  WebInstallDialogShownWatcher dialog_watcher;
  ASSERT_TRUE(FireInstallFromManifestNoResolve(manifest_url));

  // Waiting for an icon request proves the manifest command has started, while
  // holding the response prevents it from reaching the install dialog.
  icon_response().WaitForRequest();
  ASSERT_TRUE(
      provider().command_manager().IsInstallingForWebContents(web_contents()));
  EXPECT_FALSE(dialog_watcher.shown());

  // Navigate the initiating tab cross-origin.
  const GURL cross_origin_url = secondary_server_.GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_origin_url));
  EXPECT_NE(url::Origin::Create(manifest_url),
            url::Origin::Create(cross_origin_url));

  icon_response().Done();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // No dialog was shown and no app was installed.
  histograms.ExpectUniqueSample(
      "WebApp.InstallCommand.InstallFromManifestUrl.ResultCode",
      webapps::InstallResultCode::kCancelledDueToMainFrameNavigation, 1);
  EXPECT_FALSE(dialog_watcher.shown());
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
}

// Verifies the command-in-progress guard (IsInstallingForWebContents()).
// Uses the scheduler directly to skip the earlier HasCurrentInstall() guard,
// then pins the command at the dialog by not invoking acceptance.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       CreateWebAppFromManifest_CommandInProgress_Rejects) {
  const GURL test_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  // Hold the command open at the dialog stage by capturing (and not running)
  // its acceptance callback.
  base::RunLoop dialog_reached;
  std::unique_ptr<WebAppInstallInfo> held_info;
  WebAppInstallationAcceptanceCallback held_acceptance;
  auto dialog_callback = base::BindLambdaForTesting(
      [&](base::WeakPtr<WebAppScreenshotFetcher>, content::WebContents*,
          std::unique_ptr<WebAppInstallInfo> web_app_info,
          WebAppInstallationAcceptanceCallback acceptance_callback) {
        held_info = std::move(web_app_info);
        held_acceptance = std::move(acceptance_callback);
        dialog_reached.Quit();
      });

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(), std::move(dialog_callback),
      install_future.GetCallback(), FallbackBehavior::kCraftedManifestOnly);

  // Wait until the command reaches the dialog stage; it is now in progress for
  // this WebContents.
  dialog_reached.Run();
  ASSERT_TRUE(
      provider().command_manager().IsInstallingForWebContents(web_contents()));

  // The earlier HasCurrentInstall() guard must NOT be the one that fires: a
  // directly-scheduled command does not register a current ML install.
  webapps::MLInstallabilityPromoter* promoter =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents());
  ASSERT_TRUE(promoter);
  ASSERT_FALSE(promoter->HasCurrentInstall());

  // A direct CreateWebAppFromManifest() call must now short-circuit on the
  // command-in-progress guard, running its callback exactly once.
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      guard_future;
  EXPECT_FALSE(CreateWebAppFromManifest(
      web_contents(), webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      guard_future.GetCallback()));
  EXPECT_TRUE(guard_future.Get<webapps::AppId>().empty());
  EXPECT_EQ(guard_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kInstallAlreadyInProgress);

  // Release the held command so it completes, for clean teardown.
  AdaptToLaunchOnInstallSuccess(std::move(held_acceptance))
      .Run(/*accept=*/true, std::move(held_info));
  ASSERT_TRUE(install_future.Wait());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
}

// Navigating the initiating page cross-origin *after* the
// install dialog is already showing must close the dialog and not install the
// app.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       CrossOriginNavigationAfterDialogShown_ClosesDialog) {
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);

  const GURL manifest_url =
      embedded_https_test_server().GetURL(kValidManifestWithId);
  const GURL manifest_id = embedded_https_test_server().GetURL("/some_id");
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppSimpleInstallDialog");

  // With no auto-response set, the dialog stays open so we can navigate while
  // it is showing.
  ASSERT_TRUE(FireInstallFromManifestNoResolve(manifest_url));

  // Wait for the install dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  // Navigate the initiating tab cross-origin while the dialog is open.
  const GURL cross_origin_url = secondary_server_.GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_origin_url));
  EXPECT_NE(url::Origin::Create(manifest_url),
            url::Origin::Create(cross_origin_url));

  // The dialog must close in response to the navigation.
  destroyed.Wait();

  // Drain any in-flight commands and confirm no app was installed.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
}

///////////////////////////////////////////////////////////////////////////////
// Collection of tests for calling `navigator.install({manifest: ...})` for an
// app that is already installed. In these cases we show the
// `WebAppLaunchDialog` to let the user launch the app instead of attempting to
// install it again.
///////////////////////////////////////////////////////////////////////////////

class WebInstallFromManifestBackForwardCacheBrowserTest
    : public WebInstallFromManifestBrowserTest {
 public:
  WebInstallFromManifestBackForwardCacheBrowserTest() {
    back_forward_cache_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  void SetUpOnMainThread() override {
    manifest_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &embedded_https_test_server(), "/controlled_manifest.json");
    WebInstallFromManifestBrowserTest::SetUpOnMainThread();
  }

 protected:
  net::test_server::ControllableHttpResponse& manifest_response() {
    return *manifest_response_;
  }

 private:
  base::test::ScopedFeatureList back_forward_cache_feature_list_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      manifest_response_;
};

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBackForwardCacheBrowserTest,
                       AlreadyInstalled_NavigationAwayAndBack_Aborts) {
  // Install an app.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Fire navigator.install.
  NavigateToValidUrl();
  content::RenderFrameHostWrapper initiating_rfh(
      web_contents()->GetPrimaryMainFrame());
  WebInstallDialogShownWatcher launch_dialog_watcher("WebInstallLaunchDialog");
  base::HistogramTester histograms;
  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL("/controlled_manifest.json")));

  // Pause the manifest response so the API request remains in flight.
  manifest_response().WaitForRequest();
  webapps::MLInstallabilityPromoter* promoter =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents());
  ASSERT_TRUE(promoter);
  ASSERT_TRUE(promoter->HasCurrentInstall());

  // Navigate away, sending the initiating RFH to the back/forward cache.
  const GURL destination_url = secondary_server_.GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), destination_url));
  ASSERT_TRUE(initiating_rfh);
  ASSERT_TRUE(initiating_rfh->IsInLifecycleState(
      content::RenderFrameHost::LifecycleState::kInBackForwardCache));

  // Navigate back, restoring the initiating page from the back/forward cache.
  // The install must still remember that its page navigated away.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  ASSERT_EQ(initiating_rfh.get(), web_contents()->GetPrimaryMainFrame());

  // Release the delayed manifest. The same json used by `kTestPageWithId`.
  manifest_response().Send(net::HTTP_OK, "application/manifest+json",
                           R"json({
      "id": "/some_id",
      "start_url": "/banners/manifest_with_id_test_page.html",
      "name": "Manifest test app with id specified",
      "display": "standalone",
      "icons": [{
        "src": "/banners/image-512px.png",
        "sizes": "48x48"
      }]
    })json");
  manifest_response().Done();

  // The install should abort and no app should be launched (but the app remains
  // installed).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !promoter->HasCurrentInstall(); }));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(launch_dialog_watcher.shown());
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 0);

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       AlreadyInstalled_UserAcceptsLaunchDialog) {
  // Install an app.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Fire navigator.install.
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  base::AutoReset<web_app::InstallDialogTestResponse> auto_accept =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kAcceptAndLaunch);
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallFromManifest(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  // The app was launched via the Web Install API, not reinstalled.
  test::CompletePageLoadForAllWebContents();
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 1);

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectTotalCount("WebApp.Install.Source.Success", 0);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(
      kInstallResultUma, WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallResultUma,
      WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
}

// Same as above, but the caller also supplies a matching `id`.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       AlreadyInstalled_WithId_UserAcceptsLaunchDialog) {
  // Install an app.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);
  const GURL manifest_id = embedded_https_test_server().GetURL("/some_id");
  ASSERT_EQ(app_id, web_app::GenerateAppIdFromManifestId(
                        webapps::ManifestId(manifest_id)));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Fire navigator.install with the matching manifest ID.
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  base::AutoReset<web_app::InstallDialogTestResponse> auto_accept =
      web_app::SetPwaInstallationAutoRespondForTesting(
          web_app::InstallDialogTestResponse::kAcceptAndLaunch);

  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallFromManifestWithId(
      embedded_https_test_server().GetURL(kValidManifestWithId), manifest_id));

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  // The app was launched via the Web Install API, not reinstalled.
  test::CompletePageLoadForAllWebContents();
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 1);

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectTotalCount("WebApp.Install.Source.Success", 0);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(
      kInstallResultUma, WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallResultUma,
      WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       AlreadyInstalled_UserCancelsLaunchDialog) {
  // Install an app.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Fire navigator.install without waiting for the promise.
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebInstallLaunchDialog");
  base::HistogramTester histograms;
  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  // Cancel the launch dialog.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);
  views::test::CancelDialog(widget);
  destroyed.Wait();

  // The app should not be launched (but remains installed).
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 0);

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(
      kInstallResultUma, WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallResultUma,
      WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       AlreadyInstalled_Incognito_Aborts) {
  // Install an app in a regular profile.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  Browser* incognito_browser = CreateIncognitoBrowser();
  NavigateToValidUrl(incognito_browser);

  base::HistogramTester histograms;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");

  // Fire navigator.install from an incognito window.
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId),
      incognito_web_contents));

  // Wait for the "install not supported" dialog.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  // Simulate the user accepting the dialog.
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  // The install should abort and no app should be launched.
  EXPECT_FALSE(ResultExists(incognito_web_contents));
  EXPECT_TRUE(ErrorExists(incognito_web_contents));
  EXPECT_EQ(GetErrorName(incognito_web_contents), kAbortError);
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 0);

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       AlreadyInstalled_UninstalledWhileLaunchDialogOpen) {
  // Install an app.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);

  // Fire navigator.install.
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebInstallLaunchDialog");
  base::HistogramTester histograms;
  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  // Uninstall the app while the launch dialog is open.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);
  web_app::test::UninstallWebApp(browser()->GetProfile(), app_id);
  ASSERT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Accept the now-stale dialog.
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  // Drain the launch command that the accept scheduled.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  test::CompletePageLoadForAllWebContents();

  // Dialog was accepted, so the promise resolves, even though no app was
  // launched.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents(),
                           "typeof webInstallResult !== 'undefined'")
        .ExtractBool();
  }));
  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 0);

  // Nothing launched: the app is gone and the launch command bailed out.
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(
      kInstallResultUma, WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallResultUma,
      WebInstallServiceResult::kSuccessAlreadyInstalled, 1);

  // The browser survived: the current tab is still responsive.
  EXPECT_TRUE(content::ExecJs(web_contents(), "true"));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       AlreadyInstalled_UserDeniesPermission) {
  // Install an app.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);

  // Fire navigator.install but deny permission.
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/false);
  WebInstallDialogShownWatcher launch_dialog_watcher("WebInstallLaunchDialog");
  permissions::PermissionRequestObserver observer(web_contents());
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallFromManifest(
      embedded_https_test_server().GetURL(kValidManifestWithId)));
  observer.Wait();

  // The install should abort before the launch dialog is shown.
  EXPECT_TRUE(observer.request_shown());
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  EXPECT_FALSE(launch_dialog_watcher.shown());
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 0);

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kPermissionDenied, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kPermissionDenied, 1);
}

// The already-installed app's icon cannot be read from disk (e.g. its icon
// data was removed out from under us). The launch flow must fail gracefully
// with an AbortError instead of crashing, and must neither show the launch
// dialog nor launch the app.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       AlreadyInstalled_IconReadFails) {
  // Set up an installed app.
  const GURL install_url = embedded_https_test_server().GetURL(kTestPageWithId);
  const webapps::AppId app_id =
      web_app::InstallWebAppInNewTabAndClose(browser(), install_url);

  // Drain any pending OS-integration/icon writes before deleting the on-disk
  // icons, otherwise a late write could recreate them after deletion.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Delete the installed app's on-disk icon resources so the launch flow's
  // blocking icon read finds no usable icon.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const base::FilePath app_resources_dir =
        web_app::GetManifestResourcesDirectoryForApp(
            web_app::GetWebAppsRootDirectory(browser()->GetProfile()), app_id);
    ASSERT_TRUE(base::DirectoryExists(app_resources_dir));
    ASSERT_TRUE(base::DeletePathRecursively(app_resources_dir));
  }

  // Fire navigator.install.
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  base::HistogramTester histograms;
  WebInstallDialogShownWatcher launch_dialog_watcher("WebInstallLaunchDialog");
  ASSERT_TRUE(TryInstallFromManifest(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  // Verify the install aborts after the failed icon read.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents(),
                           "typeof webInstallError !== 'undefined'")
        .ExtractBool();
  }));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  EXPECT_FALSE(launch_dialog_watcher.shown());
  histograms.ExpectBucketCount("WebApp.LaunchSource", kLaunchSource, 0);

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
}

///////////////////////////////////////////////////////////////////////////////
// Privacy invariant: DataErrors must surface identically in regular and
// Incognito profiles to prevent sites from using error differences to detect
// profile type. These cases are parameterized to run in both modes and assert
// identical behavior.
///////////////////////////////////////////////////////////////////////////////

enum class ProfileMode { kRegular, kIncognito };

class WebInstallFromManifestPrivacyInvariantTest
    : public WebInstallFromManifestBrowserTest,
      public testing::WithParamInterface<ProfileMode> {
 public:
  // Navigates the appropriate browser (the regular browser, or a freshly
  // created Incognito browser depending on the test parameter) to a valid page
  // and returns its WebContents to run the install in.
  content::WebContents* NavigateAndGetWebContents() {
    Browser* test_browser = GetParam() == ProfileMode::kIncognito
                                ? CreateIncognitoBrowser()
                                : browser();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        test_browser, embedded_https_test_server().GetURL("/simple.html")));
    return test_browser->tab_strip_model()->GetActiveWebContents();
  }
};

// A valid manifest without a custom id, and no id option provided, should
// return DataError identically in regular and Incognito modes.
IN_PROC_BROWSER_TEST_P(WebInstallFromManifestPrivacyInvariantTest,
                       MissingManifestId_DataError) {
  base::HistogramTester histograms;
  content::WebContents* wc = NavigateAndGetWebContents();
  GURL manifest_url = embedded_https_test_server().GetURL(kValidManifestNoId);

  ASSERT_TRUE(TryInstallFromManifest(manifest_url, wc));

  ASSERT_TRUE(ErrorExists(wc));
  EXPECT_EQ(GetErrorName(wc), kDataError);

  // Result UMA records the no-custom-id outcome.
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kNoCustomManifestId, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kNoCustomManifestId, 1);
}

// A valid manifest with a custom id, but a non-matching id option, should
// return DataError identically in regular and Incognito modes.
IN_PROC_BROWSER_TEST_P(WebInstallFromManifestPrivacyInvariantTest,
                       MismatchedManifestId_DataError) {
  base::HistogramTester histograms;
  content::WebContents* wc = NavigateAndGetWebContents();
  GURL manifest_id = embedded_https_test_server().GetURL("/wrong-id");

  ASSERT_TRUE(TryInstallFromManifestWithId(
      embedded_https_test_server().GetURL(kValidManifestWithId), manifest_id,
      wc));

  ASSERT_TRUE(ErrorExists(wc));
  EXPECT_EQ(GetErrorName(wc), kDataError);

  // Parse succeeded and id validation ran; result UMA records the
  // manifest-id-mismatch outcome.
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kManifestIdMismatch, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kManifestIdMismatch, 1);
}

// Invalid JSON causes a parse failure that should return DataError identically
// in regular and Incognito modes.
IN_PROC_BROWSER_TEST_P(WebInstallFromManifestPrivacyInvariantTest,
                       InvalidJson_DataError) {
  base::HistogramTester histograms;
  SetDynamicManifestResponse("this is not valid json {{{");

  content::WebContents* wc = NavigateAndGetWebContents();
  GURL manifest_url =
      embedded_https_test_server().GetURL("/dynamic_manifest.json");

  ASSERT_TRUE(TryInstallFromManifest(manifest_url, wc));

  ASSERT_TRUE(ErrorExists(wc));
  EXPECT_EQ(GetErrorName(wc), kDataError);

  // Result UMA records the install-command-failed outcome.
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(
      kInstallResultUma, WebInstallServiceResult::kInstallCommandFailed, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kInstallCommandFailed,
                               1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebInstallFromManifestPrivacyInvariantTest,
                         testing::Values(ProfileMode::kRegular,
                                         ProfileMode::kIncognito),
                         [](const testing::TestParamInfo<ProfileMode>& info) {
                           return info.param == ProfileMode::kIncognito
                                      ? "Incognito"
                                      : "Regular";
                         });

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       Incognito_ValidManifest_ShowsNotSupportedDialog) {
  // A valid manifest in Incognito should show the "not supported" dialog
  // and reject with AbortError (not DataError).
  base::HistogramTester histograms;
  Browser* incognito_browser = CreateIncognitoBrowser();
  NavigateToValidUrl(incognito_browser);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  // Execute async so we can catch the dialog open. We can't wait for the
  // promise, otherwise the dialog would have closed.
  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId),
      incognito_web_contents));

  // Wait for the dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  // Verify dialog title for Incognito mode.
  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installs aren't supported in Incognito mode");

  // Simulate the user accepting the dialog.
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  // Validate JS results - should be AbortError, not DataError.
  ASSERT_TRUE(ErrorExists(incognito_web_contents));
  EXPECT_EQ(GetErrorName(incognito_web_contents), kAbortError);

  // Parse succeeded before the profile check; result UMA records the
  // unsupported-profile outcome.
  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
}

///////////////////////////////////////////////////////////////////////////////
// Guest mode: installs should show the "not supported" dialog.
///////////////////////////////////////////////////////////////////////////////

class WebInstallFromManifestGuestModeTest
    : public WebInstallFromManifestBrowserTest {
 public:
  WebInstallFromManifestGuestModeTest() = default;
  WebInstallFromManifestGuestModeTest(
      const WebInstallFromManifestGuestModeTest&) = delete;
  WebInstallFromManifestGuestModeTest& operator=(
      const WebInstallFromManifestGuestModeTest&) = delete;

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestGuestModeTest,
                       GuestMode_NotSupportedDialog) {
  base::HistogramTester histograms;
#if BUILDFLAG(IS_CHROMEOS)
  Browser* guest_browser = browser();
#else
  Browser* guest_browser = CreateGuestBrowser();
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(guest_browser->GetProfile()->IsGuestSession());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      guest_browser, embedded_https_test_server().GetURL("/simple.html")));

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");
  content::WebContents* guest_web_contents =
      guest_browser->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId),
      guest_web_contents));

  // Wait for the "not supported" dialog.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installs aren't supported in Guest mode");

  views::test::AcceptDialog(widget);
  destroyed.Wait();

  EXPECT_FALSE(ResultExists(guest_web_contents));
  EXPECT_TRUE(ErrorExists(guest_web_contents));
  EXPECT_EQ(GetErrorName(guest_web_contents), kAbortError);

  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
}

///////////////////////////////////////////////////////////////////////////////
// Policy disabled: installs should show the "not supported" dialog.
///////////////////////////////////////////////////////////////////////////////

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class WebInstallFromManifestPolicyDisabledTest
    : public WebInstallFromManifestBrowserTest {
 public:
  WebInstallFromManifestPolicyDisabledTest() = default;
  WebInstallFromManifestPolicyDisabledTest(
      const WebInstallFromManifestPolicyDisabledTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    WebAppBrowserTestBase::SetUpInProcessBrowserTestFixture();

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    policy::PolicyMap policies;
    policies.Set(policy::key::kWebAppInstallByUserEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestPolicyDisabledTest,
                       PolicyDisabled_NotSupportedDialog) {
  base::HistogramTester histograms;
  ASSERT_FALSE(
      web_app::IsWebAppInstallByUserPolicyEnabled(browser()->GetProfile()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL("/simple.html")));

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");

  ASSERT_TRUE(FireInstallFromManifestNoResolve(
      embedded_https_test_server().GetURL(kValidManifestWithId)));

  // Wait for the "not supported" dialog.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installation is blocked by administrator policy.");

  views::test::AcceptDialog(widget);
  destroyed.Wait();

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  histograms.ExpectBucketCount(kInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnsupportedProfile, 1);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

///////////////////////////////////////////////////////////////////////////////
// Install dialog contents.
///////////////////////////////////////////////////////////////////////////////

class WebInstallFromManifestDialogTest
    : public WebInstallFromManifestBrowserTest {
 public:
  SkBitmap ReadImageFile(const base::FilePath& file_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::optional<std::vector<uint8_t>> file_contents =
        base::ReadFileToBytes(file_path);
    return gfx::PNGCodec::Decode(file_contents.value());
  }

  std::u16string GetAppTitle() {
    return u"Manifest test app with id specified";
  }

  base::FilePath GetIconPath() {
    base::FilePath path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &path);
    return path.AppendASCII("banners").AppendASCII("image-512px.png");
  }
};

// Verifies the initiating-origin subtitle, app title, icon, and app origin.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestDialogTest,
                       VerifyInstallDialogContents) {
  NavigateToValidUrl();

  // Uses the same manifest as the URL-flow dialog test, so the dialog contents
  // (title, icon, origin) are expected to match.
  const GURL manifest_url =
      embedded_https_test_server().GetURL(kValidManifestWithId);

  SetPermissionResponse(/*permission_granted=*/true);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppSimpleInstallDialog");

  // The promise only resolves once the dialog is closed, so leave it in-flight
  // to inspect the dialog.
  ASSERT_TRUE(FireInstallFromManifestNoResolve(manifest_url));

  // Wait for the install dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  // Verify the initiating origin subtitle label.
  std::u16string expected_initiating_origin = base::ReplaceStringPlaceholders(
      u"from: 127.0.0.1:$1",
      base::span<const std::u16string>(
          {base::NumberToString16(embedded_https_test_server().port())}),
      nullptr);
  views::BubbleDialogDelegate* const bubble_delegate =
      widget->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(bubble_delegate->GetSubtitle(), expected_initiating_origin);

  views::ElementTrackerViews* tracker_views =
      views::ElementTrackerViews::GetInstance();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(widget);

  // Get the icon from the dialog.
  views::ImageView* icon_view =
      tracker_views->GetUniqueViewAs<views::ImageView>(
          kSimpleInstallDialogIconView, context);
  ASSERT_NE(icon_view, nullptr);

  // Convert to a bitmap.
  const ui::ImageModel& icon_view_model = icon_view->GetImageModel();
  ASSERT_FALSE(icon_view_model.IsEmpty());
  ASSERT_TRUE(icon_view_model.IsImage());
  const SkBitmap* dialog_icon_bitmap = icon_view_model.GetImage().ToSkBitmap();
  CHECK(!dialog_icon_bitmap->isNull());

  // Read the expected bitmap from the test data directory.
  SkBitmap bitmap_from_png = ReadImageFile(GetIconPath());
  CHECK(!bitmap_from_png.isNull());
  // The dialog resizes the icon. Resize the png to match.
  bitmap_from_png = skia::ImageOperations::Resize(
      bitmap_from_png, skia::ImageOperations::RESIZE_BEST,
      dialog_icon_bitmap->width(), dialog_icon_bitmap->height());
  EXPECT_TRUE(
      gfx::test::AreBitmapsClose(*dialog_icon_bitmap, bitmap_from_png, 3));

  // Verify the app title label.
  views::Label* app_title_view = tracker_views->GetUniqueViewAs<views::Label>(
      kSimpleInstallDialogAppTitle, context);
  ASSERT_NE(app_title_view, nullptr);
  EXPECT_EQ(app_title_view->GetText(), GetAppTitle());

  // Verify the app origin label. The manifest's start_url is same-origin with
  // the manifest URL, so the displayed origin matches the test server origin.
  views::Label* origin_view = tracker_views->GetUniqueViewAs<views::Label>(
      kSimpleInstallDialogAppInfoLabel, context);
  ASSERT_NE(origin_view, nullptr);
  EXPECT_EQ(origin_view->GetText(),
            url_formatter::FormatOriginForSecurityDisplay(
                url::Origin::Create(manifest_url),
                url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

}  // namespace
}  // namespace web_app
