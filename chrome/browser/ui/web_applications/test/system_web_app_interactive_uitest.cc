// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_item.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/screen.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace web_app {

class SystemWebAppLinkCaptureBrowserTest
    : public TestProfileTypeMixin<ash::SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppLinkCaptureBrowserTest() {
    SetSystemWebAppInstallation(
        ash::TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation());
  }
  ~SystemWebAppLinkCaptureBrowserTest() override = default;

  content::WebContents* CreateInitiatingWebContents() {
    GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
    NavigateViaLinkClickToURLAndWait(browser(), kInitiatingChromeUrl);
    EXPECT_EQ(kInitiatingChromeUrl, browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetLastCommittedURL());
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  Browser* CreateIncognitoBrowser() {
    Browser* incognito = Browser::Create(Browser::CreateParams(
        browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        true));

    auto* contents =
        chrome::AddSelectedTabWithURL(incognito, GURL(url::kAboutBlankURL),
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    EXPECT_TRUE(content::WaitForLoadStop(contents));

    incognito->window()->Show();
    return incognito;
  }
  const GURL kInitiatingAppUrl = GURL("chrome://initiating-app/pwa.html");
  const ash::SystemWebAppType kInitiatingAppType =
      ash::SystemWebAppType::SETTINGS;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       OmniboxTypeURLAndNavigate) {
  WaitForTestSystemAppInstall();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  ui_test_utils::SendToOmniboxAndSubmit(browser(), GetStartUrl().spec());
  observer.Wait();

  Browser* app_browser =
      FindSystemWebAppBrowser(browser()->profile(), GetAppType());
  EXPECT_TRUE(app_browser);
  ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, OmniboxPasteAndGo) {
  WaitForTestSystemAppInstall();
  OmniboxEditModel* model =
      browser()->window()->GetLocationBar()->GetOmniboxView()->model();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  model->PasteAndGo(base::UTF8ToUTF16(GetStartUrl().spec()));
  observer.Wait();

  Browser* app_browser =
      FindSystemWebAppBrowser(browser()->profile(), GetAppType());
  EXPECT_TRUE(app_browser);
  ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, AnchorLinkClick) {
  WaitForTestSystemAppInstall();

  content::WebContents* initiating_web_contents = CreateInitiatingWebContents();
  const GURL& initiating_url = initiating_web_contents->GetLastCommittedURL();
  size_t starting_browser_count = chrome::GetTotalBrowserCount();

  const std::string kAnchorTargets[] = {"", "_blank", "_self"};
  const std::string kAnchorRelValues[] = {"", "noreferrer", "noopener",
                                          "noreferrer noopener"};

  for (const auto& target : kAnchorTargets) {
    for (const auto& rel : kAnchorRelValues) {
      SCOPED_TRACE(testing::Message() << "anchor link: target='" << target
                                      << "', rel='" << rel << "'");
      content::TestNavigationObserver observer(GetStartUrl());
      observer.StartWatchingNewWebContents();
      EXPECT_TRUE(content::ExecJs(
          initiating_web_contents,
          content::JsReplace("{"
                             "  let el = document.createElement('a');"
                             "  el.href = $1;"
                             "  el.target = $2;"
                             "  el.rel = $3;"
                             "  el.textContent = 'target = ' + $2;"
                             "  document.body.appendChild(el);"
                             "  el.click();"
                             "}",
                             GetStartUrl(), target, rel)));
      observer.Wait();

      Browser* app_browser =
          FindSystemWebAppBrowser(browser()->profile(), GetAppType());
      EXPECT_TRUE(app_browser);
      ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
      EXPECT_EQ(1 + starting_browser_count, chrome::GetTotalBrowserCount());
      EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      app_browser->window()->Close();
      ui_test_utils::WaitForBrowserToClose(app_browser);

      // Check the initiating page is intact.
      EXPECT_EQ(initiating_url, initiating_web_contents->GetLastCommittedURL());
    }
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       AnchorLinkContextMenuNewTab) {
  WaitForTestSystemAppInstall();

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateViaLinkClickToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kInitiatingChromeUrl;
  context_menu_params.link_url = GetStartUrl();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();

  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  observer.Wait();

  Browser* app_browser =
      FindSystemWebAppBrowser(browser()->profile(), GetAppType());
  EXPECT_TRUE(app_browser);
  ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);

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
  NavigateViaLinkClickToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kInitiatingChromeUrl;
  context_menu_params.link_url = GetStartUrl();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();

  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 0);

  observer.Wait();

  Browser* app_browser =
      FindSystemWebAppBrowser(browser()->profile(), GetAppType());
  EXPECT_TRUE(app_browser);
  ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);

  // Check the initiating browser window is intact.
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, ChangeLocationHref) {
  WaitForTestSystemAppInstall();

  content::WebContents* initiating_web_contents = CreateInitiatingWebContents();
  const GURL& initiating_url = initiating_web_contents->GetLastCommittedURL();
  size_t starting_browser_count = chrome::GetTotalBrowserCount();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      content::ExecJs(initiating_web_contents,
                      content::JsReplace("location.href=$1;", GetStartUrl())));
  observer.Wait();

  Browser* app_browser =
      FindSystemWebAppBrowser(browser()->profile(), GetAppType());
  EXPECT_TRUE(app_browser);
  ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
  EXPECT_EQ(1 + starting_browser_count, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Check the initiating browser window is intact.
  EXPECT_EQ(initiating_url, initiating_web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest, WindowOpen) {
  WaitForTestSystemAppInstall();

  content::WebContents* initiating_web_contents = CreateInitiatingWebContents();
  const GURL& initiating_url = initiating_web_contents->GetLastCommittedURL();
  size_t starting_browser_count = chrome::GetTotalBrowserCount();

  const std::string kWindowOpenTargets[] = {"", "_blank"};
  const std::string kWindowOpenFeatures[] = {"", "noreferrer", "noopener",
                                             "noreferrer noopener"};

  for (const auto& target : kWindowOpenTargets) {
    for (const auto& features : kWindowOpenFeatures) {
      SCOPED_TRACE(testing::Message() << "window.open: target='" << target
                                      << "', features='" << features << "'");
      content::TestNavigationObserver observer(GetStartUrl());
      observer.StartWatchingNewWebContents();
      EXPECT_TRUE(
          content::ExecJs(initiating_web_contents,
                          content::JsReplace("window.open($1, $2, $3);",
                                             GetStartUrl(), target, features)));
      observer.Wait();

      Browser* app_browser =
          FindSystemWebAppBrowser(browser()->profile(), GetAppType());
      EXPECT_TRUE(app_browser);
      ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
      EXPECT_EQ(1 + starting_browser_count, chrome::GetTotalBrowserCount());
      EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      app_browser->window()->Close();
      ui_test_utils::WaitForBrowserToClose(app_browser);

      // Check the initiating browser window is intact.
      EXPECT_EQ(initiating_url, initiating_web_contents->GetLastCommittedURL());
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
      content::TestNavigationObserver observer(GetStartUrl());
      observer.StartWatchingNewWebContents();
      EXPECT_TRUE(
          content::ExecJs(initiating_web_contents,
                          content::JsReplace("window.open($1, $2, $3);",
                                             GetStartUrl(), target, features)));
      observer.Wait();

      Browser* app_browser =
          FindSystemWebAppBrowser(browser()->profile(), GetAppType());
      EXPECT_TRUE(app_browser);
      ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();

      // There should be three browsers: the default one (new tab page), the
      // initiating system app, the link capturing system app.
      EXPECT_EQ(3U, chrome::GetTotalBrowserCount());
      EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      app_browser->window()->Close();
      ui_test_utils::WaitForBrowserToClose(app_browser);

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
  content::WebContents* web_contents = LaunchApp(GetAppType(), &app_browser);

  GURL kInitiatingChromeUrl = GURL(chrome::kChromeUIAboutURL);
  NavigateViaLinkClickToURLAndWait(browser(), kInitiatingChromeUrl);
  EXPECT_EQ(kInitiatingChromeUrl, browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetLastCommittedURL());

  const GURL kPageURL = GetStartUrl().Resolve("/page2.html");
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(content::ExecJs(
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

IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       IncognitoBrowserOmniboxLinkCapture) {
  WaitForTestSystemAppInstall();
  GURL start_url = GetStartUrl();

  Browser* incognito_browser = CreateIncognitoBrowser();
  browser()->window()->Close();
  ui_test_utils::WaitForBrowserToClose(browser());

  content::TestNavigationObserver observer(start_url);
  observer.StartWatchingNewWebContents();
  incognito_browser->window()->GetLocationBar()->FocusLocation(true);
  ui_test_utils::SendToOmniboxAndSubmit(incognito_browser, start_url.spec());
  observer.Wait();

  // We launch SWAs into the incognito profile's original profile.
  Browser* app_browser = FindSystemWebAppBrowser(
      incognito_browser->profile()->GetOriginalProfile(), GetAppType());
  EXPECT_TRUE(app_browser);
  ui_test_utils::BrowserActivationWaiter(app_browser).WaitForActivation();
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

class SystemWebAppManagerWindowSizeControlsTest
    : public TestProfileTypeMixin<ash::SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerWindowSizeControlsTest() {
    SetSystemWebAppInstallation(ash::TestSystemWebAppInstallation::
                                    SetUpNonResizeableAndNonMaximizableApp());
  }
  ~SystemWebAppManagerWindowSizeControlsTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerWindowSizeControlsTest,
                       NonResizeableWindow) {
  WaitForTestSystemAppInstall();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  Browser* app_browser;
  LaunchApp(GetAppType(), &app_browser);

  EXPECT_FALSE(app_browser->create_params().can_resize);
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerWindowSizeControlsTest,
                       NonMaximizableWindow) {
  WaitForTestSystemAppInstall();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  Browser* app_browser;
  LaunchApp(GetAppType(), &app_browser);

  EXPECT_FALSE(app_browser->create_params().can_maximize);
}

// Use LoginManagerTest here instead of SystemWebAppManagerBrowserTest, because
// it's less complicated to add SWA to LoginManagerTest than adding multi-logins
// to SWA browsertest.
class SystemWebAppManagerMultiDesktopLaunchBrowserTest
    : public ash::LoginManagerTest {
 public:
  SystemWebAppManagerMultiDesktopLaunchBrowserTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
    installation_ =
        ash::TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation();
  }

  ~SystemWebAppManagerMultiDesktopLaunchBrowserTest() override = default;

  void WaitForSystemWebAppInstall(Profile* profile) {
    base::RunLoop run_loop;

    ash::SystemWebAppManager::Get(profile)->on_apps_synchronized().Post(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Wait one execution loop for
          // on_apps_synchronized() to be called on all
          // listeners.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, run_loop.QuitClosure());
        }));
    run_loop.Run();
  }

  webapps::AppId GetAppId(Profile* profile) {
    std::optional<webapps::AppId> app_id =
        ash::SystemWebAppManager::Get(profile)->GetAppIdForSystemApp(
            installation_->GetType());
    CHECK(app_id.has_value());
    return *app_id;
  }

  Browser* LaunchAppOnProfile(Profile* profile) {
    webapps::AppId app_id = GetAppId(profile);

    auto launch_params = apps::AppLaunchParams(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::LaunchSource::kFromAppListGrid);

    content::TestNavigationObserver navigation_observer(
        installation_->GetAppUrl());

    // Watch new WebContents to wait for launches that open an app for the first
    // time.
    navigation_observer.StartWatchingNewWebContents();

    // Watch existing WebContents to wait for launches that re-use the
    // WebContents e.g. launching an already opened SWA.
    navigation_observer.WatchExistingWebContents();

    LaunchSystemWebAppAsync(profile, installation_->GetType());

    navigation_observer.Wait();

    Browser* swa_browser =
        FindSystemWebAppBrowser(profile, installation_->GetType());
    EXPECT_TRUE(swa_browser);
    ui_test_utils::BrowserActivationWaiter(swa_browser).WaitForActivation();

    return swa_browser;
  }

  void AwaitWebAppCommandsCompleteForTesting(Profile* profile) {
    ash::SystemWebAppManager::Get(profile)
        ->GetWebAppProvider(profile)
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();
  }

 protected:
  std::unique_ptr<ash::TestSystemWebAppInstallation> installation_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id1_;
  AccountId account_id2_;
};

IN_PROC_BROWSER_TEST_F(SystemWebAppManagerMultiDesktopLaunchBrowserTest,
                       LaunchToActiveDesktop) {
  // Login two users.
  LoginUser(account_id1_);
  base::RunLoop().RunUntilIdle();

  // Wait for System Apps to be installed on both user profiles.
  auto* user_manager = user_manager::UserManager::Get();
  Profile* profile1 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id1_));
  WaitForSystemWebAppInstall(profile1);

  installation_ =
      ash::TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation();
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  base::RunLoop().RunUntilIdle();
  Profile* profile2 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id2_));
  WaitForSystemWebAppInstall(profile2);
  // Set user 1 to be active.
  user_manager->SwitchActiveUser(account_id1_);
  EXPECT_TRUE(multi_user_util::IsProfileFromActiveUser(profile1));
  EXPECT_FALSE(multi_user_util::IsProfileFromActiveUser(profile2));

  // Launch the app from user 2 profile. The window should be on user 1
  // (the active) desktop.
  Browser* browser2 = LaunchAppOnProfile(profile2);
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          browser2->window()->GetNativeWindow(), account_id1_));

  // Launch the app from user 1 profile. The window should be on user 1 (the
  // active) desktop. And there should be two different browser windows
  // (for each profile).
  Browser* browser1 = LaunchAppOnProfile(profile1);
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          browser1->window()->GetNativeWindow(), account_id1_));

  EXPECT_NE(browser1, browser2);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());

  // Switch to user 2, then launch the app. SWAs reuse their window, so it
  // should bring `browser2` to user 2 (the active) desktop.
  user_manager->SwitchActiveUser(account_id2_);
  Browser* browser2_relaunch = LaunchAppOnProfile(profile2);

  EXPECT_EQ(browser2, browser2_relaunch);
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          browser2->window()->GetNativeWindow(), account_id2_));
}

