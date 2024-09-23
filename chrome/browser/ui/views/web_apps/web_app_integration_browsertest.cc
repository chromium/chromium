// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "web_app_integration_test_driver.h"

namespace web_app::integration_tests {
namespace {

using WebAppIntegration = WebAppIntegrationTest;

// Manual tests:

IN_PROC_BROWSER_TEST_F(WebAppIntegration, EnterAndExitFullScreenApp) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.EnterFullScreenApp();
  helper_.ExitFullScreenApp();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, UninstallFromList) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.UninstallFromList(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateScope) {
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateIcon) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.ManifestUpdateIcon(Site::kStandalone,
                             UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateTitleAccept) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       ManifestUpdateTitleSkipUninstallAccept) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.ManifestUpdateTitle(
      Site::kStandalone, Title::kStandaloneUpdated,
      UpdateDialogResponse::kCancelUninstallAndAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateTitleCancel) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckPlatformShortcutNotExists(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdatePolicyAppTitle) {
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kSkipDialog);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, LaunchFromMenuOption) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, OpenInChrome) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.OpenInChrome();
  helper_.CheckTabCreated(Number::kOne);
}

// TODO(crbug.com/41490445): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_ManifestUpdateDisplayBrowser DISABLED_ManifestUpdateDisplayBrowser
#else
#define MAYBE_ManifestUpdateDisplayBrowser ManifestUpdateDisplayBrowser
#endif
IN_PROC_BROWSER_TEST_F(WebAppIntegration, MAYBE_ManifestUpdateDisplayBrowser) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kBrowser);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       ManifestUpdateDisplayOverrideWindowControlsOverlay) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WindowControlsOverlayNotEnabledWithoutWCOManifest) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlay(Site::kStandalone, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ToggleWindowControlsOverlay) {
  helper_.CreateShortcut(Site::kWco, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.DisableWindowControlsOverlay(Site::kWco);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WindowControlsOverlayStatePreservesBetweenLaunches) {
  helper_.CreateShortcut(Site::kWco, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.ClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, SwitchIncognitoProfile) {
  helper_.SwitchIncognitoProfile();
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckCreateShortcutNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       AppsWithoutServiceWorkerCanBeInstalledViaMenu) {
  helper_.InstallMenuOption(InstallableSite::kNoServiceWorker);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kNoServiceWorker);
  helper_.CheckAppNavigationIsStartUrl();
  helper_.NavigateBrowser(Site::kNoServiceWorker);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckAppInListIconCorrect) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckAppInListIconCorrectFails) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);  // green icon
  EXPECT_NONFATAL_FAILURE(helper_.CheckAppInListIconCorrect(Site::kMinimalUi),
                          "expected_color");  // black icon
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       CheckAppInListIconCorrectAfterInstallPolicyApp) {
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckAppNavigation) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckAppNavigationFails) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  EXPECT_NONFATAL_FAILURE(helper_.CheckAppNavigation(Site::kStandaloneNestedB),
                          "webapps_integration/standalone/basic.html");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, RicherInstallModal) {
  helper_.InstallOmniboxIcon(InstallableSite::kScreenshots);
  helper_.CheckAppInListWindowed(Site::kScreenshots);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckBrowserNavigation) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckBrowserNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckBrowserNavigationFails) {
#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kShortcutsNotApps)) {
    GTEST_SKIP()
        << "Explicit skip to prevent EXPECT_NONFATAL_FAILURE to be triggered";
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kBrowser);
  EXPECT_NONFATAL_FAILURE(helper_.CheckBrowserNavigation(Site::kStandalone),
                          "webapps_integration/standalone/foo/basic.html");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckSubAppInstallation) {
  helper_.InstallIsolatedApp(Site::kHasSubApps);
  helper_.CheckNoSubApps(Site::kHasSubApps);
  helper_.InstallSubApp(Site::kHasSubApps, Site::kSubApp1,
                        SubAppInstallDialogOptions::kUserAllow);
  helper_.CheckHasSubApp(Site::kHasSubApps, Site::kSubApp1);
  helper_.CheckNotHasSubApp(Site::kHasSubApps, Site::kSubApp2);
  helper_.CheckAppInListWindowed(Site::kSubApp1);
  EXPECT_NONFATAL_FAILURE(helper_.CheckNoSubApps(Site::kHasSubApps),
                          "Expected equality of these values");
  helper_.RemoveSubApp(Site::kHasSubApps, Site::kSubApp1);
  helper_.CheckNoSubApps(Site::kHasSubApps);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, NewAppTab) {
  helper_.CreateShortcut(Site::kTabbed, WindowOptions::kWindowed);
  helper_.CheckAppNavigation(Site::kTabbed);
  helper_.NewAppTab(Site::kTabbed);
  helper_.CheckAppTabCreated();
  helper_.CheckAppTabIsSite(Site::kTabbed, Number::kOne);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckSiteHandlesFile) {
  helper_.InstallMenuOption(InstallableSite::kFileHandler);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckSiteNotHandlesFile) {
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckSiteNotHandlesFile(Site::kStandalone, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kStandalone, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, DisableEnableFileHandling) {
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kBar);

  helper_.DisableFileHandling(Site::kMinimalUi);
  helper_.CheckSiteNotHandlesFile(Site::kMinimalUi, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kMinimalUi, FileExtension::kBar);

  helper_.EnableFileHandling(Site::kMinimalUi);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kBar);
}

