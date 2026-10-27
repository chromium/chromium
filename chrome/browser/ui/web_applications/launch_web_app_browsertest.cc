// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_page_waiter.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
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

class LaunchWebAppContextMenuBrowserTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<
          blink::mojom::ManifestLaunchHandler_ClientMode> {
 protected:
  webapps::AppId InstallApp(bool tabbed = false) {
    auto info = WebAppInstallInfo::CreateForTesting(
        StartUrl(), blink::mojom::DisplayMode::kStandalone,
        mojom::UserDisplayMode::kStandalone, GetParam());
    if (tabbed) {
      info->display_override = {
          DisplayOverride::Create(blink::mojom::DisplayMode::kTabbed)};
    }
    return InstallWebApp(std::move(info));
  }

  GURL StartUrl() {
    return embedded_https_test_server().GetURL("/title1.html");
  }

  GURL LinkUrl() { return embedded_https_test_server().GetURL("/title2.html"); }

  void OpenLink(content::WebContents* source) {
    content::ContextMenuParams params;
    params.page_url = source->GetLastCommittedURL();
    params.frame_url = params.page_url;
    params.frame_origin = url::Origin::Create(params.page_url);
    params.link_url = LinkUrl();
    params.unfiltered_link_url = params.link_url;
    TestRenderViewContextMenu menu(*source->GetPrimaryMainFrame(), params);
    menu.Init();
    ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
    ASSERT_TRUE(
        menu.IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
    menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP, 0);
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }

  size_t BrowserCount() {
    return ProfileBrowserCollection::GetForProfile(profile())->GetSize();
  }

  void CheckOpenInAppRespectsLaunchHandler(bool from_other_app);

 private:
  base::test::ScopedFeatureList tab_strip_feature_{
      blink::features::kDesktopPWAsTabStrip};
};

