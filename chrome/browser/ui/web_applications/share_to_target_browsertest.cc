// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/run_loop.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#else
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

content::EvalJsResult ReadTextContent(content::WebContents* web_contents,
                                      const char* id) {
  const std::string script =
      base::StringPrintf("document.getElementById('%s').textContent", id);
  return content::EvalJs(web_contents, script);
}

}  // namespace

namespace web_app {

class ShareToTargetBrowserTest : public WebAppControllerBrowserTest {
 public:
  void SetSelectedSharesheetApp(const AppId& app_id) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    base::RunLoop run_loop;
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::TestController>()
        ->SetSelectedSharesheetApp(app_id, run_loop.QuitClosure());
    run_loop.Run();
#else
    sharesheet::SharesheetService::SetSelectedAppForTesting(
        base::UTF8ToUTF16(app_id));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
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

  bool IsServiceAvailable() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // If ash is does not contain the relevant test controller functionality,
    // then there's nothing to do for this test.
    if (chromeos::LacrosService::Get()->GetInterfaceVersion(
            crosapi::mojom::TestController::Uuid_) <
        static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                             kSetSelectedSharesheetAppMinVersion)) {
      LOG(WARNING) << "Unsupported ash version.";
      return false;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(ShareToTargetBrowserTest, ShareToPosterWebApp) {
  if (!IsServiceAvailable())
    return;

  const GURL app_url = https_server()->GetURL("/web_share_target/poster.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  SetSelectedSharesheetApp(app_id);

  // Poster web app does not accept image shares.
  EXPECT_EQ("share failed: AbortError: Share canceled",
            ExecuteShare("share_single_file()"));

  content::WebContents* web_contents = ShareToTarget("share_title()");
  EXPECT_EQ("Subject", ReadTextContent(web_contents, "headline"));

  web_contents = ShareToTarget("share_url()");
  EXPECT_EQ("https://example.com/", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(ShareToTargetBrowserTest, ShareToChartsWebApp) {
  if (!IsServiceAvailable())
    return;

  const GURL app_url = https_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  SetSelectedSharesheetApp(app_id);

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ("************", ReadTextContent(web_contents, "notes"));

  web_contents = ShareToTarget("share_url()");
  EXPECT_EQ("https://example.com/", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(ShareToTargetBrowserTest, ShareImage) {
  if (!IsServiceAvailable())
    return;

  const GURL app_url =
      https_server()->GetURL("/web_share_target/multimedia.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  SetSelectedSharesheetApp(app_id);

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ(std::string(12, '*'), ReadTextContent(web_contents, "image"));
  EXPECT_EQ("sample.webp", ReadTextContent(web_contents, "image_filename"));
}

IN_PROC_BROWSER_TEST_F(ShareToTargetBrowserTest, ShareMultimedia) {
  if (!IsServiceAvailable())
    return;

  const GURL app_url =
      https_server()->GetURL("/web_share_target/multimedia.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  SetSelectedSharesheetApp(app_id);

  content::WebContents* web_contents = ShareToTarget("share_multiple_files()");
  EXPECT_EQ(std::string(345, '*'), ReadTextContent(web_contents, "audio"));
  EXPECT_EQ(std::string(67890, '*'), ReadTextContent(web_contents, "video"));
  EXPECT_EQ(std::string(1, '*'), ReadTextContent(web_contents, "image"));
  EXPECT_EQ("sam.ple.mp3", ReadTextContent(web_contents, "audio_filename"));
  EXPECT_EQ("sample.mp4", ReadTextContent(web_contents, "video_filename"));
  EXPECT_EQ("sam_ple.gif", ReadTextContent(web_contents, "image_filename"));
}

IN_PROC_BROWSER_TEST_F(ShareToTargetBrowserTest, ShareToPartialWild) {
  if (!IsServiceAvailable())
    return;

  const GURL app_url =
      https_server()->GetURL("/web_share_target/partial-wild.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  SetSelectedSharesheetApp(app_id);

  // Partial Wild does not accept text shares.
  EXPECT_EQ("share failed: AbortError: Share canceled",
            ExecuteShare("share_title()"));

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ("************", ReadTextContent(web_contents, "graphs"));
}

}  // namespace web_app