IN_PROC_BROWSER_TEST_F(SystemWebAppManagerMultiDesktopLaunchBrowserTest,
                       ProfileScheduledForDeletion) {
  // Login two users.
  LoginUser(account_id1_);
  base::RunLoop().RunUntilIdle();

  // Wait for System Apps to be installed on both user profiles.
  auto* user_manager = user_manager::UserManager::Get();
  Profile* profile1 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id1_));
  WaitForSystemWebAppInstall(profile1);

  installation_ =
      ash::TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation();
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  base::RunLoop().RunUntilIdle();
  AwaitWebAppCommandsCompleteForTesting(profile1);
  Profile* profile2 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id2_));
  WaitForSystemWebAppInstall(profile2);
  AwaitWebAppCommandsCompleteForTesting(profile2);
  const webapps::AppId& app_id1 = GetAppId(profile1);
  const webapps::AppId& app_id2 = GetAppId(profile2);

  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          profile2->GetPath(), base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

  {
    auto launch_params = apps::AppLaunchParams(
        app_id2, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::LaunchSource::kFromAppListGrid);
    content::WebContents* web_contents =
        apps::AppServiceProxyFactory::GetForProfile(profile2)
            ->BrowserAppLauncher()
            ->LaunchAppWithParamsForTesting(std::move(launch_params));
    EXPECT_EQ(web_contents, nullptr);
  }

  {
    auto launch_params = apps::AppLaunchParams(
        app_id1, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::LaunchSource::kFromAppListGrid);
    content::WebContents* web_contents =
        apps::AppServiceProxyFactory::GetForProfile(profile1)
            ->BrowserAppLauncher()
            ->LaunchAppWithParamsForTesting(std::move(launch_params));
    EXPECT_NE(web_contents, nullptr);
  }
}

