// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

namespace {

apps::AppServiceProxyChromeOs* GetAppServiceProxy(Profile* profile) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  return apps::AppServiceProxyFactory::GetForProfile(profile);
}

base::FilePath PrepareWebShareDirectory(Profile* profile) {
  constexpr base::FilePath::CharType kWebShareDirname[] =
      FILE_PATH_LITERAL(".WebShare");
  const base::FilePath directory =
      file_manager::util::GetMyFilesFolderForProfile(profile).Append(
          kWebShareDirname);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File::Error result = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(directory, &result));
  return directory;
}

void RemoveWebShareDirectory(const base::FilePath& directory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::DeletePathRecursively(directory));
}

base::FilePath StoreSharedFile(const base::FilePath& directory,
                               const base::StringPiece& name,
                               const base::StringPiece& content) {
  const base::FilePath path = directory.Append(name);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  EXPECT_EQ(file.WriteAtCurrentPos(content.begin(), content.size()),
            static_cast<int>(content.size()));
  return path;
}

content::EvalJsResult ReadTextContent(content::WebContents* web_contents,
                                      const char* id) {
  const std::string script =
      base::StringPrintf("document.getElementById('%s').textContent", id);
  return content::EvalJs(web_contents, script);
}

}  // namespace

namespace web_app {

class WebShareTargetBrowserTest : public WebAppControllerBrowserTest {
 public:
  GURL share_target_url() const {
    return embedded_test_server()->GetURL("/web_share_target/share.html");
  }

  content::WebContents* LaunchAppWithIntent(const AppId& app_id,
                                            apps::mojom::IntentPtr&& intent,
                                            const GURL& expected_url) {
    apps::AppLaunchParams params = apps::CreateAppLaunchParamsForIntent(
        app_id,
        /*event_flags=*/0, apps::mojom::AppLaunchSource::kSourceAppLauncher,
        display::kDefaultDisplayId,
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
        std::move(intent));

    ui_test_utils::UrlLoadObserver url_observer(
        expected_url, content::NotificationService::AllSources());
    content::WebContents* const web_contents =
        GetAppServiceProxy(profile())
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(std::move(params));
    DCHECK(web_contents);
    url_observer.Wait();
    EXPECT_EQ(expected_url, web_contents->GetVisibleURL());
    return web_contents;
  }

  std::string ExecuteShare(const std::string& script) {
    const GURL url = embedded_test_server()->GetURL("/webshare/index.html");
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* const contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(contents, script).ExtractString();
  }

  content::WebContents* ShareToTarget(const std::string& script) {
    ui_test_utils::AllBrowserTabAddedWaiter waiter;
    EXPECT_EQ("share succeeded", ExecuteShare(script));

    content::WebContents* contents = waiter.Wait();
    EXPECT_TRUE(content::WaitForLoadStop(contents));
    return contents;
  }
};

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareTextFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const base::FilePath directory = PrepareWebShareDirectory(profile());

  apps::mojom::IntentPtr intent;
  {
    const base::FilePath first_csv =
        StoreSharedFile(directory, "first.csv", "1,2,3,4,5");
    const base::FilePath second_csv =
        StoreSharedFile(directory, "second.csv", "6,7,8,9,0");

    std::vector<base::FilePath> file_paths({first_csv, second_csv});
    std::vector<std::string> content_types(2, "text/csv");
    intent = apps_util::CreateShareIntentFromFiles(
        profile(), std::move(file_paths), std::move(content_types));
  }

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("1,2,3,4,5 6,7,8,9,0", ReadTextContent(web_contents, "records"));

