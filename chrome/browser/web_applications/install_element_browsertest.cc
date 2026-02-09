// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_install_service_impl.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
constexpr char kInstallElementId[] = "install-app";
constexpr char kInstallDialogName[] = "WebAppSimpleInstallDialog";
constexpr char kInstallResultUma[] = "WebApp.WebInstallElement.Result";
constexpr char kInstallTypeUma[] = "WebApp.WebInstallElement.InstallType";
constexpr char kVariantedInstallTypeUma[] =
    "WebApp.WebInstallService.Element.InstallType";
constexpr char kVariantedInstallResultUma[] =
    "WebApp.WebInstallService.Element.Result";
constexpr char kInstallElementPageStartUrl[] =
    "/web_apps/install_element/index.html";
constexpr char kInstallElementPageId[] = "/some_id";
constexpr char kCustomIdPageInstallUrl[] =
    "/web_apps/custom_id/install_url.html";
constexpr char kCustomIdPageId[] = "/some_id";
constexpr char kNoCustomIdPageInstallUrl[] =
    "/web_apps/install_url/install_url.html";
// Since this page has no custom id, it defaults to start_url.
constexpr char kNoCustomIdPageId[] = "/web_apps/install_url/index.html";
constexpr char kElementRequestingPageUkm[] = "ElementResultByRequestingPage";
constexpr char kElementInstalledAppUkm[] = "ElementResultByInstalledApp";
}  // namespace

namespace web_app {

class InstallElementBrowserTest : public WebAppBrowserTestBase {
 public:
  InstallElementBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kInstallElement,
         blink::features::kBypassPepcSecurityForTesting},
        {});
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    console_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(web_contents());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void BlockWebInstallPermission(const GURL& url) {
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    settings_map->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::WEB_APP_INSTALLATION,
        CONTENT_SETTING_BLOCK);
  }

  bool SetButtonInstallUrl(const GURL& install_url) {
    const std::string script =
        "document.getElementById('" + std::string(kInstallElementId) +
        "').setAttribute('installurl', '" + install_url.spec() + "');";
    return content::ExecJs(web_contents(), script);
  }

  bool SetButtonManifestId(const GURL& manifest_id) {
    const std::string script =
        "document.getElementById('" + std::string(kInstallElementId) +
        "').setAttribute('manifestid', '" + manifest_id.spec() + "');";
    return content::ExecJs(web_contents(), script);
  }

  // Simulates a click on an element with the given |id|.
  bool ClickElementWithId(const std::string& id,
                          content::WebContents* contents = nullptr) {
    const std::string script = "document.getElementById('" + id + "').click();";
    return content::ExecJs(contents ? contents : web_contents(), script);
  }

  void WaitForPromptActionEvent(const std::string& id) {
    ExpectConsoleMessage(id + "-promptaction");
  }

  void WaitForDismissEvent(const std::string& id) {
    ExpectConsoleMessage(id + "-promptdismiss");
  }

  // The web app test pages log quite a few additional console messages during
  // setup/load. Make sure that the set of received messages contains at least 1
  // instance of `expected_message`.
  void ExpectConsoleMessage(const std::string& expected_message) {
    EXPECT_TRUE(console_observer_->Wait());
    EXPECT_GE(console_observer_->messages().size(), 1u);

    bool found = false;
    for (const auto& message : console_observer_->messages()) {
      if (base::UTF16ToUTF8(message.message) == expected_message) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Expected console message not found: "
                       << expected_message;

    // Reset console observer to wait for next message.
    console_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(web_contents());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContentsConsoleObserver> console_observer_;
};

// Test installing current document (no attributes).
// <install></install>
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, Install) {
  // Setup histogram tester before navigation so it captures the WebDX feature
  // counter recorded when the <install> element is parsed on page load.
  base::HistogramTester histograms;

  // Navigate to a page with <install> elements.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Setup test listeners and dialog auto-accepts.
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(),
            u"Web app install element test app with id");

  // Verify the app is installed.
  webapps::AppId app_id =
      GenerateAppIdFromManifestId(webapps::ManifestId(https_server()->GetURL("/some_id")));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  // Check use counter.
  histograms.ExpectBucketCount(
      "Blink.UseCounter.WebDXFeatures",
      blink::mojom::WebDXFeature::kDRAFT_InstallElement, 1);

  histograms.ExpectBucketCount(kInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      kInstallTypeUma, web_app::WebInstallServiceType::kCurrentDocument, 1);
  // Check the varianted UMAs.
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               web_app::WebInstallServiceType::kCurrentDocument,
                               1);
}