using SystemWebAppLaunchProfileBrowserTest =
    ash::SystemWebAppManagerBrowserTest;

IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchProfileBrowserTest,
                       LaunchFromNormalSessionIncognitoProfile) {
  Profile* startup_profile = browser()->profile();
  ASSERT_TRUE(!startup_profile->IsOffTheRecord());

  WaitForTestSystemAppInstall();
  Profile* incognito_profile =
      startup_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  LaunchSystemWebAppAsync(incognito_profile, GetAppType());
  observer.Wait();

  EXPECT_FALSE(FindSystemWebAppBrowser(incognito_profile, GetAppType()));
  EXPECT_TRUE(FindSystemWebAppBrowser(startup_profile, GetAppType()));
}

#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()
// The following tests are disabled outside official non-DCHECK builds.
// LaunchSystemWebAppAsync hits a DUMP_WILL_BE_NOTREACHED() if it can't find a
// suitable profile. EXPECT_NOTREACHED_DEATH isn't reliable in browser_tests, so
// we don't test this. Here we verify LaunchSystemWebAppAsync doesn't crash
// in official builds.
IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchProfileBrowserTest,
                       LaunchFromSignInProfile) {
  WaitForTestSystemAppInstall();

  Profile* signin_profile = ash::ProfileHelper::GetSigninProfile();

  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  LaunchSystemWebAppAsync(signin_profile, GetAppType());

  // Use RunUntilIdle() here, because this catches the scenario where
  // LaunchSystemWebAppAsync mistakenly picks a profile to launch the app.
  //
  // RunUntilIdle() serves a catch-all solution, so we don't have to flush mojo
  // calls on all existing profiles (and those potentially created during
  // launch).
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}
#endif  // !DCHECK_IS_ON()