  RemoveWebShareDirectory(directory);
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareImageWithText) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const base::FilePath directory = PrepareWebShareDirectory(profile());

  apps::mojom::IntentPtr intent;
  {
    const base::FilePath first_svg =
        StoreSharedFile(directory, "first.svg", "picture");

    std::vector<base::FilePath> file_paths({first_svg});
    std::vector<std::string> content_types(1, "image/svg+xml");
    intent = apps_util::CreateShareIntentFromFiles(
        profile(), std::move(file_paths), std::move(content_types),
        /*share_text=*/"Euclid https://example.org/",
        /*share_title=*/"Elements");
  }

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("picture", ReadTextContent(web_contents, "graphs"));

  EXPECT_EQ("Elements", ReadTextContent(web_contents, "headline"));
  EXPECT_EQ("Euclid", ReadTextContent(web_contents, "author"));
  EXPECT_EQ("https://example.org/", ReadTextContent(web_contents, "link"));

  RemoveWebShareDirectory(directory);
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const base::FilePath directory = PrepareWebShareDirectory(profile());

  apps::mojom::IntentPtr intent;
  {
    const base::FilePath first_weba =
        StoreSharedFile(directory, "first.weba", "a");
    const base::FilePath second_weba =
        StoreSharedFile(directory, "second.weba", "b");
    const base::FilePath third_weba =
        StoreSharedFile(directory, "third.weba", "c");

    std::vector<base::FilePath> file_paths(
        {first_weba, second_weba, third_weba});
    std::vector<std::string> content_types(3, "audio/webm");
    intent = apps_util::CreateShareIntentFromFiles(
        profile(), std::move(file_paths), std::move(content_types));
    intent->share_text = "";
    intent->share_title = "";
  }

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("a b c", ReadTextContent(web_contents, "notes"));

  RemoveWebShareDirectory(directory);
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, PostBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/poster.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile(), /*file_paths=*/std::vector<base::FilePath>(),
      /*mime_types=*/std::vector<std::string>());

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());

  // Poster web app's service worker detects omitted values.
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "headline"));
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "author"));
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, PostLink) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/poster.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const apps::ShareTarget* share_target =
      WebAppProvider::Get(browser()->profile())
          ->registrar()
          .GetAppShareTarget(app_id);
  EXPECT_EQ(share_target->method, apps::ShareTarget::Method::kPost);
  EXPECT_EQ(share_target->enctype, apps::ShareTarget::Enctype::kFormUrlEncoded);

  const std::string shared_title = "Hyperlink";
  const std::string shared_link = "https://example.org/a?b=c&d=e%20#f";

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile(), /*file_paths=*/std::vector<base::FilePath>(),
      /*mime_types=*/std::vector<std::string>(),
      /*share_text=*/shared_link,
      /*share_title=*/shared_title);

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), share_target_url());
  EXPECT_EQ("POST", ReadTextContent(web_contents, "method"));
  EXPECT_EQ("application/x-www-form-urlencoded",
            ReadTextContent(web_contents, "type"));

  EXPECT_EQ(shared_title, ReadTextContent(web_contents, "headline"));
  // Poster web app's service worker detects omitted value.
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "author"));
  EXPECT_EQ(shared_link, ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, GetLink) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/gatherer.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  const apps::ShareTarget* share_target =
      WebAppProvider::Get(browser()->profile())
          ->registrar()
          .GetAppShareTarget(app_id);
  EXPECT_EQ(share_target->method, apps::ShareTarget::Method::kGet);
  EXPECT_EQ(share_target->enctype, apps::ShareTarget::Enctype::kFormUrlEncoded);

  const std::string shared_title = "My News";
  const std::string shared_link = "http://example.com/news";
  const GURL expected_url(share_target_url().spec() +
                          "?headline=My+News&link=http://example.com/news");

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile(), /*file_paths=*/std::vector<base::FilePath>(),
      /*mime_types=*/std::vector<std::string>(),
      /*share_text=*/shared_link,
      /*share_title=*/shared_title);

  content::WebContents* const web_contents =
      LaunchAppWithIntent(app_id, std::move(intent), expected_url);
  EXPECT_EQ("GET", ReadTextContent(web_contents, "method"));
  EXPECT_EQ(expected_url.spec(), ReadTextContent(web_contents, "url"));

  EXPECT_EQ(shared_title, ReadTextContent(web_contents, "headline"));
  // Gatherer web app's service worker detects omitted value.
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "author"));
  EXPECT_EQ(shared_link, ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareToPosterWebApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/poster.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  sharesheet::SharesheetService::SetSelectedAppForTesting(
      base::UTF8ToUTF16(app_id));

  // Poster web app does not accept image shares.
  EXPECT_EQ("share failed: AbortError: Share canceled",
            ExecuteShare("share_single_file()"));

  content::WebContents* web_contents = ShareToTarget("share_title()");
  EXPECT_EQ("Subject", ReadTextContent(web_contents, "headline"));

  web_contents = ShareToTarget("share_url()");
  EXPECT_EQ("https://example.com/", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareToChartsWebApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  sharesheet::SharesheetService::SetSelectedAppForTesting(
      base::UTF8ToUTF16(app_id));

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ("************", ReadTextContent(web_contents, "notes"));

  web_contents = ShareToTarget("share_url()");
  EXPECT_EQ("https://example.com/", ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(WebShareTargetBrowserTest, ShareToPartialWild) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/partial-wild.html");
  const AppId app_id = web_app::InstallWebAppFromManifest(browser(), app_url);
  sharesheet::SharesheetService::SetSelectedAppForTesting(
      base::UTF8ToUTF16(app_id));

  // Partial Wild does not accept text shares.
  EXPECT_EQ("share failed: AbortError: Share canceled",
            ExecuteShare("share_title()"));

  content::WebContents* web_contents = ShareToTarget("share_single_file()");
  EXPECT_EQ("************", ReadTextContent(web_contents, "graphs"));
}

}  // namespace web_app
