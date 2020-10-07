// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"
#include "chrome/browser/web_applications/test/test_system_web_app_installation.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace web_app {

class SystemWebAppLinkCaptureBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppLinkCaptureBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock*/ false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation(
            install_from_web_app_info());
  }
  ~SystemWebAppLinkCaptureBrowserTest() override = default;

 protected:
  Browser* CreateIncognitoBrowser() {
    Browser* incognito = new Browser(Browser::CreateParams(
        browser()->profile()->GetPrimaryOTRProfile(), true));

    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(incognito, GURL(url::kAboutBlankURL),
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    observer.Wait();

    incognito->window()->Show();
    return incognito;
  }
  const GURL kInitiatingAppUrl = GURL("chrome://initiating-app/pwa.html");
  const SystemAppType kInitiatingAppType = SystemAppType::SETTINGS;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       OmniboxTypeURLAndNavigate) {
  WaitForTestSystemAppInstall();

  content::TestNavigationObserver observer(maybe_installation_->GetAppUrl());
  observer.StartWatchingNewWebContents();
  ui_test_utils::SendToOmniboxAndSubmit(
      browser(), maybe_installation_->GetAppUrl().spec());
  observer.Wait();

  Browser* app_browser = FindSystemWebAppBrowser(
      browser()->profile(), maybe_installation_->GetType());
  EXPECT_TRUE(app_browser);
  EXPECT_EQ(app_browser, chrome::FindLastActive());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, OmniboxPasteAndGo) {
  WaitForTestSystemAppInstall();
  OmniboxEditModel* model =
      browser()->window()->GetLocationBar()->GetOmniboxView()->model();

  content::TestNavigationObserver observer(maybe_installation_->GetAppUrl());
  observer.StartWatchingNewWebContents();
  model->PasteAndGo(base::UTF8ToUTF16(maybe_installation_->GetAppUrl().spec()));
  observer.Wait();

  Browser* app_browser = FindSystemWebAppBrowser(
      browser()->profile(), maybe_installation_->GetType());
  EXPECT_TRUE(app_browser);
  EXPECT_EQ(app_browser, chrome::FindLastActive());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, AnchorLinkClick) {
  WaitForTestSystemAppInstall();

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  const std::string kAnchorTargets[] = {"", "_blank", "_self"};
  const std::string kAnchorRelValues[] = {"", "noreferrer", "noopener",
                                          "noreferrer noopener"};

  for (const auto& target : kAnchorTargets) {
    for (const auto& rel : kAnchorRelValues) {
      SCOPED_TRACE(testing::Message() << "anchor link: target='" << target
                                      << "', rel='" << rel << "'");
      content::TestNavigationObserver observer(
          maybe_installation_->GetAppUrl());
      observer.StartWatchingNewWebContents();
      EXPECT_TRUE(content::ExecuteScript(
          browser()->tab_strip_model()->GetActiveWebContents(),
          content::JsReplace("{"
                             "  let el = document.createElement('a');"
                             "  el.href = $1;"
                             "  el.target = $2;"
                             "  el.rel = $3;"
                             "  el.textContent = 'target = ' + $2;"
                             "  document.body.appendChild(el);"
                             "  el.click();"
                             "}",
                             maybe_installation_->GetAppUrl(), target, rel)));
      observer.Wait();

      Browser* app_browser = FindSystemWebAppBrowser(
          browser()->profile(), maybe_installation_->GetType());
      EXPECT_TRUE(app_browser);
      EXPECT_EQ(app_browser, chrome::FindLastActive());
      EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
      EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      app_browser->window()->Close();
      base::RunLoop().RunUntilIdle();

      // Check the initiating browser window is intact.
      EXPECT_EQ(kInitiatingChromeUrl, browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetLastCommittedURL());
    }
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       AnchorLinkContextMenuNewTab) {
  WaitForTestSystemAppInstall();

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kInitiatingChromeUrl;
  context_menu_params.link_url = maybe_installation_->GetAppUrl();

  content::TestNavigationObserver observer(maybe_installation_->GetAppUrl());
  observer.StartWatchingNewWebContents();

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  observer.Wait();

  Browser* app_browser = FindSystemWebAppBrowser(
      browser()->profile(), maybe_installation_->GetType());
  EXPECT_TRUE(app_browser);
  EXPECT_EQ(app_browser, chrome::FindLastActive());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
  app_browser->window()->Close();
  base::RunLoop().RunUntilIdle();

  // Check the initiating browser window is intact.
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       AnchorLinkContextMenuNewWindow) {
  WaitForTestSystemAppInstall();

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kInitiatingChromeUrl;
  context_menu_params.link_url = maybe_installation_->GetAppUrl();

  content::TestNavigationObserver observer(maybe_installation_->GetAppUrl());
  observer.StartWatchingNewWebContents();

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 0);

  observer.Wait();

  Browser* app_browser = FindSystemWebAppBrowser(
      browser()->profile(), maybe_installation_->GetType());
  EXPECT_TRUE(app_browser);
  EXPECT_EQ(app_browser, chrome::FindLastActive());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
  app_browser->window()->Close();
  base::RunLoop().RunUntilIdle();

  // Check the initiating browser window is intact.
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, ChangeLocationHref) {
  WaitForTestSystemAppInstall();

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  content::TestNavigationObserver observer(maybe_installation_->GetAppUrl());
  observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::JsReplace("location.href=$1;",
                         maybe_installation_->GetAppUrl())));
  observer.Wait();

  Browser* app_browser = FindSystemWebAppBrowser(
      browser()->profile(), maybe_installation_->GetType());
  EXPECT_TRUE(app_browser);
  EXPECT_EQ(app_browser, chrome::FindLastActive());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Check the initiating browser window is intact.
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, WindowOpen) {
  WaitForTestSystemAppInstall();

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  const std::string kWindowOpenTargets[] = {"", "_blank"};
  const std::string kWindowOpenFeatures[] = {"", "noreferrer", "noopener",
                                             "noreferrer noopener"};

  for (const auto& target : kWindowOpenTargets) {
    for (const auto& features : kWindowOpenFeatures) {
      SCOPED_TRACE(testing::Message() << "window.open: target='" << target
                                      << "', features='" << features << "'");
      content::TestNavigationObserver observer(
          maybe_installation_->GetAppUrl());
      observer.StartWatchingNewWebContents();
      EXPECT_TRUE(content::ExecuteScript(
          browser()->tab_strip_model()->GetActiveWebContents(),
          content::JsReplace("window.open($1, $2, $3);",
                             maybe_installation_->GetAppUrl(), target,
                             features)));
      observer.Wait();

      Browser* app_browser = FindSystemWebAppBrowser(
          browser()->profile(), maybe_installation_->GetType());
      EXPECT_TRUE(app_browser);
      EXPECT_EQ(app_browser, chrome::FindLastActive());
      EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
      EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      app_browser->window()->Close();
      base::RunLoop().RunUntilIdle();

      // Check the initiating browser window is intact.
      EXPECT_EQ(kInitiatingChromeUrl, browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetLastCommittedURL());
    }
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       WindowOpenFromOtherSWA) {
  WaitForTestSystemAppInstall();

  content::WebContents* initiating_web_contents = LaunchApp(kInitiatingAppType);

  const std::string kWindowOpenTargets[] = {"", "_blank"};
  const std::string kWindowOpenFeatures[] = {"", "noreferrer", "noopener",
                                             "noreferrer noopener"};

  for (const auto& target : kWindowOpenTargets) {
    for (const auto& features : kWindowOpenFeatures) {
      SCOPED_TRACE(testing::Message() << "window.open: target='" << target
                                      << "', features='" << features << "'");
      content::TestNavigationObserver observer(
          maybe_installation_->GetAppUrl());
      observer.StartWatchingNewWebContents();
      EXPECT_TRUE(content::ExecuteScript(
          initiating_web_contents,
          content::JsReplace("window.open($1, $2, $3);",
                             maybe_installation_->GetAppUrl(), target,
                             features)));
      observer.Wait();

      Browser* app_browser = FindSystemWebAppBrowser(
          browser()->profile(), maybe_installation_->GetType());
      EXPECT_TRUE(app_browser);
      EXPECT_EQ(app_browser, chrome::FindLastActive());

      // There should be three browsers: the default one (new tab page), the
      // initiating system app, the link capturing system app.
      EXPECT_EQ(3U, chrome::GetTotalBrowserCount());
      EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      app_browser->window()->Close();
      base::RunLoop().RunUntilIdle();

      // Check the initiating browser window is intact.
      EXPECT_EQ(kInitiatingAppUrl,
                initiating_web_contents->GetLastCommittedURL());
    }
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       CaptureToOpenedWindowAndNavigateURL) {
  WaitForTestSystemAppInstall();

  Browser* app_browser;
  content::WebContents* web_contents =
      LaunchApp(maybe_installation_->GetType(), &app_browser);

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  const GURL kPageURL = maybe_installation_->GetAppUrl().Resolve("/page2.html");
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::JsReplace("let el = document.createElement('a');"
                         "el.href = $1;"
                         "el.textContent = 'Link to SWA Page 2';"
                         "document.body.appendChild(el);"
                         "el.click();",
                         kPageURL)));
  observer.Wait();

  EXPECT_EQ(kPageURL, app_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
}

// TODO(crbug.com/1109594): Update this when we decide if SWAs can be
// link-captured to standalone incognito windows. Currently, the SWA gets
// launched into a standalone browser window in incognito profile.
IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       IncognitoBrowserOmniboxLinkCapture) {
  WaitForTestSystemAppInstall();

  Browser* incognito_browser = CreateIncognitoBrowser();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();

  content::TestNavigationObserver observer(maybe_installation_->GetAppUrl());
  observer.StartWatchingNewWebContents();
  incognito_browser->window()->GetLocationBar()->FocusLocation(true);
  ui_test_utils::SendToOmniboxAndSubmit(
      incognito_browser, maybe_installation_->GetAppUrl().spec());
  observer.Wait();

  Browser* app_browser = FindSystemWebAppBrowser(
      incognito_browser->profile(), maybe_installation_->GetType());
  EXPECT_TRUE(app_browser);
  EXPECT_EQ(app_browser, chrome::FindLastActive());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_INSTALL_TYPES_P(
    SystemWebAppLinkCaptureBrowserTest);

}  // namespace web_app
