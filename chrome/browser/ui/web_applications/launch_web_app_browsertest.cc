// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

class LaunchWebAppBrowserTest : public WebAppBrowserTestBase {
 public:
  LaunchWebAppBrowserTest() = default;
  ~LaunchWebAppBrowserTest() override = default;

 protected:
  void LaunchAppFromContextMenuAndGetHeaders(
      const GURL& source_url,
      const GURL& pwa_launch_url,
      std::string& out_headers,
      network::mojom::ReferrerPolicy referrer_policy =
          network::mojom::ReferrerPolicy::kDefault) {
    const webapps::AppId app_id = InstallPWA(pwa_launch_url);
    apps::AppReadinessWaiter(profile(), app_id).Await();

    ASSERT_TRUE(content::SetCookie(profile(), pwa_launch_url,
                                   "strict_cookie=1; SameSite=Strict"));
    ASSERT_TRUE(content::SetCookie(profile(), pwa_launch_url,
                                   "lax_cookie=1; SameSite=Lax"));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), source_url));
    content::WebContents* source_tab =
        browser()->GetTabStripModel()->GetActiveWebContents();

    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    {
      ui_test_utils::UrlLoadObserver url_observer(pwa_launch_url);
      content::ContextMenuParams params;
      params.page_url = source_url;
      params.frame_url = source_url;
      params.frame_origin = url::Origin::Create(source_url);
      params.referrer_policy = referrer_policy;
      params.link_url = pwa_launch_url;
      TestRenderViewContextMenu menu(*source_tab->GetPrimaryMainFrame(),
                                     params);
      menu.Init();
      menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
                          /*event_flags=*/0);
      url_observer.Wait();
    }

    BrowserWindowInterface* const app_browser = browser_created_observer.Wait();
    ASSERT_TRUE(app_browser);
    content::WebContents* app_contents =
        app_browser->GetTabStripModel()->GetActiveWebContents();
    ASSERT_TRUE(app_contents);

    out_headers = base::ToLowerASCII(
        content::EvalJs(app_contents,
                        "document.getElementById('request-headers').innerText")
            .ExtractString());

    UninstallWebApp(app_id);
    apps::AppReadinessWaiter(profile(), app_id,
                             apps::Readiness::kUninstalledByUser)
        .Await();
  }
};

IN_PROC_BROWSER_TEST_F(LaunchWebAppBrowserTest, OpenLinkInWebApp) {
  const GURL start_url("https://app.site.test/example/index");
  const webapps::AppId app_id = InstallPWA(start_url);
  apps::AppReadinessWaiter(profile(), app_id).Await();

  size_t num_browsers =
      ProfileBrowserCollection::GetForProfile(browser()->GetProfile())
          ->GetSize();
  const int num_tabs = browser()->GetTabStripModel()->count();
  content::WebContents* initial_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  const GURL initial_url = initial_tab->GetLastCommittedURL();
  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  {
    ui_test_utils::UrlLoadObserver url_observer(start_url);
    content::ContextMenuParams params;
    params.page_url = GURL("https://www.example.com/");
    params.link_url = start_url;
    TestRenderViewContextMenu menu(*initial_tab->GetPrimaryMainFrame(), params);
    menu.Init();
    menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
                        0 /* event_flags */);
    url_observer.Wait();
  }

  BrowserWindowInterface* const app_browser = browser_created_observer.Wait();
  EXPECT_EQ(num_tabs, browser()->GetTabStripModel()->count());
  EXPECT_EQ(++num_browsers,
            ProfileBrowserCollection::GetForProfile(browser()->GetProfile())
                ->GetSize());
  EXPECT_NE(browser(), app_browser);
  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(start_url, app_browser->GetTabStripModel()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());

  UninstallWebApp(app_id);
  apps::AppReadinessWaiter(profile(), app_id,
                           apps::Readiness::kUninstalledByUser)
      .Await();
}