// Generated tests:

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableBrowser_11NotPromotable_7NotPromotable_37NotPromotable_17_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableBrowser_11NotPromotable_7NotPromotable_10NotPromotable_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.UninstallFromList(Site::kNotPromotable);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableWindowed_12NotPromotable_7NotPromotable_37NotPromotable_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableWindowed_12NotPromotable_7NotPromotable_69NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromMenuOption(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableWindowed_12NotPromotable_7NotPromotable_35NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromLaunchIcon(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableWindowed_12NotPromotable_7NotPromotable_34NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromChromeApps(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableWindowed_12NotPromotable_7NotPromotable_10NotPromotable_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.UninstallFromList(Site::kNotPromotable);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29MinimalUiWindowed_37MinimalUi_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29MinimalUiWindowed_69MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29MinimalUiWindowed_35MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29MinimalUiWindowed_34MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_31MinimalUi_37MinimalUi_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_31MinimalUi_69MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_31MinimalUi_35MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_31MinimalUi_34MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_47MinimalUi_37MinimalUi_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_47MinimalUi_69MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_47MinimalUi_35MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_47MinimalUi_34MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29StandaloneNestedAWindowed_37NotInstalled_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.NavigateBrowser(Site::kNotInstalled);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_31StandaloneNestedA_37NotInstalled_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kNotInstalled);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_47StandaloneNestedA_37NotInstalled_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kNotInstalled);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_37Standalone_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_37StandaloneNestedA_18) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_37NotPromotable_15Standalone_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_38_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.NavigateNotfoundUrl();
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_69StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_35StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_34StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedB_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_69StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_35StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_34StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedB_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_69StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_35StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_34StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedB_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableNoShortcutWindowedWebApp_12NotPromotable_37NotPromotable_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableNoShortcutWindowedWebApp_12NotPromotable_69NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.LaunchFromMenuOption(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableNoShortcutWindowedWebApp_12NotPromotable_35NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.LaunchFromLaunchIcon(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableNoShortcutWindowedWebApp_12NotPromotable_34NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.LaunchFromChromeApps(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableNoShortcutBrowserWebApp_11NotPromotable_37NotPromotable_17_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kNotPromotable);
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableWithShortcutBrowserWebApp_11NotPromotable_7NotPromotable_37NotPromotable_17_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_37MinimalUi_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_69MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_35MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_34MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_32MinimalUiNoShortcutWindowedWebApp_37MinimalUi_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_32MinimalUiNoShortcutWindowedWebApp_69MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_32MinimalUiNoShortcutWindowedWebApp_35MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_32MinimalUiNoShortcutWindowedWebApp_34MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableWithShortcutWindowedWebApp_12NotPromotable_7NotPromotable_37NotPromotable_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableWithShortcutWindowedWebApp_12NotPromotable_7NotPromotable_69NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromMenuOption(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableWithShortcutWindowedWebApp_12NotPromotable_7NotPromotable_35NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromLaunchIcon(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableWithShortcutWindowedWebApp_12NotPromotable_7NotPromotable_34NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromChromeApps(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedAWithShortcutWindowedWebApp_37NotInstalled_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.NavigateBrowser(Site::kNotInstalled);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedANoShortcutWindowedWebApp_37NotInstalled_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.NavigateBrowser(Site::kNotInstalled);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_31Screenshots) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kScreenshots);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebShortcut_15Standalone_75StandaloneNotStartUrl_69StandaloneNotStartUrl_133StandaloneNotStartUrl) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebShortcut);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandaloneNotStartUrl);
  helper_.LaunchFromMenuOption(Site::kStandaloneNotStartUrl);
  helper_.CheckAppNavigation(Site::kStandaloneNotStartUrl);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebShortcut_15Standalone_75StandaloneNotStartUrl_35StandaloneNotStartUrl_133StandaloneNotStartUrl) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebShortcut);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandaloneNotStartUrl);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNotStartUrl);
  helper_.CheckAppNavigation(Site::kStandaloneNotStartUrl);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebShortcut_15Standalone_75StandaloneNotStartUrl_34StandaloneNotStartUrl_133StandaloneNotStartUrl) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebShortcut);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandaloneNotStartUrl);
  helper_.LaunchFromChromeApps(Site::kStandaloneNotStartUrl);
  helper_.CheckAppNavigation(Site::kStandaloneNotStartUrl);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebApp_15StandaloneNotStartUrl_75Standalone_69Standalone_133Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppNotInList(Site::kStandaloneNotStartUrl);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebApp_15StandaloneNotStartUrl_75Standalone_35Standalone_133Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppNotInList(Site::kStandaloneNotStartUrl);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebApp_15StandaloneNotStartUrl_75Standalone_34Standalone_133Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppNotInList(Site::kStandaloneNotStartUrl);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutBrowserWebApp_15StandaloneNotStartUrl_75Standalone_34Standalone_134Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppNotInList(Site::kStandaloneNotStartUrl);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckBrowserNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutBrowserWebShortcut_15Standalone_75StandaloneNotStartUrl_34StandaloneNotStartUrl_134StandaloneNotStartUrl) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebShortcut);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandaloneNotStartUrl);
  helper_.LaunchFromChromeApps(Site::kStandaloneNotStartUrl);
  helper_.CheckBrowserNavigation(Site::kStandaloneNotStartUrl);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29MinimalUiWindowed_116MinimalUiWco_117MinimalUi_143_69MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29MinimalUiWindowed_116MinimalUiWco_117MinimalUi_143_35MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29MinimalUiWindowed_116MinimalUiWco_117MinimalUi_143_34MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31MinimalUi_116MinimalUiWco_117MinimalUi_143_69MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31MinimalUi_116MinimalUiWco_117MinimalUi_143_35MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31MinimalUi_116MinimalUiWco_117MinimalUi_143_34MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_143_69MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_143_35MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_143_34MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiNoShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_143_69MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiNoShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_143_35MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiNoShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_143_34MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47MinimalUi_116MinimalUiWco_117MinimalUi_143_69MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47MinimalUi_116MinimalUiWco_117MinimalUi_143_35MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47MinimalUi_116MinimalUiWco_117MinimalUi_143_34MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedAWithShortcutWindowedWebApp_8StandaloneNestedAStandalone_117StandaloneNestedA_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedAWithShortcutWindowedWebApp_8StandaloneNestedAStandalone_117StandaloneNestedA_69StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedAWithShortcutWindowedWebApp_8StandaloneNestedAStandalone_117StandaloneNestedA_35StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedAWithShortcutWindowedWebApp_8StandaloneNestedAStandalone_117StandaloneNestedA_34StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedAWithShortcutWindowedWebApp_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedB_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNestedAWithShortcutWindowedWebApp_8StandaloneNestedAStandalone_117StandaloneNestedA_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_37Tabbed_20_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.NavigateBrowser(Site::kTabbed);
  helper_.CheckLaunchIconShown();
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_143_69Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_143_35Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

