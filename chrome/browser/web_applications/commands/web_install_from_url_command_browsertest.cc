// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/command_metrics_test_helper.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
constexpr apps::LaunchSource kLaunchSource =
    apps::LaunchSource::kFromWebInstallApi;
constexpr char kAbortError[] = "AbortError";
constexpr char kDataError[] = "DataError";
}  // namespace

namespace web_app {

class WebInstallFromUrlCommandBrowserTest : public WebAppBrowserTestBase {
 public:
  WebInstallFromUrlCommandBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppInstallation);
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    secondary_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(secondary_server_.Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Tests start on an about:blank page. We need to navigate to any valid URL
  // before we can execute `navigator.install()`
  void NavigateToValidUrl() {
    VLOG(0) << https_server()->GetURL("/simple.html").spec();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server()->GetURL("/simple.html")));
  }

  // When the permission prompt shows, it must be granted or denied.
  void SetPermissionResponse(bool permission_granted) {
    permissions::PermissionRequestManager::AutoResponseType response =
        permission_granted
            ? permissions::PermissionRequestManager::AutoResponseType::
                  ACCEPT_ALL
            : permissions::PermissionRequestManager::AutoResponseType::DENY_ALL;

    permissions::PermissionRequestManager::FromWebContents(web_contents())
        ->set_auto_response_for_test(response);
  }

  // 2 param navigator.install(install_url, manifest_id)
  bool TryInstallApp(std::string install_url, std::string manifest_id) {
    std::string script = "navigator.install('" + install_url + "', '" +
                         manifest_id +
                         "').then(result => {"
                         "  webInstallResult = result;"
                         "}).catch(error => {"
                         "  webInstallError = error;"
                         "});";
    return ExecJs(web_contents(), script);
  }

  // 1 param navigator.install(install_url)
  bool TryInstallApp(std::string install_url) {
    std::string script = "navigator.install('" + install_url +
                         "').then(result => {"
                         "  webInstallResult = result;"
                         "}).catch(error => {"
                         "  webInstallError = error;"
                         "});";

    return ExecJs(web_contents(), script);
  }

  bool ResultExists() {
    // ExecJs returns false when an error is thrown, including when a variable
    // is undefined.
    return ExecJs(web_contents(), "webInstallResult");
  }

  bool ErrorExists() {
    // ExecJs returns false when an error is thrown, including when a variable
    // is undefined.
    return ExecJs(web_contents(), "webInstallError");
  }

  std::string GetManifestIdResult() {
    return EvalJs(web_contents(), "webInstallResult.manifestId")
        .ExtractString();
  }

  std::string GetErrorName() {
    return EvalJs(web_contents(), "webInstallError.name").ExtractString();
  }

 protected:
  net::EmbeddedTestServer secondary_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

///////////////////////////////////////////////////////////////////////////////
// Intended use cases -- 1 and 2 parameter -- for sites that meet
// all manifest id requirements. We expect successful installs here.
///////////////////////////////////////////////////////////////////////////////
IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_OneParam) {
  NavigateToValidUrl();

  // Requires an `install_url` of a document with an `id` field in its
  // manifest.json.
  std::string install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html").spec();

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallApp(install_url));

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_TwoParam) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