using SystemWebAppLaunchProfileGuestSessionBrowserTest =
    SystemWebAppLaunchProfileBrowserTest;

IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchProfileGuestSessionBrowserTest,
                       LaunchFromGuestSessionOriginalProfile) {
  // We should start into the guest session browsing profile.
  Profile* startup_profile = browser()->profile();
  ASSERT_TRUE(startup_profile->IsGuestSession());
  ASSERT_TRUE(startup_profile->IsPrimaryOTRProfile());

  WaitForTestSystemAppInstall();

  // We typically don't get the original profile as an argument, but it is a
  // valid input to LaunchSystemWebAppAsync.
  Profile* original_profile = browser()->profile()->GetOriginalProfile();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  LaunchSystemWebAppAsync(original_profile, GetAppType());
  observer.Wait();

  EXPECT_FALSE(FindSystemWebAppBrowser(original_profile, GetAppType()));
  EXPECT_TRUE(FindSystemWebAppBrowser(startup_profile, GetAppType()));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchProfileGuestSessionBrowserTest,
                       LaunchFromGuestSessionPrimaryOTRProfile) {
  // We should start into the guest session browsing profile.
  Profile* startup_profile = browser()->profile();
  ASSERT_TRUE(startup_profile->IsGuestSession());
  ASSERT_TRUE(startup_profile->IsPrimaryOTRProfile());

  WaitForTestSystemAppInstall();

  content::TestNavigationObserver observer(GetStartUrl());
  observer.StartWatchingNewWebContents();
  LaunchSystemWebAppAsync(startup_profile, GetAppType());
  observer.Wait();

  EXPECT_TRUE(FindSystemWebAppBrowser(startup_profile, GetAppType()));
}

