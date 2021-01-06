// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#endif

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
    Browser* incognito = Browser::Create(Browser::CreateParams(
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

// TODO(crbug.com/1135863): Decide and formalize this behavior. This test is
// disabled in DCHECK builds, because it hits a DCHECK in LaunchSystemWebApp. In
// production builds, SWA is link captured to the original profile. The goal is
// to behave reasonably, and not crashing.
#if DCHECK_IS_ON()
#define MAYBE_IncognitoBrowserOmniboxLinkCapture \
  DISABLED_IncognitoBrowserOmniboxLinkCapture
#else
#define MAYBE_IncognitoBrowserOmniboxLinkCapture \
  IncognitoBrowserOmniboxLinkCapture
#endif
IN_PROC_BROWSER_TEST_P(SystemWebAppLinkCaptureBrowserTest,
                       MAYBE_IncognitoBrowserOmniboxLinkCapture) {
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

  // We launch SWAs into the incognito profile's original profile.
  Browser* app_browser = FindSystemWebAppBrowser(
      incognito_browser->profile()->GetOriginalProfile(),
      maybe_installation_->GetType());
  EXPECT_TRUE(app_browser);
  EXPECT_EQ(app_browser, chrome::FindLastActive());
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Browser::TYPE_APP, app_browser->type());
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Use LoginManagerTest here instead of SystemWebAppManagerBrowserTest, because
// it's less complicated to add SWA to LoginManagerTest than adding multi-logins
// to SWA browsertest.
class SystemWebAppManagerMultiDesktopLaunchBrowserTest
    : public chromeos::LoginManagerTest {
 public:
  SystemWebAppManagerMultiDesktopLaunchBrowserTest()
      : chromeos::LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
    installation_ =
        TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation(
            /* use_web_app_info*/ true);
  }

  ~SystemWebAppManagerMultiDesktopLaunchBrowserTest() override = default;

  void WaitForSystemWebAppInstall(Profile* profile) {
    base::RunLoop run_loop;
    web_app::WebAppProvider::Get(profile)
        ->system_web_app_manager()
        .on_apps_synchronized()
        .Post(FROM_HERE, base::BindLambdaForTesting([&]() {
                // Wait one execution loop for on_apps_synchronized() to be
                // called on all listeners.
                base::ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE, run_loop.QuitClosure());
              }));
    run_loop.Run();
  }

  AppId GetAppId(Profile* profile) {
    SystemWebAppManager& manager =
        web_app::WebAppProvider::Get(profile)->system_web_app_manager();

    base::Optional<AppId> app_id =
        manager.GetAppIdForSystemApp(installation_->GetType());
    CHECK(app_id.has_value());
    return *app_id;
  }

  Browser* LaunchAppOnProfile(Profile* profile) {
    AppId app_id = GetAppId(profile);

    auto launch_params = apps::AppLaunchParams(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::mojom::AppLaunchSource::kSourceAppLauncher);

    content::TestNavigationObserver navigation_observer(
        installation_->GetAppUrl());

    // Watch new WebContents to wait for launches that open an app for the first
    // time.
    navigation_observer.StartWatchingNewWebContents();

    // Watch existing WebContents to wait for launches that re-use the
    // WebContents e.g. launching an already opened SWA.
    navigation_observer.WatchExistingWebContents();

    content::WebContents* web_contents =
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(std::move(launch_params));

    navigation_observer.Wait();

    return chrome::FindBrowserWithWebContents(web_contents);
  }

 protected:
  std::unique_ptr<TestSystemWebAppInstallation> installation_;
  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id1_;
  AccountId account_id2_;
};