// Disabled on Mac due to flakiness: https://crbug.com/1478373.
#if BUILDFLAG(IS_MAC)
#define MAYBE_WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144 \
  DISABLED_WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144
#else
#define MAYBE_WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144 \
  WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144
#endif
IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    MAYBE_WAI_32TabbedWithShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_37Tabbed_20_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.NavigateBrowser(Site::kTabbed);
  helper_.CheckLaunchIconShown();
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_143_69Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_143_35Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

// Disabled on Mac due to flakiness: https://crbug.com/1478373.
#if BUILDFLAG(IS_MAC)
#define MAYBE_WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144 \
  DISABLED_WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144
#else
#define MAYBE_WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144 \
  WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144
#endif
IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    MAYBE_WAI_32TabbedNoShortcutWindowedWebApp_12Tabbed_143_34Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kTabbed, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_47Tabbed_12Tabbed_37Tabbed_20_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.NavigateBrowser(Site::kTabbed);
  helper_.CheckLaunchIconShown();
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_47Tabbed_12Tabbed_143_69Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_47Tabbed_12Tabbed_143_35Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_47Tabbed_12Tabbed_143_34Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_73_166_167) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.SwitchIncognitoProfile();
  helper_.NavigateAppHome();
  helper_.CheckBrowserNotAtAppHome();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_73_37Standalone_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.SwitchIncognitoProfile();
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_73_37NotPromotable_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.SwitchIncognitoProfile();
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_69Standalone_79StandaloneStandaloneOriginal_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_35Standalone_79StandaloneStandaloneOriginal_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_34Standalone_79StandaloneStandaloneOriginal_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedAcceptUpdate_117Standalone_69Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedAcceptUpdate_117Standalone_35Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedAcceptUpdate_117Standalone_34Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelDialogAndUninstall_117Standalone_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_68StandaloneAcceptUpdate_117Standalone_110StandaloneRed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone,
                             UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_39StandaloneMinimalUi_16_79StandaloneStandaloneOriginal_27_14) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CheckCustomToolbar();
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CloseCustomToolbar();
  helper_.CheckAppNavigationIsStartUrl();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_39StandaloneMinimalUi_16_79StandaloneStandaloneOriginal_71_22One) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CheckCustomToolbar();
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.OpenInChrome();
  helper_.CheckTabCreated(Number::kOne);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_10Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromList(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_149Standalone_11Standalone_37Standalone_18) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.SetOpenInTabFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_71_22One_163Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenInChrome();
  helper_.CheckTabCreated(Number::kOne);
  helper_.CheckAppLoadedInTab(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutWindowedWebApp_7Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_37Standalone_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_37Standalone_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutWindowedWebApp_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_69Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_35Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_34Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37MinimalUi_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_73_37Standalone_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.SwitchIncognitoProfile();
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_69Standalone_79StandaloneStandaloneOriginal_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_35Standalone_79StandaloneStandaloneOriginal_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_34Standalone_79StandaloneStandaloneOriginal_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedAcceptUpdate_117Standalone_69Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedAcceptUpdate_117Standalone_35Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedAcceptUpdate_117Standalone_34Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelDialogAndUninstall_117Standalone_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated,
                              UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_68StandaloneAcceptUpdate_117Standalone_110StandaloneRed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone,
                             UpdateDialogResponse::kAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_39StandaloneMinimalUi_16_79StandaloneStandaloneOriginal_27_14) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CheckCustomToolbar();
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CloseCustomToolbar();
  helper_.CheckAppNavigationIsStartUrl();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_39StandaloneMinimalUi_16_79StandaloneStandaloneOriginal_71_22One) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CheckCustomToolbar();
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.OpenInChrome();
  helper_.CheckTabCreated(Number::kOne);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_10Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromList(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_149Standalone_11Standalone_37Standalone_18) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.SetOpenInTabFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_71_22One_163Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenInChrome();
  helper_.CheckTabCreated(Number::kOne);
  helper_.CheckAppLoadedInTab(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutWindowedWebApp_7Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_37Standalone_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_37Standalone_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutWindowedWebApp_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_69Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_35Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_34Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37MinimalUi_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_73_37Standalone_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.SwitchIncognitoProfile();
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_39StandaloneMinimalUi_27_14) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CloseCustomToolbar();
  helper_.CheckAppNavigationIsStartUrl();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_44Standalone_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_69Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_35Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_34Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_116StandaloneTabbed_143_117Standalone_69Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_116StandaloneTabbed_143_117Standalone_35Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_116StandaloneTabbed_143_117Standalone_34Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_7Standalone_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_44Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_150Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_150Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_150Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_29StandaloneWindowed_12Standalone_7Standalone_24_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_31Standalone_12Standalone_7Standalone_24_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_7Standalone_47Standalone_12Standalone_7Standalone_24_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_39StandaloneMinimalUi_27_14) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CloseCustomToolbar();
  helper_.CheckAppNavigationIsStartUrl();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_44Standalone_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_69Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_35Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_34Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_79StandaloneStandaloneOriginal_12Standalone_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_44Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_150Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_150Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_150Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_29StandaloneWindowed_12Standalone_7Standalone_24_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_31Standalone_12Standalone_7Standalone_24_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_79StandaloneStandaloneOriginal_11Standalone_47Standalone_12Standalone_7Standalone_24_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_39StandaloneMinimalUi_16_79StandaloneStandaloneOriginal_27_14) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CheckCustomToolbar();
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CloseCustomToolbar();
  helper_.CheckAppNavigationIsStartUrl();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_39StandaloneMinimalUi_16_79StandaloneStandaloneOriginal_71_22One) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigatePwa(Site::kStandalone, Site::kMinimalUi);
  helper_.CheckCustomToolbar();
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.OpenInChrome();
  helper_.CheckTabCreated(Number::kOne);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_10Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromList(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoWithShortcutWindowedWebApp_116WcoStandalone_117Wco_143_69Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoWithShortcutWindowedWebApp_116WcoStandalone_117Wco_143_35Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoWithShortcutWindowedWebApp_116WcoStandalone_117Wco_143_34Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoNoShortcutWindowedWebApp_116WcoStandalone_117Wco_143_69Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoNoShortcutWindowedWebApp_116WcoStandalone_117Wco_143_35Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoNoShortcutWindowedWebApp_116WcoStandalone_117Wco_143_34Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_69Standalone_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_35Standalone_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_34Standalone_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_149Standalone_11Standalone_37Standalone_18) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.SetOpenInTabFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_71_22One_163Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenInChrome();
  helper_.CheckTabCreated(Number::kOne);
  helper_.CheckAppLoadedInTab(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutWindowedWebApp_7Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_37Standalone_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneNoShortcutBrowserWebApp_7Standalone_12Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_37Standalone_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutBrowserWebApp_12Standalone_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_32StandaloneWithShortcutWindowedWebApp_44Standalone_12Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_69Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_35Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneTabbed_143_117Standalone_34Standalone_24_94_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.MaybeClosePwa();
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37StandaloneNestedA_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_37MinimalUi_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_73_37Standalone_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.SwitchIncognitoProfile();
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_10Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.UninstallFromList(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_150Standalone_12Standalone_69Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_150Standalone_12Standalone_35Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_150Standalone_12Standalone_34Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_150Standalone_12Standalone_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindowFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_32StandaloneWithShortcutWindowedWebApp_7Standalone_11Standalone_37Standalone_18) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_32StandaloneWithShortcutWindowedWebApp_7Standalone_11Standalone_44Standalone_11Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_32StandaloneNoShortcutWindowedWebApp_7Standalone_11Standalone_37Standalone_18) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_32StandaloneNoShortcutWindowedWebApp_7Standalone_11Standalone_44Standalone_11Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_32StandaloneWithShortcutBrowserWebApp_44Standalone_11Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_79StandaloneStandaloneOriginal_11Standalone_7Standalone_32StandaloneNoShortcutBrowserWebApp_44Standalone_11Standalone_7Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29TabbedWindowed_12Tabbed_37Tabbed_20_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kTabbed, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.NavigateBrowser(Site::kTabbed);
  helper_.CheckLaunchIconShown();
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29TabbedWindowed_12Tabbed_143_69Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kTabbed, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29TabbedWindowed_12Tabbed_143_35Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kTabbed, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29TabbedWindowed_12Tabbed_143_34Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kTabbed, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_31Tabbed_12Tabbed_37Tabbed_20_17) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.NavigateBrowser(Site::kTabbed);
  helper_.CheckLaunchIconShown();
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_31Tabbed_12Tabbed_143_69Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_31Tabbed_12Tabbed_143_35Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_31Tabbed_12Tabbed_143_34Tabbed_144) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelUninstallAndAcceptUpdate_117Standalone_69Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(
      Site::kStandalone, Title::kStandaloneUpdated,
      UpdateDialogResponse::kCancelUninstallAndAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelUninstallAndAcceptUpdate_117Standalone_35Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(
      Site::kStandalone, Title::kStandaloneUpdated,
      UpdateDialogResponse::kCancelUninstallAndAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelUninstallAndAcceptUpdate_117Standalone_34Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(
      Site::kStandalone, Title::kStandaloneUpdated,
      UpdateDialogResponse::kCancelUninstallAndAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelUninstallAndAcceptUpdate_117Standalone_69Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(
      Site::kStandalone, Title::kStandaloneUpdated,
      UpdateDialogResponse::kCancelUninstallAndAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelUninstallAndAcceptUpdate_117Standalone_35Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(
      Site::kStandalone, Title::kStandaloneUpdated,
      UpdateDialogResponse::kCancelUninstallAndAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_79StandaloneStandaloneOriginal_24_12Standalone_7Standalone_112StandaloneNotShown_88StandaloneStandaloneUpdatedCancelUninstallAndAcceptUpdate_117Standalone_34Standalone_79StandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(
      Site::kStandalone, Title::kStandaloneUpdated,
      UpdateDialogResponse::kCancelUninstallAndAcceptUpdate);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_CreateShortcutChromeUrlWindowed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.CreateShortcut(Site::kChromeUrl, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_InstallMenuOptionChromeURL) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kChromeUrl);
  helper_.CheckWindowCreated();
}

}  // namespace
}  // namespace web_app::integration_tests