using SystemWebAppLaunchOmniboxNavigateBrowsertest =
    ash::SystemWebAppManagerBrowserTest;

IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchOmniboxNavigateBrowsertest,
                       OpenInTab) {
  WaitForTestSystemAppInstall();

  content::TestNavigationObserver observer(GetStartUrl());
  // The app should load in the blank WebContents created when browser starts.
  observer.WatchExistingWebContents();
  ui_test_utils::SendToOmniboxAndSubmit(browser(), GetStartUrl().spec());
  observer.Wait();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), GetStartUrl());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Verifies the tab has an associated tab helper for System App's
  // webapps::AppId.
  EXPECT_EQ(*web_app::WebAppTabHelper::GetAppId(web_contents),
            *ash::GetAppIdForSystemWebApp(browser()->profile(), GetAppType()));
}

// A one shot observer which waits for an activation of any window.
class TestActivationObserver : public wm::ActivationChangeObserver {
 public:
  TestActivationObserver(const TestActivationObserver&) = delete;
  TestActivationObserver& operator=(const TestActivationObserver&) = delete;

  TestActivationObserver() {
    activation_observer_.Observe(ash::Shell::Get()->activation_client());
  }

  ~TestActivationObserver() override = default;

  void Wait() { run_loop_.Run(); }

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    Browser* browser = chrome::FindBrowserWithWindow(gained_active);
    // Check that the activated window is actually a browser.
    EXPECT_TRUE(browser);
    // Check also that the browser is actually an app.
    EXPECT_TRUE(browser->is_type_app());
    run_loop_.Quit();
  }

 private:
  // The MessageLoopRunner used to spin the message loop.
  base::RunLoop run_loop_;
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_observer_{this};
};

