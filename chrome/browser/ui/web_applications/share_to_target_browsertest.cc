// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

namespace {

content::EvalJsResult ReadTextContent(content::WebContents* web_contents,
                                      const char* id) {
  const std::string script =
      base::StringPrintf("document.getElementById('%s').textContent", id);
  return content::EvalJs(web_contents, script);
}

class ScopedSharesheetAppSelection {
 public:
  explicit ScopedSharesheetAppSelection(const std::string& app_id) {
    SetSelectedSharesheetApp(app_id);
  }

  ScopedSharesheetAppSelection(const ScopedSharesheetAppSelection&) = delete;
  ScopedSharesheetAppSelection& operator=(const ScopedSharesheetAppSelection&) =
      delete;

  ~ScopedSharesheetAppSelection() { SetSelectedSharesheetApp(std::string()); }

 private:
  static void SetSelectedSharesheetApp(const std::string& app_id) {
    sharesheet::SharesheetService::SetSelectedAppForTesting(
        base::UTF8ToUTF16(app_id));
  }
};

}  // namespace

namespace web_app {

class ShareToTargetBrowserTest : public WebAppBrowserTestBase,
                                 public testing::WithParamInterface<
                                     apps::test::LinkCapturingFeatureVersion> {
 public:
  ShareToTargetBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam()), {});
  }

  std::string ExecuteShare(const std::string& script) {
    const GURL url = https_server()->GetURL("/webshare/index.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::WebContents* const contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(contents, script).ExtractString();
  }

  content::WebContents* ShareToTarget(const std::string& script) {
    ui_test_utils::AllBrowserTabAddedWaiter waiter;
    EXPECT_EQ("share succeeded", ExecuteShare(script));

    content::WebContents* contents = waiter.Wait();
    EXPECT_TRUE(content::WaitForLoadStop(contents));

    // For Ash builds, we could verify no files have been added to Recent Files.

    return contents;
  }

  void InstallWebAppFromManifest(const GURL& app_url) {
    DCHECK(app_id_.empty());
    app_id_ = web_app::InstallWebAppFromManifest(browser(), app_url);
    // Enabling link capturing to ensure it doesn't interfere.
    EXPECT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_id_),
              base::ok());
  }

  const webapps::AppId& app_id() const { return app_id_; }

 private:
  // WebAppBrowserTestBase:
  void TearDownOnMainThread() override {
    if (!app_id_.empty()) {
      CloseAppWindows(app_id_);
    }
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  static void CloseAppWindows(const webapps::AppId& app_id) {
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&app_id](BrowserWindowInterface* browser) {
          const web_app::AppBrowserController* const app_controller =
              web_app::AppBrowserController::From(browser);
          if (app_controller && app_controller->app_id() == app_id) {
            browser->GetWindow()->Close();
          }
          return true;
        });
  }

  webapps::AppId app_id_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ShareToTargetBrowserTest, ShareToPosterWebApp) {
  const GURL app_url = https_server()->GetURL("/web_share_target/poster.html");
  InstallWebAppFromManifest(app_url);
  ScopedSharesheetAppSelection selection(app_id());

  // Poster web app does not accept image shares.
  EXPECT_EQ("share failed: AbortError: Share canceled",
            ExecuteShare("share_single_file()"));

  content::WebContents* web_contents = ShareToTarget("share_title()");
  EXPECT_EQ("Subject", ReadTextContent(web_contents, "headline"));

  web_contents = ShareToTarget("share_url()");
  EXPECT_EQ("https://example.com/", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_P(ShareToTargetBrowserTest, ShareToChartsWebApp) {
  const GURL app_url = https_server()->GetURL("/web_share_target/charts.html");
  InstallWebAppFromManifest(app_url);
  ScopedSharesheetAppSelection selection(app_id());

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ("************", ReadTextContent(web_contents, "notes"));

  web_contents = ShareToTarget("share_url()");
  EXPECT_EQ("https://example.com/", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_P(ShareToTargetBrowserTest, ShareImage) {
  const GURL app_url =
      https_server()->GetURL("/web_share_target/multimedia.html");
  InstallWebAppFromManifest(app_url);
  ScopedSharesheetAppSelection selection(app_id());

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ(std::string(12, '*'), ReadTextContent(web_contents, "image"));
  EXPECT_EQ("sample.webp", ReadTextContent(web_contents, "image_filename"));
}

IN_PROC_BROWSER_TEST_P(ShareToTargetBrowserTest, ShareMultimedia) {
  const GURL app_url =
      https_server()->GetURL("/web_share_target/multimedia.html");
  InstallWebAppFromManifest(app_url);
  ScopedSharesheetAppSelection selection(app_id());

  content::WebContents* web_contents = ShareToTarget("share_multiple_files()");
  EXPECT_EQ(std::string(345, '*'), ReadTextContent(web_contents, "audio"));
  EXPECT_EQ(std::string(67890, '*'), ReadTextContent(web_contents, "video"));
  EXPECT_EQ(std::string(1, '*'), ReadTextContent(web_contents, "image"));
  EXPECT_EQ("sam.ple.mp3", ReadTextContent(web_contents, "audio_filename"));
  EXPECT_EQ("sample.mp4", ReadTextContent(web_contents, "video_filename"));
  EXPECT_EQ("sam_ple.gif", ReadTextContent(web_contents, "image_filename"));
}

IN_PROC_BROWSER_TEST_P(ShareToTargetBrowserTest, ShareToPartialWild) {
  const GURL app_url =
      https_server()->GetURL("/web_share_target/partial-wild.html");
  InstallWebAppFromManifest(app_url);
  ScopedSharesheetAppSelection selection(app_id());

  // Partial Wild does not accept text shares.
  EXPECT_EQ("share failed: AbortError: Share canceled",
            ExecuteShare("share_title()"));

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ("************", ReadTextContent(web_contents, "graphs"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ShareToTargetBrowserTest,
    // Ensure share target still works with navigation capturing v2.
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV1DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::
                        kV2DefaultOffCaptureExistingFrames),
    apps::test::LinkCapturingVersionToString);

}  // namespace web_app