///////////////////////////////////////////////////////////////////////////////
// Permissions handling
///////////////////////////////////////////////////////////////////////////////
IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_SameOrigin_AllowPermission) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_TRUE(ResultExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id);
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_SameOrigin_DenyPermission) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  SetPermissionResponse(/*permission_granted=*/false);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_CrossOrigin_AllowPermission) {
  // Navigate to a valid URL on the primary server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/simple.html")));

  std::string install_url =
      secondary_server_
          .GetURL("/banners/manifest_test_page.html?manifest=manifest.json")
          .spec();
  std::string manifest_id =
      secondary_server_.GetURL("/banners/manifest_test_page.html").spec();
  base::HistogramTester histograms;

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_TRUE(ResultExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id);
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_CrossOrigin_DenyPermission) {
  // Navigate to a valid URL on the primary server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/simple.html")));

  std::string install_url =
      secondary_server_
          .GetURL("/banners/manifest_test_page.html?manifest=manifest.json")
          .spec();
  std::string manifest_id =
      secondary_server_.GetURL("/banners/manifest_test_page.html").spec();
  SetPermissionResponse(/*permission_granted=*/false);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

///////////////////////////////////////////////////////////////////////////////
// Error cases - bad manifests, invalid URLs, etc
///////////////////////////////////////////////////////////////////////////////
IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, NoManifest) {
  NavigateToValidUrl();

  // If the site does not have a manifest, the manifest_id will default to the
  std::string install_url =
      https_server()->GetURL("/banners/no_manifest_test_page.html").spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(
      histograms,
      test::ForAllGetAllSamples(
          test::GetInstallCommandResultHistogramNames(".WebInstallFromUrl",
                                                      ".Crafted"),
          base::BucketsAre(base::Bucket(
              webapps::InstallResultCode::kNotValidManifestForWebApp, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InvalidManifest) {
  NavigateToValidUrl();

  // If the site has an invalid manifest, the manifest_id defaults to the
  std::string install_url =
      https_server()->GetURL("/banners/invalid_manifest_test_page.html").spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(
      histograms,
      test::ForAllGetAllSamples(
          test::GetInstallCommandResultHistogramNames(".WebInstallFromUrl",
                                                      ".Crafted"),
          base::BucketsAre(base::Bucket(
              webapps::InstallResultCode::kNotValidManifestForWebApp, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       ManifestIdMismatch) {
  NavigateToValidUrl();

  // The computed manifest id of this app is the same as the install_url.
  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = https_server()->GetURL("/incorrect_id").spec();
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kDataError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kManifestIdMismatch, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, ManifestMissingId) {
  NavigateToValidUrl();

  // No id specified in the manifest.json
  std::string install_url = GetInstallableAppURL().spec();
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);

  ASSERT_TRUE(TryInstallApp(install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kDataError);

  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kNoCustomManifestId, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       ManifestWithNoIcons) {
  NavigateToValidUrl();

  // The computed manifest id of this app is the same as the install_url.
  std::string install_url =
      GetAppURLWithManifest("/banners/manifest_no_icon.json").spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(
      histograms,
      test::ForAllGetAllSamples(
          test::GetInstallCommandResultHistogramNames(".WebInstallFromUrl",
                                                      ".Crafted"),
          base::BucketsAre(base::Bucket(
              webapps::InstallResultCode::kNotValidManifestForWebApp, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InvalidInstallUrl) {
  NavigateToValidUrl();

  // If the site does not have a manifest, the manifest_id will default to the
  // current document.
  std::string install_url = "https://invalid.url";
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kInstallURLLoadFailed, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

class WebInstallFromUrlCommandDialogTest
    : public WebInstallFromUrlCommandBrowserTest {
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
    return path.AppendASCII("banners").AppendASCII("launcher-icon-1x.png");
  }
};

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandDialogTest,
                       VerifyInstallDialogContents) {
  // Go to /simple.html
  NavigateToValidUrl();

  // Target a different page to install.
  const GURL install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");

  SetPermissionResponse(/*permission_granted=*/true);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppSimpleInstallDialog");

  // We don't actually care about the result of the install, and EvalJs blocks
  // until the promise resolves, which only happens after the dialog is closed.
  // Execute the install asynchronously so we can actually check the dialog
  // contents without the promise timing out.
  ExecuteScriptAsync(web_contents(),
                     "navigator.install('" + install_url.spec() + "');");

  // Wait for the install dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

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
  base::FilePath path = GetIconPath();
  SkBitmap bitmap_from_png = ReadImageFile(path);
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

  // Verify the origin label.
  views::Label* start_url_view = tracker_views->GetUniqueViewAs<views::Label>(
      kSimpleInstallDialogOriginLabel, context);
  ASSERT_NE(start_url_view, nullptr);
  EXPECT_EQ(
      start_url_view->GetText(),
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          install_url));
}
}  // namespace web_app