IN_PROC_BROWSER_TEST_F(SystemWebAppManagerMultiDesktopLaunchBrowserTest,
                       LaunchToActiveDesktop) {
  // Login two users.
  LoginUser(account_id1_);
  base::RunLoop().RunUntilIdle();
  chromeos::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  base::RunLoop().RunUntilIdle();

  // Wait for System Apps to be installed on both user profiles.
  auto* user_manager = user_manager::UserManager::Get();
  Profile* profile1 = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id1_));
  Profile* profile2 = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id2_));
  WaitForSystemWebAppInstall(profile1);
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
  chromeos::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  base::RunLoop().RunUntilIdle();

  // Wait for System Apps to be installed on both user profiles.
  auto* user_manager = user_manager::UserManager::Get();
  Profile* profile1 = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id1_));
  Profile* profile2 = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id2_));
  WaitForSystemWebAppInstall(profile1);
  WaitForSystemWebAppInstall(profile2);

  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      profile2->GetPath(), base::DoNothing());

  {
    auto launch_params = apps::AppLaunchParams(
        GetAppId(profile2),
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::mojom::AppLaunchSource::kSourceAppLauncher);
    content::WebContents* web_contents =
        apps::AppServiceProxyFactory::GetForProfile(profile2)
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(std::move(launch_params));
    EXPECT_EQ(web_contents, nullptr);
  }

  {
    auto launch_params = apps::AppLaunchParams(
        GetAppId(profile1),
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::mojom::AppLaunchSource::kSourceAppLauncher);
    content::WebContents* web_contents =
        apps::AppServiceProxyFactory::GetForProfile(profile1)
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(std::move(launch_params));
    EXPECT_NE(web_contents, nullptr);
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// The following tests are disabled in DCHECK builds. LaunchSystemWebApp DCHECKs
// if the wrong profile is used. EXPECT_DCHECK_DEATH (or its variants) aren't
// reliable in browsertests, so we don't test this. This is okay because these
// tests are used to verify that in release builds, LaunchSystemWebApp doesn't
// crash and behaves reasonably (pick an appropriate profile).
#if BUILDFLAG(IS_CHROMEOS_ASH) && !DCHECK_IS_ON()
using SystemWebAppLaunchProfileBrowserTest = SystemWebAppManagerBrowserTest;

IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchProfileBrowserTest,
                       LaunchFromNormalSessionIncognitoProfile) {
  Profile* startup_profile = browser()->profile();
  ASSERT_TRUE(startup_profile->IsRegularProfile());

  WaitForTestSystemAppInstall();
  Profile* incognito_profile = startup_profile->GetPrimaryOTRProfile();

  Browser* result_browser =
      LaunchSystemWebApp(incognito_profile, GetMockAppType(),
                         GetStartUrl(LaunchParamsForApp(GetMockAppType())));
  EXPECT_EQ(startup_profile, result_browser->profile());
  EXPECT_TRUE(result_browser->profile()->IsRegularProfile());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchProfileBrowserTest,
                       LaunchFromSignInProfile) {
  WaitForTestSystemAppInstall();

  Profile* signin_profile = chromeos::ProfileHelper::GetSigninProfile();

  Browser* result_browser =
      LaunchSystemWebApp(signin_profile, GetMockAppType(),
                         GetStartUrl(LaunchParamsForApp(GetMockAppType())));
  EXPECT_EQ(nullptr, result_browser);
}

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
  // valid input to LaunchSystemWebApp.
  Profile* original_profile = browser()->profile()->GetOriginalProfile();

  Browser* result_browser =
      LaunchSystemWebApp(original_profile, GetMockAppType(),
                         GetStartUrl(LaunchParamsForApp(GetMockAppType())));

  EXPECT_EQ(startup_profile, result_browser->profile());
  EXPECT_TRUE(result_browser->profile()->IsGuestSession());
  EXPECT_TRUE(result_browser->profile()->IsPrimaryOTRProfile());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppLaunchProfileGuestSessionBrowserTest,
                       LaunchFromGuestSessionPrimaryOTRProfile) {
  // We should start into the guest session browsing profile.
  Profile* startup_profile = browser()->profile();
  ASSERT_TRUE(startup_profile->IsGuestSession());
  ASSERT_TRUE(startup_profile->IsPrimaryOTRProfile());

  WaitForTestSystemAppInstall();

  Browser* result_browser =
      LaunchSystemWebApp(startup_profile, GetMockAppType(),
                         GetStartUrl(LaunchParamsForApp(GetMockAppType())));

  EXPECT_EQ(startup_profile, result_browser->profile());
  EXPECT_TRUE(result_browser->profile()->IsGuestSession());
  EXPECT_TRUE(result_browser->profile()->IsPrimaryOTRProfile());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && !DCHECK_IS_ON()

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_INSTALL_TYPES_P(
    SystemWebAppLinkCaptureBrowserTest);

#if BUILDFLAG(IS_CHROMEOS_ASH) && !DCHECK_IS_ON()
INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_INSTALL_TYPES_P(
    SystemWebAppLaunchProfileBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    SystemWebAppLaunchProfileGuestSessionBrowserTest);
#endif

}  // namespace web_app
