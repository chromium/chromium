// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace web_app {

class CreateShortcutBrowserTest : public WebAppControllerBrowserTest {
 public:
  AppId InstallShortcutAppForCurrentUrl() {
    chrome::SetAutoAcceptWebAppDialogForTesting(true, false);
    WebAppInstallObserver observer(profile());
    CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
    AppId app_id = observer.AwaitNextInstall();
    chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
    return app_id;
  }

  AppRegistrar& registrar() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    CHECK(provider);
    return provider->registrar();
  }

  AppRegistryController& registry_controller() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    CHECK(provider);
    return provider->registry_controller();
  }
};

IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       CreateShortcutForInstallableSite) {
  base::UserActionTester user_action_tester;
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppShortName(app_id), GetInstallableAppName());
  // Shortcut apps to PWAs should launch in a tab.
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id), DisplayMode::kBrowser);

  EXPECT_EQ(0, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("CreateShortcut"));
}

IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest, InstallSourceRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // LatestWebAppInstallSource should be correctly set and reported to UMA for
  // both installable and non-installable sites.
  for (const GURL& url : {GetInstallableAppURL(),
                          embedded_test_server()->GetURL(
                              "/web_apps/theme_color_only_manifest.html")}) {
    base::HistogramTester histogram_tester;
    NavigateToURLAndWait(browser(), url);
    AppId app_id = InstallShortcutAppForCurrentUrl();

    base::Optional<int> install_source = GetIntWebAppPref(
        profile()->GetPrefs(), app_id, kLatestWebAppInstallSource);
    EXPECT_TRUE(install_source.has_value());
    EXPECT_EQ(static_cast<WebappInstallSource>(*install_source),
              WebappInstallSource::MENU_CREATE_SHORTCUT);
    histogram_tester.ExpectUniqueSample(
        "Webapp.Install.InstallEvent",
        static_cast<int>(WebappInstallSource::MENU_CREATE_SHORTCUT), 1);
  }
}

IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       CanInstallOverTabShortcutApp) {
  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  InstallShortcutAppForCurrentUrl();

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       CannotInstallOverWindowShortcutApp) {
  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  AppId app_id = InstallShortcutAppForCurrentUrl();
  // Change launch container to open in window.
  registry_controller().SetAppUserDisplayMode(app_id, DisplayMode::kStandalone,
                                              /*is_user_action=*/false);

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

// Check that toolbar is not shown for shortcut apps within extensions pages.
// This simulates a case where the user has manually navigated to a page hosted
// within an extension, then added it as a shortcut app.
// Regression test for https://crbug.com/828233.
IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       ShouldShowCustomTabBarForExtensionPage) {
  // This involves the creation of a regular (non-app) extension with a popup
  // page, and the creation of a shortcut app created from the popup page URL
  // (allowing the extension's popup page to be loaded in a window).

  // Install the extension that has the popup page.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Install the shortcut app that links to the extension's popup page.
  const GURL popup_url("chrome-extension://" + last_loaded_extension_id() +
                       "/popup.html");

  NavigateToURLAndWait(browser(), popup_url);
  const AppId app_id = InstallShortcutAppForCurrentUrl();
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  CHECK(app_browser);
  CHECK(app_browser != browser());

  // Navigate to the app's launch page; the toolbar should not be visible,
  // because extensions pages are secure.
  NavigateAndCheckForToolbar(app_browser, popup_url, false);
}

// Tests that Create Shortcut doesn't timeout on a page that has a delayed
// iframe load. Context: crbug.com/1046883
IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest, WorksAfterDelayedIFrameLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURLAndWait(browser(), embedded_test_server()->GetURL(
                                      "/favicon/page_with_favicon.html"));

  // Append an iframe and wait for it to finish loading.
  const char script[] = R"(
    const iframe = document.createElement('iframe');
    iframe.onload = _ => domAutomationController.send('success');
    iframe.srcdoc = 'inner page';
    document.body.appendChild(iframe);
  )";
  EXPECT_EQ(content::EvalJsWithManualReply(
                browser()->tab_strip_model()->GetActiveWebContents(), script)
                .ExtractString(),
            "success");

  InstallShortcutAppForCurrentUrl();
}

// Tests that Create Shortcut on non-promotable sites still uses available
// manifest data.
IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest,
                       UseNonPromotableManifestData) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURLAndWait(browser(),
                       embedded_test_server()->GetURL(
                           "/web_apps/theme_color_only_manifest.html"));
  AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetRGB(0x12, 0x34, 0x56));
}

// Tests that Create Shortcut won't use manifest data that's invalid.
IN_PROC_BROWSER_TEST_P(CreateShortcutBrowserTest, IgnoreInvalidManifestData) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/web_apps/invalid_start_url_manifest.html");
  NavigateToURLAndWait(browser(), url);
  AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppStartUrl(app_id), url);
}

INSTANTIATE_TEST_SUITE_P(All,
                         CreateShortcutBrowserTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

}  // namespace web_app
