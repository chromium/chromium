// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_paths.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/chrome_features.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace {

std::string LoadExtension(Profile* profile, const base::FilePath& path) {
  extensions::ChromeTestExtensionLoader loader(profile);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(path);
  EXPECT_TRUE(extension);
  return extension->id();
}

}  // namespace

namespace web_app {

// TODO(crbug.com/344912771): Remove once ShortcutsNotApps launches to 100%
// Stable.
class CreateShortcutBrowserTest : public WebAppBrowserTestBase {
 public:
  CreateShortcutBrowserTest() {
#if !BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.InitAndDisableFeature(features::kShortcutsNotApps);
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }
  webapps::AppId InstallShortcutAppForCurrentUrl(bool open_as_window = false) {
    SetAutoAcceptWebAppDialogForTesting(true, open_as_window);
    WebAppTestInstallObserver observer(profile());
    observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
    webapps::AppId app_id = observer.Wait();
    SetAutoAcceptWebAppDialogForTesting(false, false);
    return app_id;
  }

  // Start URL points to `PageWithDifferentStartUrlManifestStartUrl`.
  GURL PageWithDifferentStartUrl() {
    return https_server()->GetURL("/web_apps/different_start_url.html");
  }

  GURL PageWithDifferentStartUrlManifestStartUrl() {
    return https_server()->GetURL("/web_apps/basic.html");
  }

  WebAppRegistrar& registrar() {
    auto* provider = WebAppProvider::GetForTest(profile());
    CHECK(provider);
    return provider->registrar_unsafe();
  }

  WebAppSyncBridge& sync_bridge() {
    auto* provider = WebAppProvider::GetForTest(profile());
    CHECK(provider);
    return provider->sync_bridge_unsafe();
  }

#if !BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       CreateShortcutForInstallableSite) {
  base::UserActionTester user_action_tester;
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  webapps::AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppShortName(app_id), GetInstallableAppName());
  // Shortcut apps to PWAs should launch in a tab.
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kBrowser);

  EXPECT_EQ(0, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("CreateShortcut"));
}

// TODO(crbug.com/40269598): flaky on Mac11 Tests builder.
#if BUILDFLAG(IS_MAC)
#define MAYBE_InstallSourceRecorded DISABLED_InstallSourceRecorded
#else
#define MAYBE_InstallSourceRecorded InstallSourceRecorded
#endif
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest, MAYBE_InstallSourceRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // LatestWebAppInstallSource should be correctly set and reported to UMA for
  // both installable and non-installable (shortcut) sites.
  for (const GURL& url :
       {GetInstallableAppURL(),
        embedded_test_server()->GetURL(
            "/web_apps/get_manifest.html?theme_color_only.json")}) {
    base::HistogramTester histogram_tester;
    NavigateViaLinkClickToURLAndWait(browser(), url);
    webapps::AppId app_id = InstallShortcutAppForCurrentUrl();

    EXPECT_EQ(webapps::WebappInstallSource::MENU_CREATE_SHORTCUT,
              *registrar().GetLatestAppInstallSource(app_id));
    histogram_tester.ExpectUniqueSample(
        "Webapp.Install.InstallEvent",
        static_cast<int>(webapps::WebappInstallSource::MENU_CREATE_SHORTCUT),
        1);
  }
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       CanInstallOverTabShortcutApp) {
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());
  InstallShortcutAppForCurrentUrl();

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       CannotInstallOverWindowShortcutApp) {
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());
  webapps::AppId app_id = InstallShortcutAppForCurrentUrl();
  // Change launch container to open in window.
  sync_bridge().SetAppUserDisplayModeForTesting(
      app_id, mojom::UserDisplayMode::kStandalone);

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
//
// TODO(crbug.com/40793595): Remove chrome-extension scheme for web apps.
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       ShouldShowCustomTabBarForExtensionPage) {
  // This involves the creation of a regular (non-app) extension with a popup
  // page, and the creation of a shortcut app created from the popup page URL
  // (allowing the extension's popup page to be loaded in a window).

  base::FilePath test_data_dir_;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);

  // Install the extension that has the popup page.
  std::string extension_id =
      LoadExtension(profile(), test_data_dir_.AppendASCII("extensions")
                                   .AppendASCII("ui")
                                   .AppendASCII("browser_action_popup"));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Install the shortcut app that links to the extension's popup page.
  const GURL popup_url("chrome-extension://" + extension_id + "/popup.html");

  NavigateViaLinkClickToURLAndWait(browser(), popup_url);

  // TODO(crbug.com/40793595): IDC_CREATE_SHORTCUT command must become disabled.
  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));

  const webapps::AppId app_id = InstallShortcutAppForCurrentUrl();
  ASSERT_FALSE(app_id.empty());
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  CHECK(app_browser);
  CHECK(app_browser != browser());

  // Navigate to the app's launch page; the toolbar should not be visible,
  // because extensions pages are secure.
  NavigateAndCheckForToolbar(app_browser, popup_url, false);
}

