// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
constexpr apps::LaunchSource kLaunchSource =
    apps::LaunchSource::kFromWebInstallApi;
constexpr char kNotAllowedError[] = "NotAllowedError";
constexpr char kAbortError[] = "AbortError";
}  // namespace

namespace web_app {

class WebInstallFromUrlCommandBrowserTest : public WebAppBrowserTestBase {
 public:
  WebInstallFromUrlCommandBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppInstallation);
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

  bool TryInstallApp(std::string manifest_id,
                     std::string install_url,
                     bool with_gesture = true) {
    std::string script = "navigator.install('" + manifest_id + "', '" +
                         install_url +
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InstallApp) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  base::HistogramTester tester;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url));

  EXPECT_TRUE(ResultExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id);
  EXPECT_FALSE(ErrorExists());

  tester.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource, 1);
  tester.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  tester.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                            /*sample=*/true, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallAppWithoutUserGesture) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url, /*with_gesture=*/false));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kNotAllowedError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InvalidUrl) {
  NavigateToValidUrl();

  // If the site does not have a manifest, the manifest_id will default to the
  // current document.
  std::string install_url = https_server()->GetURL("/invalid_url").spec();
  std::string manifest_id = install_url;
  base::HistogramTester tester;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  tester.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, NoManifest) {
  NavigateToValidUrl();

  // If the site does not have a manifest, the manifest_id will default to the
  // current document.
  std::string install_url =
      https_server()->GetURL("/banners/no_manifest_test_page.html").spec();
  std::string manifest_id = install_url;
  base::HistogramTester tester;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  tester.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InvalidManifest) {
  NavigateToValidUrl();

  // If the site has an invalid manifest, the manifest_id defaults to the
  // current document.
  std::string install_url =
      https_server()->GetURL("/banners/invalid_manifest_test_page.html").spec();
  std::string manifest_id = install_url;
  base::HistogramTester tester;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  tester.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       ManifestIdMismatch) {
  NavigateToValidUrl();

  // The computed manifest id of this app is the same as the install_url.
  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = https_server()->GetURL("/incorrect_id").spec();
  base::HistogramTester tester;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  tester.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       ManifestWithNoIcons) {
  NavigateToValidUrl();

  // The computed manifest id of this app is the same as the install_url.
  std::string install_url =
      GetAppURLWithManifest("/banners/manifest_no_icon.json").spec();
  std::string manifest_id = install_url;
  base::HistogramTester tester;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  tester.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InvalidInstallUrl) {
  NavigateToValidUrl();

  std::string install_url = "https://invalid.url";
  std::string manifest_id = install_url;
  base::HistogramTester tester;
  ASSERT_TRUE(TryInstallApp(manifest_id, install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  tester.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource, 1);
}

}  // namespace web_app
