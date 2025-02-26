// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/command_metrics_test_helper.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
constexpr apps::LaunchSource kLaunchSource =
    apps::LaunchSource::kFromWebInstallApi;
constexpr char kNotAllowedError[] = "NotAllowedError";
constexpr char kAbortError[] = "AbortError";
constexpr char kTypeError[] = "TypeError";
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

  // 2 param navigator.install()
  bool TryInstallApp(std::string install_url,
                     std::string manifest_id,
                     bool with_gesture = true) {
    std::string script = "navigator.install('" + install_url + "', '" +
                         manifest_id +
                         "').then(result => {"
                         "  webInstallResult = result;"
                         "}).catch(error => {"
                         "  webInstallError = error;"
                         "});";

    if (with_gesture) {
      return ExecJs(web_contents(), script);
    } else {
      return ExecJs(web_contents(), script,
                    content::EXECUTE_SCRIPT_NO_USER_GESTURE);
    }
  }

  // 1 param navigator.install()
  bool TryInstallAppFromInstallUrlOnly(std::string install_url) {
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

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppSameOrigin_AllowPermission) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
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
                       InstallAppSameOrigin_DenyPermission) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  SetPermissionResponse(/*permission_granted=*/false);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

// Basic test to exercise 1-param `navigator.install()` API end to end.
IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppCrossOriginFromInstallUrlOnly) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  SetPermissionResponse(/*permission_granted=*/true);
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallAppFromInstallUrlOnly(install_url));

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
                       InstallAppWithoutUserGesture) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id, /*with_gesture=*/false));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kNotAllowedError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_OneParam_Undefined) {
  NavigateToValidUrl();

  std::string script =
      "let install_url;"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_OneParam_Null) {
  NavigateToValidUrl();

  std::string script =
      "let install_url=null;"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_OneParam_Number) {
  NavigateToValidUrl();

  std::string script =
      "let install_url = new Number(1);"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_OneParam_Empty) {
  NavigateToValidUrl();

  std::string script =
      "let install_url='';"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_TwoParams_UndefinedInstallUrl) {
  NavigateToValidUrl();

  std::string manifest_id = GetInstallableAppURL().spec();
  std::string script =
      "let install_url;"
      "navigator.install(install_url, '" +
      manifest_id +
      "').then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_TwoParams_UndefinedManifestId) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string script =
      "let manifest_id;"
      "navigator.install('" +
      install_url +
      "', manifest_id).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_TwoParams_EmptyManifestId) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string script =
      "let manifest_id = '';"
      "navigator.install('" +
      install_url +
      "', manifest_id).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppBadInput_TwoParams_NullManifestId) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string script =
      "let manifest_id = null;"
      "navigator.install('" +
      install_url +
      "', manifest_id).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

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
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kNotInstallable, 1))));
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

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppCrossOrigin_AllowPermission) {
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
                       InstallAppCrossOrigin_DenyPermission) {
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

}  // namespace web_app