IN_PROC_BROWSER_TEST_F(LaunchWebAppBrowserTest,
                       OpenLinkInWebApp_CrossSiteDropsSameSiteStrict) {
  const GURL cross_site_source_url =
      embedded_https_test_server().GetURL("attacker.test", "/title1.html");
  const GURL pwa_launch_url =
      embedded_https_test_server().GetURL("app.site.test", "/echoall");

  std::string headers;
  LaunchAppFromContextMenuAndGetHeaders(cross_site_source_url, pwa_launch_url,
                                        headers);
  ASSERT_FALSE(HasFatalFailure());

  // 1. Sec-Fetch-Site must be cross-site, not none.
  EXPECT_THAT(headers, ::testing::HasSubstr("sec-fetch-site: cross-site"));

  // 2. Referer must reflect the cross-site initiating frame origin (path
  //    stripped by default strict-origin-when-cross-origin policy).
  EXPECT_THAT(
      headers,
      ::testing::HasSubstr(
          "referer: " +
          base::ToLowerASCII(
              url::Origin::Create(cross_site_source_url).GetURL().spec())));

  // 3. SameSite=Strict cookie must be withheld. SameSite=Lax must be sent.
  EXPECT_THAT(headers, ::testing::HasSubstr("lax_cookie=1"));
  EXPECT_THAT(headers, ::testing::Not(::testing::HasSubstr("strict_cookie=1")));
}

IN_PROC_BROWSER_TEST_F(LaunchWebAppBrowserTest,
                       OpenLinkInWebApp_SameSiteSendsSameSiteStrict) {
  const GURL same_site_source_url =
      embedded_https_test_server().GetURL("app.site.test", "/title1.html");
  const GURL pwa_launch_url =
      embedded_https_test_server().GetURL("app.site.test", "/echoall");

  std::string headers;
  LaunchAppFromContextMenuAndGetHeaders(same_site_source_url, pwa_launch_url,
                                        headers);
  ASSERT_FALSE(HasFatalFailure());

  // 1. Sec-Fetch-Site must be same-origin.
  EXPECT_THAT(headers, ::testing::HasSubstr("sec-fetch-site: same-origin"));

  // 2. Referer must reflect the initiating frame URL with path intact.
  EXPECT_THAT(headers, ::testing::HasSubstr(
                           "referer: " +
                           base::ToLowerASCII(same_site_source_url.spec())));

  // 3. Both SameSite=Strict and SameSite=Lax cookies must be sent for
  // same-site.
  EXPECT_THAT(headers, ::testing::HasSubstr("lax_cookie=1"));
  EXPECT_THAT(headers, ::testing::HasSubstr("strict_cookie=1"));
}

IN_PROC_BROWSER_TEST_F(LaunchWebAppBrowserTest,
                       OpenLinkInWebApp_CrossSiteNoReferrer) {
  const GURL cross_site_source_url =
      embedded_https_test_server().GetURL("attacker.test", "/title1.html");
  const GURL pwa_launch_url =
      embedded_https_test_server().GetURL("app.site.test", "/echoall");

  std::string headers;
  LaunchAppFromContextMenuAndGetHeaders(cross_site_source_url, pwa_launch_url,
                                        headers,
                                        network::mojom::ReferrerPolicy::kNever);
  ASSERT_FALSE(HasFatalFailure());

  // 1. Sec-Fetch-Site must still be cross-site.
  EXPECT_THAT(headers, ::testing::HasSubstr("sec-fetch-site: cross-site"));

  // 2. Referer must NOT be present due to kNever policy.
  EXPECT_THAT(headers, ::testing::Not(::testing::HasSubstr("referer:")));

  // 3. SameSite=Strict cookie must still be withheld.
  EXPECT_THAT(headers, ::testing::HasSubstr("lax_cookie=1"));
  EXPECT_THAT(headers, ::testing::Not(::testing::HasSubstr("strict_cookie=1")));
}

}  // namespace web_app