// Tests that Create Shortcut doesn't timeout on a page that has a delayed
// iframe load. Context: crbug.com/1046883
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest, WorksAfterDelayedIFrameLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateViaLinkClickToURLAndWait(
      browser(),
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html"));

  // Append an iframe and wait for it to finish loading.
  const char script[] = R"(
    const iframe = document.createElement('iframe');
    new Promise(resolve => {
      iframe.onload = _ => resolve('success');
      iframe.srcdoc = 'inner page';
      document.body.appendChild(iframe);
    });
  )";
  EXPECT_EQ(content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(), script)
                .ExtractString(),
            "success");

  InstallShortcutAppForCurrentUrl();
}

// Tests that Create Shortcut on non-promotable sites still uses available
// manifest data.
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       UseNonPromotableManifestData) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateViaLinkClickToURLAndWait(
      browser(), embedded_test_server()->GetURL(
                     "/web_apps/get_manifest.html?theme_color_only.json"));
  webapps::AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetRGB(0x12, 0x34, 0x56));
}

// Tests that Create Shortcut won't use manifest data that's invalid.
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest, IgnoreInvalidManifestData) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?invalid_start_url.json");
  NavigateViaLinkClickToURLAndWait(browser(), url);
  webapps::AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppStartUrl(app_id), url);
}

// TODO(crbug.com/40883914): Un-flake and re-enable this test.
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       DISABLED_CreateShortcutAgainOverwriteUserDisplayMode) {
  base::UserActionTester user_action_tester;
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  webapps::AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppShortName(app_id), GetInstallableAppName());
  // Shortcut apps to PWAs should launch in a tab.
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kBrowser);
  // TODO(crbug.com/40808578): We need to wait a bit longer for the
  // WebAppInstallTask to complete before starting another install.
  // Move the install/update/uninstall events out of
  // WebAppRegistrarObserver and into a WebAppInstallManagerObserver
  // interface so they can be guaranteed to fire after the
  // WebAppInstallTask's lifetime has ended.
  base::RunLoop().RunUntilIdle();

  InstallShortcutAppForCurrentUrl(/*open_as_window=*/true);
  // Re-install with enabling open_as_window should update user display mode.
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
}

// TODO(crbug.com/40908616): Re-enable this test
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       DISABLED_OpenShortcutWindowOnlyOnce) {
  base::UserActionTester user_action_tester;
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  WebAppTestInstallObserver observer(profile());
  // The "Create shortcut" call is executed twice, but the dialog
  // must be shown only once.
  ASSERT_TRUE(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
  ASSERT_TRUE(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));

  EXPECT_EQ(1u, provider().command_manager().GetCommandCountForTesting());
}

// Tests that Create Shortcut on sites where the title is a url generates a
// letter icon correctly and does not use the "H" letter from the "https"
// scheme.
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest, UseHostWhenTitleIsUrl) {
  NavigateViaLinkClickToURLAndWait(
      browser(), https_server()->GetURL("example.com", "/empty.html"));
  webapps::AppId app_id = InstallShortcutAppForCurrentUrl();

  base::test::TestFuture<std::map<SquareSizePx, SkBitmap>> future;
  WebAppProvider::GetForTest(profile())->icon_manager().ReadIcons(
      app_id, IconPurpose::ANY, {icon_size::k128}, future.GetCallback());

  std::map<SquareSizePx, SkBitmap> icon_bitmaps = future.Get();
  DCHECK(base::Contains(icon_bitmaps, icon_size::k128));
  SkBitmap bitmap = std::move(icon_bitmaps.at(icon_size::k128));

  // The letter for https://example.com should be the first letter of the host,
  // which is "E".
  SkBitmap generated_icon_bitmap =
      shortcuts::GenerateBitmap(icon_size::k128, static_cast<char32_t>('E'));
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, generated_icon_bitmap));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest,
                       InstallableSiteDifferentStartUrl) {
  NavigateViaLinkClickToURLAndWait(browser(), PageWithDifferentStartUrl());
  webapps::AppId app_id = InstallShortcutAppForCurrentUrl();

  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kBrowser);

  // Using the manifest makes it a non-shortcut.
  EXPECT_EQ(registrar().IsShortcutApp(app_id), false);

  // Title from manifest.
  EXPECT_EQ(registrar().GetAppShortName(app_id), "Basic web app");

  // Start URL from manifest.
  EXPECT_EQ(registrar().GetAppById(app_id)->start_url(),
            PageWithDifferentStartUrlManifestStartUrl());
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserTest, InstallOverTabShortcutApp) {
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());
  webapps::AppId shortcut_app_id = InstallShortcutAppForCurrentUrl();

  EXPECT_FALSE(registrar().IsShortcutApp(shortcut_app_id));

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);

  webapps::AppId web_app_id = test::InstallPwaForCurrentUrl(new_browser);

  EXPECT_EQ(shortcut_app_id, web_app_id);
  EXPECT_FALSE(registrar().IsShortcutApp(web_app_id));
}

}  // namespace web_app