class SystemWebAppManagerCloseFromScriptsTest
    : public TestProfileTypeMixin<ash::SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerCloseFromScriptsTest() {
    SetSystemWebAppInstallation(
        ash::TestSystemWebAppInstallation::
            SetupAppWithAllowScriptsToCloseWindows(true));
  }
  ~SystemWebAppManagerCloseFromScriptsTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerCloseFromScriptsTest, WindowClose) {
  WaitForTestSystemAppInstall();

  Browser* app_browser;
  LaunchApp(GetAppType(), &app_browser);

  const GURL kPageURL = GetStartUrl().Resolve("/page2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, kPageURL));
  EXPECT_EQ(kPageURL, app_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());

  EXPECT_TRUE(
      content::ExecJs(app_browser->tab_strip_model()->GetActiveWebContents(),
                      "window.close();"));

  ui_test_utils::WaitForBrowserToClose(app_browser);
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

class SystemWebAppManagerShouldNotCloseFromScriptsTest
    : public TestProfileTypeMixin<ash::SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerShouldNotCloseFromScriptsTest() {
    SetSystemWebAppInstallation(
        ash::TestSystemWebAppInstallation::
            SetupAppWithAllowScriptsToCloseWindows(false));
  }
  ~SystemWebAppManagerShouldNotCloseFromScriptsTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerShouldNotCloseFromScriptsTest,
                       ShouldNotCloseWindow) {
  WaitForTestSystemAppInstall();

  Browser* app_browser;
  LaunchApp(GetAppType(), &app_browser);

  const GURL kPageURL = GetStartUrl().Resolve("/page2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, kPageURL));
  EXPECT_EQ(kPageURL, app_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());

  content::WebContentsConsoleObserver console_observer(
      app_browser->tab_strip_model()->GetActiveWebContents());
  console_observer.SetPattern(
      "Scripts may close only the windows that were opened by them.");

  EXPECT_TRUE(
      content::ExecJs(app_browser->tab_strip_model()->GetActiveWebContents(),
                      "window.close();"));

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
}

class SystemWebAppNewWindowMenuItemTest
    : public TestProfileTypeMixin<ash::SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppNewWindowMenuItemTest() {
    SetSystemWebAppInstallation(
        ash::TestSystemWebAppInstallation::SetUpAppWithNewWindowMenuItem());
  }
  ~SystemWebAppNewWindowMenuItemTest() override = default;

  ash::ShelfItemDelegate* GetAppShelfItemDelegate() {
    auto app_id = GetManager().GetAppIdForSystemApp(GetAppType()).value();
    return ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(app_id));
  }

  std::unique_ptr<AppServiceAppItem> GetAppServiceAppItem() {
    Profile* profile = browser()->profile();
    std::unique_ptr<AppServiceAppItem> item;
    auto app_id = GetManager().GetAppIdForSystemApp(GetAppType()).value();
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->AppRegistryCache()
        .ForOneApp(
            app_id, [profile, &item](const apps::AppUpdate& update) {
              item = std::make_unique<AppServiceAppItem>(
                  profile, /*model_updater=*/nullptr, /*sync_item=*/nullptr,
                  update);

              // Because model updater is null, set position manually.
              item->SetChromePosition(item->CalculateDefaultPositionForTest());
            });
    return item;
  }

  std::unique_ptr<ui::MenuModel> GetShelfContextMenu(
      ash::ShelfItemDelegate* item_delegate,
      int64_t display_id) {
    base::RunLoop run_loop;
    std::unique_ptr<ui::MenuModel> menu;
    item_delegate->GetContextMenu(
        display_id, base::BindLambdaForTesting(
                        [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
                          menu = std::move(created_menu);
                          run_loop.Quit();
                        }));
    run_loop.Run();
    return menu;
  }

  std::unique_ptr<ui::SimpleMenuModel> GetAppListContextMenu(
      ChromeAppListItem* item) {
    base::RunLoop run_loop;
    std::unique_ptr<ui::SimpleMenuModel> menu;
    item->GetContextMenuModel(
        ash::AppListItemContext::kNone,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
              menu = std::move(created_menu);
              run_loop.Quit();
            }));
    run_loop.Run();
    return menu;
  }

  void ExpectMenuCommandLaunchesSystemWebApp(
      std::unique_ptr<ui::MenuModel> menu,
      int command_id,
      const GURL& app_url) {
    ASSERT_TRUE(menu);
    ui::MenuModel* model = menu.get();

    size_t command_index;
    ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model,
                                                &command_index);
    EXPECT_TRUE(menu->IsEnabledAt(command_index));

    content::TestNavigationObserver observer(app_url);
    observer.StartWatchingNewWebContents();
    menu->ActivatedAt(command_index);
    observer.Wait();
  }

  int64_t GetDisplayId() {
    return display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppNewWindowMenuItemTest,
                       ShelfContextMenuOpensNewWindow) {
  WaitForTestSystemAppInstall();

  // Launch the app so it shows up in shelf.
  LaunchApp(GetAppType());

  auto* shelf_item_delegate = GetAppShelfItemDelegate();
  ASSERT_TRUE(shelf_item_delegate);

  ExpectMenuCommandLaunchesSystemWebApp(
      GetShelfContextMenu(shelf_item_delegate, GetDisplayId()), ash::LAUNCH_NEW,
      GetStartUrl());

  EXPECT_EQ(2U, GetSystemWebAppBrowserCount(GetAppType()));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppNewWindowMenuItemTest,
                       AppListContextMenuLaunchNew) {
  WaitForTestSystemAppInstall();

  LaunchApp(GetAppType());

  auto item = GetAppServiceAppItem();
  ASSERT_TRUE(item);

  ExpectMenuCommandLaunchesSystemWebApp(GetAppListContextMenu(item.get()),
                                        ash::LAUNCH_NEW, GetStartUrl());

  EXPECT_EQ(2U, GetSystemWebAppBrowserCount(GetAppType()));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppLinkCaptureBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppLaunchProfileBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    SystemWebAppLaunchProfileGuestSessionBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerWindowSizeControlsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    SystemWebAppLaunchOmniboxNavigateBrowsertest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerCloseFromScriptsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerShouldNotCloseFromScriptsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppNewWindowMenuItemTest);

}  // namespace web_app