IN_PROC_BROWSER_TEST_P(LaunchWebAppContextMenuBrowserTest,
                       SameAppOpensNewWindow) {
  const webapps::AppId app_id = InstallApp();
  BrowserWindowInterface* source_browser = LaunchWebAppBrowserAndWait(app_id);
  content::WebContents* source =
      source_browser->GetTabStripModel()->GetActiveWebContents();
  const size_t browser_count = BrowserCount();
  const int tab_count = source_browser->GetTabStripModel()->count();
  ui_test_utils::BrowserCreatedObserver created;

  using ClientMode = blink::mojom::ManifestLaunchHandler_ClientMode;
  if (GetParam() == ClientMode::kFocusExisting) {
    ASSERT_EQ(StartUrl().spec(), content::EvalJs(source, R"(
      new Promise(resolve => {
        launchQueue.setConsumer(params => resolve(params.targetURL));
      });
    )"));
    ASSERT_TRUE(content::ExecJs(source, R"(
      window.extraLaunches = [];
      launchQueue.setConsumer(params => extraLaunches.push(params.targetURL));
    )"));
  }

  OpenLink(source);
  ASSERT_FALSE(HasFatalFailure());
  // Check before waiting so a regression fails rather than waiting forever for
  // a window that was never created.
  ASSERT_EQ(browser_count + 1, BrowserCount());
  BrowserWindowInterface* target_browser = created.Wait();
  EXPECT_NE(source_browser, target_browser);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(target_browser, app_id));
  content::WebContents* target =
      target_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(test::WebAppPageWaiter(target)
                  .ExpectUrl(LinkUrl())
                  .ExpectNoManifest()
                  .WaitAndFlushCommands());
  EXPECT_EQ(LinkUrl().spec(), content::EvalJs(target, R"(
    new Promise(resolve => {
      launchQueue.setConsumer(params => resolve(params.targetURL));
    });
  )"));
  EXPECT_EQ(StartUrl(), source->GetLastCommittedURL());
  EXPECT_EQ(tab_count, source_browser->GetTabStripModel()->count());
  if (GetParam() == ClientMode::kFocusExisting) {
    EXPECT_EQ(0, content::EvalJs(source, "window.extraLaunches.length"));
  }
}

IN_PROC_BROWSER_TEST_P(LaunchWebAppContextMenuBrowserTest,
                       TabbedAppOpensNewTab) {
  const webapps::AppId app_id = InstallApp(/*tabbed=*/true);
  BrowserWindowInterface* app_browser = LaunchWebAppBrowserAndWait(app_id);
  content::WebContents* source =
      app_browser->GetTabStripModel()->GetActiveWebContents();
  const size_t browser_count = BrowserCount();
  const int tab_count = app_browser->GetTabStripModel()->count();

  OpenLink(source);
  ASSERT_FALSE(HasFatalFailure());
  EXPECT_EQ(browser_count, BrowserCount());
  ASSERT_EQ(tab_count + 1, app_browser->GetTabStripModel()->count());
  content::WebContents* target =
      app_browser->GetTabStripModel()->GetActiveWebContents();
  EXPECT_NE(source, target);
  ASSERT_TRUE(test::WebAppPageWaiter(target)
                  .ExpectUrl(LinkUrl())
                  .ExpectNoManifest()
                  .WaitAndFlushCommands());
  EXPECT_EQ(StartUrl(), source->GetLastCommittedURL());
}

void LaunchWebAppContextMenuBrowserTest::CheckOpenInAppRespectsLaunchHandler(
    bool from_other_app) {
  const webapps::AppId app_id = InstallApp();
  BrowserWindowInterface* app_browser = LaunchWebAppBrowserAndWait(app_id);
  content::WebContents* app_contents =
      app_browser->GetTabStripModel()->GetActiveWebContents();
  BrowserWindowInterface* source_browser = browser();
  if (from_other_app) {
    // Use a page without a manifest so this test does not trigger unrelated
    // manifest updates or OS integration while checking context-menu launches.
    const GURL other_app_url =
        embedded_https_test_server().GetURL("/title3.html");
    auto info = WebAppInstallInfo::CreateForTesting(other_app_url);
    info->scope = other_app_url;
    const webapps::AppId other_app_id = InstallWebApp(std::move(info));
    ASSERT_NE(app_id, other_app_id);
    source_browser = LaunchWebAppBrowserAndWait(other_app_id);
    ASSERT_TRUE(
        AppBrowserController::IsForWebApp(source_browser, other_app_id));
  }
  content::WebContents* source =
      source_browser->GetTabStripModel()->GetActiveWebContents();
  const GURL source_url = source->GetLastCommittedURL();
  const size_t browser_count = BrowserCount();
  ui_test_utils::BrowserCreatedObserver created;
  using ClientMode = blink::mojom::ManifestLaunchHandler_ClientMode;
  if (GetParam() == ClientMode::kFocusExisting) {
    // Consume the initial launch before observing the context-menu launch.
    ASSERT_EQ(StartUrl().spec(), content::EvalJs(app_contents, R"(
                new Promise(resolve => {
                  launchQueue.setConsumer(params => resolve(params.targetURL));
                });
              )"));
    ASSERT_TRUE(content::ExecJs(app_contents, R"(
      window.nextLaunch = new Promise(resolve => {
        launchQueue.setConsumer(params => resolve(params.targetURL));
      });
      true;
    )"));
  }

  OpenLink(source);
  ASSERT_FALSE(HasFatalFailure());
  if (GetParam() == ClientMode::kAuto ||
      GetParam() == ClientMode::kNavigateNew) {
    ASSERT_EQ(browser_count + 1, BrowserCount());
    auto* target_browser = created.Wait();
    EXPECT_TRUE(AppBrowserController::IsForWebApp(target_browser, app_id));
    auto* target = target_browser->GetTabStripModel()->GetActiveWebContents();
    ASSERT_TRUE(test::WebAppPageWaiter(target)
                    .ExpectUrl(LinkUrl())
                    .ExpectNoManifest()
                    .WaitAndFlushCommands());
    EXPECT_EQ(StartUrl(), app_contents->GetLastCommittedURL());
  } else {
    EXPECT_EQ(browser_count, BrowserCount());
    const GURL expected_url =
        GetParam() == ClientMode::kFocusExisting ? StartUrl() : LinkUrl();
    ASSERT_TRUE(test::WebAppPageWaiter(app_contents)
                    .ExpectUrl(expected_url)
                    .ExpectNoManifest()
                    .WaitAndFlushCommands());
    if (GetParam() == ClientMode::kFocusExisting) {
      EXPECT_EQ(LinkUrl().spec(),
                content::EvalJs(app_contents, "window.nextLaunch"));
    }
  }
  EXPECT_EQ(source_url, source->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(LaunchWebAppContextMenuBrowserTest,
                       OrdinaryOpenInAppRespectsLaunchHandler) {
  CheckOpenInAppRespectsLaunchHandler(/*from_other_app=*/false);
}

IN_PROC_BROWSER_TEST_P(LaunchWebAppContextMenuBrowserTest,
                       OtherAppOpenInAppRespectsLaunchHandler) {
  CheckOpenInAppRespectsLaunchHandler(/*from_other_app=*/true);
}

INSTANTIATE_TEST_SUITE_P(
    AllClientModes,
    LaunchWebAppContextMenuBrowserTest,
    testing::Values(
        blink::mojom::ManifestLaunchHandler_ClientMode::kAuto,
        blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew,
        blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting,
        blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting),
    [](const testing::TestParamInfo<
        blink::mojom::ManifestLaunchHandler_ClientMode>& info) {
      using ClientMode = blink::mojom::ManifestLaunchHandler_ClientMode;
      switch (info.param) {
        case ClientMode::kAuto:
          return "Auto";
        case ClientMode::kNavigateNew:
          return "NavigateNew";
        case ClientMode::kNavigateExisting:
          return "NavigateExisting";
        case ClientMode::kFocusExisting:
          return "FocusExisting";
      }
    });

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