// Test installing from a background document (installurl only).
// <install installurl="..."></install>
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, InstallWithUrl) {
  // Setup histogram tester before navigation so it captures the WebDX feature
  // counter recorded when the <install> element is parsed on page load.
  base::HistogramTester histograms;

  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Setup test listeners and dialog auto-accepts.
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Dynamically set the installurl attribute.
  // Since we're installing by URL only, the manifest must contain an id.
  const GURL install_url = https_server()->GetURL(kCustomIdPageInstallUrl);
  ASSERT_TRUE(SetButtonInstallUrl(install_url));

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(), u"Simple web app with a custom id");

  // Verify the app is installed.
  webapps::AppId app_id = GenerateAppIdFromManifestId(
      webapps::ManifestId(https_server()->GetURL(kInstallElementPageId)));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  // Check use counter.
  histograms.ExpectBucketCount(
      "Blink.UseCounter.WebDXFeatures",
      blink::mojom::WebDXFeature::kDRAFT_InstallElement, 1);

  histograms.ExpectBucketCount(kInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      kInstallTypeUma, web_app::WebInstallServiceType::kBackgroundDocument, 1);
  // Check the varianted UMAs.
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallTypeUma,
      web_app::WebInstallServiceType::kBackgroundDocument, 1);

  // Verify UKM entries for element-triggered install.
  auto ukm_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::WebApp_WebInstall::kEntryName);
  ASSERT_EQ(2u, ukm_entries.size());
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[0], kElementRequestingPageUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kSuccess));
  // First entry should be of source type, NAVIGATION_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[0]->source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[1], kElementInstalledAppUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kSuccess));
  // Second entry should be of source type, APP_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[1]->source_id),
            ukm::SourceIdType::APP_ID);
}

// Test installing from a background document (both installurl and manifestid).
// <install installurl="..." manifestid="..."></install>
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, InstallWithUrlAndId) {
  // Setup histogram tester before navigation so it captures the WebDX feature
  // counter recorded when the <install> element is parsed on page load.
  base::HistogramTester histograms;

  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Setup test listeners and dialog auto-accepts.
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Dynamically set the installurl and manifestid attributes.
  const GURL install_url = https_server()->GetURL(kNoCustomIdPageInstallUrl);
  ASSERT_TRUE(SetButtonInstallUrl(install_url));
  const GURL manifest_id = https_server()->GetURL(kNoCustomIdPageId);
  ASSERT_TRUE(SetButtonManifestId(manifest_id));

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(), u"Simple web app");

  // Verify the app is installed.
  webapps::AppId app_id = GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  // Check use counter.
  histograms.ExpectBucketCount(
      "Blink.UseCounter.WebDXFeatures",
      blink::mojom::WebDXFeature::kDRAFT_InstallElement, 1);

  histograms.ExpectBucketCount(kInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      kInstallTypeUma, web_app::WebInstallServiceType::kBackgroundDocument, 1);
  // Check the varianted UMAs.
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallTypeUma,
      web_app::WebInstallServiceType::kBackgroundDocument, 1);

  // Verify UKM entries for element-triggered install.
  auto ukm_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::WebApp_WebInstall::kEntryName);
  ASSERT_EQ(2u, ukm_entries.size());
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[0], kElementRequestingPageUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kSuccess));
  // First entry should be of source type, NAVIGATION_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[0]->source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[1], kElementInstalledAppUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kSuccess));
  // Second entry should be of source type, APP_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[1]->source_id),
            ukm::SourceIdType::APP_ID);
}

IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, InstallWithUrl_UserDenies) {
  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Simulate the user declining the install prompt.
  auto auto_decline_pwa_install_confirmation =
      SetAutoDeclinePWAInstallConfirmationForTesting();
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Dynamically set the installurl attribute.
  // Since we're installing by URL only, the manifest must contain an id.
  const GURL install_url = https_server()->GetURL(kCustomIdPageInstallUrl);
  ASSERT_TRUE(SetButtonInstallUrl(install_url));

  // Click the install element.
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));

  // Verify promptdismiss event was fired.
  WaitForDismissEvent(kInstallElementId);

  // Verify the app is not installed.
  webapps::AppId app_id =
      GenerateAppIdFromManifestId(webapps::ManifestId(https_server()->GetURL(kCustomIdPageId)));
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallServiceResult::kCanceledByUser, 1);
  histograms.ExpectBucketCount(
      kInstallTypeUma, web_app::WebInstallServiceType::kBackgroundDocument, 1);
  // Check the varianted UMAs.
  histograms.ExpectBucketCount(
      kVariantedInstallResultUma,
      web_app::WebInstallServiceResult::kCanceledByUser, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallTypeUma,
      web_app::WebInstallServiceType::kBackgroundDocument, 1);

  // Verify UKM entries for element-triggered install cancellation.
  auto ukm_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::WebApp_WebInstall::kEntryName);
  ASSERT_EQ(2u, ukm_entries.size());
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[0], kElementRequestingPageUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kCanceledByUser));
  // First entry should be of source type, NAVIGATION_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[0]->source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[1], kElementInstalledAppUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kCanceledByUser));
  // Second entry should be of source type, APP_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[1]->source_id),
            ukm::SourceIdType::APP_ID);
}

// Test that current document install succeeds even when permission is denied,
// since current document installs bypass permission.
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, Install_DenyPermission) {
  // Navigate to a page with <install> elements.
  const GURL current_document_url =
      https_server()->GetURL(kInstallElementPageStartUrl);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), current_document_url));

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  base::HistogramTester histograms;

  // Block the web install permission for the current document origin.
  BlockWebInstallPermission(current_document_url);

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(),
            u"Web app install element test app with id");

  // Verify the app is installed.
  webapps::AppId app_id = GenerateAppIdFromManifestId(
      webapps::ManifestId(https_server()->GetURL(kInstallElementPageId)));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount(kInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      kInstallTypeUma, web_app::WebInstallServiceType::kCurrentDocument, 1);
  // Check the varianted UMAs.
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               web_app::WebInstallServiceResult::kSuccess, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               web_app::WebInstallServiceType::kCurrentDocument,
                               1);
}

// Test that when permission is denied for background document install, install
// still occurs. <install> elements bypass permission.
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest,
                       InstallWithUrl_IgnoresDeniedPermission) {
  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Setup test listeners and dialog auto-accepts.
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();

  // Dynamically set the installurl attribute to a background document URL.
  const GURL install_url = https_server()->GetURL(kCustomIdPageInstallUrl);
  ASSERT_TRUE(SetButtonInstallUrl(install_url));
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Block the web install permission for this origin.
  BlockWebInstallPermission(install_url);

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(), u"Simple web app with a custom id");

  // Verify the app is installed.
  webapps::AppId app_id =
      GenerateAppIdFromManifestId(webapps::ManifestId(https_server()->GetURL(kCustomIdPageId)));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // The element does not check or use the permission.
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallServiceResult::kPermissionDenied,
      0);
  histograms.ExpectBucketCount(
      kInstallTypeUma, web_app::WebInstallServiceType::kBackgroundDocument, 1);
  // Check the varianted UMAs.
  histograms.ExpectBucketCount(
      kVariantedInstallResultUma,
      web_app::WebInstallServiceResult::kPermissionDenied, 0);
  histograms.ExpectBucketCount(
      kVariantedInstallTypeUma,
      web_app::WebInstallServiceType::kBackgroundDocument, 1);

  // Verify UKM entries for element-triggered install.
  auto ukm_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::WebApp_WebInstall::kEntryName);
  ASSERT_EQ(2u, ukm_entries.size());
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[0], kElementRequestingPageUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kSuccess));
  // First entry should be of source type, NAVIGATION_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[0]->source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder.ExpectEntryMetric(
      ukm_entries[1], kElementInstalledAppUkm,
      static_cast<int>(web_app::WebInstallServiceResult::kSuccess));
  // Second entry should be of source type, APP_ID.
  EXPECT_EQ(ukm::GetSourceIdType(ukm_entries[1]->source_id),
            ukm::SourceIdType::APP_ID);
}

IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest,
                       InstallWithUrl_AlreadyInstalled) {
  // There should be no apps installed initially.
  EXPECT_EQ(provider().registrar_unsafe().GetAppIds().size(), 0u);

  base::HistogramTester histograms;

  // Install a background document and close the app window.
  const GURL background_doc_install_url =
      https_server()->GetURL(kCustomIdPageInstallUrl);
  webapps::AppId installed_app_id =
      web_app::InstallWebAppFromPageAndCloseAppBrowser(
          browser(), background_doc_install_url);

  // Generate the app id from the manifest id and verify it matches the app just
  // installed.
  const GURL manifest_id = https_server()->GetURL(kCustomIdPageId);
  webapps::AppId generated_app_id = GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id));
  EXPECT_EQ(installed_app_id, generated_app_id);

  // Verify that the app was installed and launched.
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      generated_app_id, WebAppFilter::LaunchableFromInstallApi()));
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);

  // Now navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Dynamically set the installurl attribute to the background document just
  // installed.
  ASSERT_TRUE(SetButtonInstallUrl(background_doc_install_url));
  base::AutoReset<bool> auto_accept =
      SetAutoAcceptWebInstallLaunchDialogForTesting();

  // Click the install element.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));
  browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app is still installed.
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      generated_app_id, WebAppFilter::LaunchableFromInstallApi()));

  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromWebInstallApi, 1);
  histograms.ExpectBucketCount(
      kInstallResultUma,
      web_app::WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
  histograms.ExpectBucketCount(
      kInstallTypeUma, web_app::WebInstallServiceType::kBackgroundDocument, 1);
  // Check the varianted UMAs.
  histograms.ExpectBucketCount(
      kVariantedInstallResultUma,
      web_app::WebInstallServiceResult::kSuccessAlreadyInstalled, 1);
  histograms.ExpectBucketCount(
      kVariantedInstallTypeUma,
      web_app::WebInstallServiceType::kBackgroundDocument, 1);
}

// Tests the case where an app is already installed on initial page load, then
// uninstalled, and the element is clicked without reloading the page. We expect
// this to behave like a fresh install.
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest,
                       InstallWithUrl_AlreadyInstalledThenUninstalled) {
  // Step 1: Preinstall a background document.
  const GURL background_doc_install_url =
      https_server()->GetURL(kCustomIdPageInstallUrl);
  const GURL manifest_id = https_server()->GetURL(kCustomIdPageId);

  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), background_doc_install_url);
  EXPECT_EQ(app_id, GenerateAppIdFromManifestId(webapps::ManifestId(manifest_id)));

  // Verify that the app was installed.
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Step 2: Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Dynamically set the installurl attribute to the installed app.
  ASSERT_TRUE(SetButtonInstallUrl(background_doc_install_url));

  // Step 3: Uninstall the app and verify it's no longer installed.
  test::UninstallWebApp(profile(), app_id);
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));

  // Step 4: Without refreshing the page, click the install element button.
  // The button text still says "Launch" but the app is uninstalled, so we
  // expect the install dialog (not the launch dialog) to show.

  // Set up to wait for the install dialog.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);

  // Click the install element asynchronously so we can wait for the dialog.
  content::ExecuteScriptAsync(
      web_contents(), "document.getElementById('" +
                          std::string(kInstallElementId) + "').click();");

  // Step 5: Verify that the install dialog shows.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  // Step 6: Accept the dialog.
  views::test::AcceptDialog(widget);

  // Verify promptaction event was fired and the app installed.
  WaitForPromptActionEvent(kInstallElementId);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::LaunchableFromInstallApi()));
}

///////////////////////////////////////////////////////////////////////////////
// Bad input error cases - bad manifests, invalid URLs, etc
///////////////////////////////////////////////////////////////////////////////

IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, InvalidInstallUrl) {
  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kInstallElementPageStartUrl)));

  // Dynamically set an invalid installurl attribute.
  const GURL invalid_url = GURL("https://invalid.url");
  ASSERT_TRUE(SetButtonInstallUrl(invalid_url));

  // Click the install element.
  ASSERT_TRUE(ClickElementWithId(kInstallElementId));

  // No installation should have occurred due to the invalid URL.
  // We cannot generate a valid app_id from an invalid URL, so we just verify
  // that the dismiss event was fired as expected.
  // TODO(crbug.com/462493894): Decide how to surface kDataError. For now,
  // promptdismiss is used for all error cases.
  WaitForDismissEvent(kInstallElementId);
}

}  // namespace web_app
