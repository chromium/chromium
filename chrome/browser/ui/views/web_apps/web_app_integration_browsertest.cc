// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "web_app_integration_test_driver.h"

namespace web_app::integration_tests {
namespace {

using WebAppIntegration = WebAppIntegrationTest;

// Manual tests:

IN_PROC_BROWSER_TEST_F(WebAppIntegration, EnterAndExitFullScreenApp) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.EnterFullScreenApp();
  helper_.ExitFullScreenApp();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, UninstallFromList) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.UninstallFromList(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateScope) {
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateIcon) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateTitleAccept) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdateTitleCancel) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckPlatformShortcutNotExists(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ManifestUpdatePolicyAppTitle) {
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, LaunchFromMenuOption) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, OpenInChrome) {
  helper_.InstallMenuOption(Site::kStandalone);
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
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kBrowser);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       ManifestUpdateDisplayOverrideWindowControlsOverlay) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kWco);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WindowControlsOverlayNotEnabledWithoutWCOManifest) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlay(Site::kStandalone, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, ToggleWindowControlsOverlay) {
  helper_.InstallMenuOption(Site::kWco);
  helper_.CheckWindowCreated();
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.DisableWindowControlsOverlay(Site::kWco);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WindowControlsOverlayStatePreservesBetweenLaunches) {
  helper_.InstallMenuOption(Site::kWco);
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
  helper_.InstallMenuOption(Site::kNoServiceWorker);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kNoServiceWorker);
  helper_.CheckAppNavigationIsStartUrl();
  helper_.NavigateBrowser(Site::kNoServiceWorker);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckAppInListIconCorrect) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckAppInListIconCorrectFails) {
  helper_.InstallMenuOption(Site::kStandalone);  // green icon
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
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckAppNavigationFails) {
  helper_.InstallMenuOption(Site::kStandalone);
  EXPECT_NONFATAL_FAILURE(helper_.CheckAppNavigation(Site::kStandaloneNestedB),
                          "webapps_integration/standalone/basic.html");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, RicherInstallModal) {
  helper_.InstallOmniboxIcon(InstallableSite::kScreenshots);
  helper_.CheckAppInListWindowed(Site::kScreenshots);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckBrowserNavigation) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.SetOpenInTabFromAppSettings(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckBrowserNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckBrowserNavigationFails) {
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.SetOpenInTabFromAppSettings(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  EXPECT_NONFATAL_FAILURE(helper_.CheckBrowserNavigation(Site::kStandalone),
                          "webapps_integration/standalone/foo/basic.html");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, AppLaunchedInTab) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.SetOpenInTabFromAppSettings(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppLoadedInTab(Site::kStandalone);
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
  helper_.InstallMenuOption(Site::kTabbed);
  helper_.CheckAppNavigation(Site::kTabbed);
  helper_.NewAppTab(Site::kTabbed);
  helper_.CheckAppTabCreated();
  helper_.CheckAppTabIsSite(Site::kTabbed, Number::kOne);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckSiteHandlesFile) {
  helper_.InstallMenuOption(Site::kFileHandler);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckSiteNotHandlesFile) {
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckSiteNotHandlesFile(Site::kStandalone, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kStandalone, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, DisableEnableFileHandling) {
  helper_.InstallMenuOption(Site::kMinimalUi);
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

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_31MinimalUi_37MinimalUi_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckLaunchIconShown();
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

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuOptionChromeUrlWindowed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kChromeUrl);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_InstallMenuStandalone_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_InstallMenuStandalone_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuMinimalUi_NavigateBrowserMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.NavigateBrowser(Site::kMinimalUi);
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuTabbed_NavigateBrowserTabbed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.NavigateBrowser(Site::kTabbed);
  helper_.CheckLaunchIconShown();
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuTabbed_MaybeClosePwa_LaunchFromMenuOptionTabbed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuTabbed_MaybeClosePwa_LaunchFromLaunchIconTabbed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuTabbed_MaybeClosePwa_LaunchFromChromeAppsTabbed) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kTabbed);
  helper_.CheckAppInListWindowed(Site::kTabbed);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kTabbed);
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuNotPromotable_NavigateBrowserNotPromotable) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kNotPromotable);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.NavigateBrowser(Site::kNotPromotable);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuNotPromotable_UninstallFromListNotPromotable) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kNotPromotable);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.UninstallFromList(Site::kNotPromotable);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuNotPromotable_LaunchFromMenuOptionNotPromotable) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kNotPromotable);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromMenuOption(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuNotPromotable_LaunchFromLaunchIconNotPromotable) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kNotPromotable);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromLaunchIcon(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuNotPromotable_LaunchFromChromeAppsNotPromotable) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kNotPromotable);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromChromeApps(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_InstallMenuChromeUrl) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kChromeUrl);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_NavigateBrowserNotInstalled) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kNotInstalled);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

// TODO(crbug.com/405233966): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_WAI_InstallOmniboxIconStandalone_NavigatePwaStandaloneMinimalUi_CloseCustomToolbar \
  DISABLED_WAI_InstallOmniboxIconStandalone_NavigatePwaStandaloneMinimalUi_CloseCustomToolbar
#else
#define MAYBE_WAI_InstallOmniboxIconStandalone_NavigatePwaStandaloneMinimalUi_CloseCustomToolbar \
  WAI_InstallOmniboxIconStandalone_NavigatePwaStandaloneMinimalUi_CloseCustomToolbar
#endif
IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    MAYBE_WAI_InstallOmniboxIconStandalone_NavigatePwaStandaloneMinimalUi_CloseCustomToolbar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_NavigatePwaStandaloneMinimalUi_OpenInChrome) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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

// TODO(crbug.com/405233966): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_WAI_InstallOmniboxIconStandalone_UninstallFromListStandalone_NavigateBrowserStandalone \
  DISABLED_WAI_InstallOmniboxIconStandalone_UninstallFromListStandalone_NavigateBrowserStandalone
#else
#define MAYBE_WAI_InstallOmniboxIconStandalone_UninstallFromListStandalone_NavigateBrowserStandalone \
  WAI_InstallOmniboxIconStandalone_UninstallFromListStandalone_NavigateBrowserStandalone
#endif
IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    MAYBE_WAI_InstallOmniboxIconStandalone_UninstallFromListStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_SetOpenInTabFromAppSettingsStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallOmniboxIconStandalone_OpenInChrome) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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

// TODO(crbug.com/405233966): Re-enable this test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromLaunchIconStandalone \
  WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromLaunchIconStandalone
#else
#define MAYBE_WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromLaunchIconStandalone \
  WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromLaunchIconStandalone
#endif
IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    MAYBE_WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_NavigateBrowserMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconStandalone_SwitchIncognitoProfile_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_NavigatePwaStandaloneMinimalUi_CloseCustomToolbar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_NavigatePwaStandaloneMinimalUi_OpenInChrome) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_UninstallFromListStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_SetOpenInTabFromAppSettingsStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuStandalone_OpenInChrome) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneNoShortcutBrowserWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneWithShortcutBrowserWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_UninstallPolicyAppStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuStandalone_NavigateBrowserMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallMenuStandalone_SwitchIncognitoProfile_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
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
    WAI_InstallOmniboxIconMinimalUi_LaunchFromMenuOptionMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_LaunchFromLaunchIconMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_LaunchFromChromeAppsMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_LaunchFromMenuOptionMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_LaunchFromLaunchIconMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_LaunchFromChromeAppsMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_LaunchFromMenuOptionMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_LaunchFromLaunchIconMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_LaunchFromChromeAppsMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuMinimalUi_LaunchFromMenuOptionMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuMinimalUi_LaunchFromLaunchIconMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_InstallMenuMinimalUi_LaunchFromChromeAppsMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseAcceptUpdate) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseCancelDialogAndUninstall) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseCancelDialogAndUninstall) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandalone_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseAcceptUpdate) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseCancelDialogAndUninstall) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseCancelDialogAndUninstall) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kCancelDialogAndUninstall);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromMenuOptionStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromLaunchIconStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_ManifestUpdateIconStandalone_TriggerUpdateDialogAndHandleResponseIgnoreDialog_LaunchFromChromeAppsStandalone_TriggerUpdateDialogAndHandleResponseAcceptUpdate_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(
      MenuButtonState::kExpandedUpdateAvailable);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kIgnoreDialog);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.TriggerUpdateDialogAndHandleResponse(
      UpdateDialogResponse::kAcceptUpdate);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandalone_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_LaunchFromMenuOptionStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_LaunchFromMenuOptionStandalone_ManifestUpdateIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_LaunchFromLaunchIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_LaunchFromLaunchIconStandalone_ManifestUpdateIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_LaunchFromChromeAppsStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_LaunchFromChromeAppsStandalone_ManifestUpdateIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromMenuOptionStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromLaunchIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsStandalone_ManifestUpdateDisplayStandaloneTabbed_MaybeClosePwa_LaunchFromChromeAppsStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kTabbed);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckTabNotCreated();
  helper_.CheckWindowDisplayTabbed();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_LaunchFromMenuOptionStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_LaunchFromMenuOptionStandalone_ManifestUpdateIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromMenuOption(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_LaunchFromLaunchIconStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_LaunchFromLaunchIconStandalone_ManifestUpdateIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromLaunchIcon(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_LaunchFromChromeAppsStandalone_ManifestUpdateTitleStandaloneStandaloneUpdated) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateTitle(Site::kStandalone, Title::kStandaloneUpdated);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneUpdated);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNoShortcutWindowedWebApp_LaunchFromChromeAppsStandalone_ManifestUpdateIconStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppTitle(Site::kStandalone, Title::kStandaloneOriginal);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromChromeApps(Site::kStandalone);
  helper_.CheckAppIcon(Site::kStandalone, Color::kGreen);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateIcon(Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.CheckAppIcon(Site::kStandalone, Color::kRed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconMinimalUi_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppMinimalUiNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromMenuOptionMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromLaunchIconMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuMinimalUi_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_ManifestUpdateDisplayMinimalUiWco_MaybeClosePwa_LaunchFromChromeAppsMinimalUi_EnableWindowControlsOverlayMinimalUi) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kMinimalUi);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallOmniboxIconStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppStandaloneNestedAWithShortcutWindowedWebApp_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNestedA,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromMenuOptionStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromLaunchIconStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromMenuOptionStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromMenuOption(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromLaunchIconStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromLaunchIcon(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_LaunchFromChromeAppsStandaloneNestedA_NavigatePwaStandaloneNestedAStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedB) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedB);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallMenuStandaloneNestedA_LaunchFromChromeAppsStandaloneNestedA_ManifestUpdateScopeToStandaloneNestedAStandalone_NavigateBrowserStandaloneNestedA) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallMenuOption(Site::kStandaloneNestedA);
  helper_.LaunchFromChromeApps(Site::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.NavigateBrowser(Site::kStandaloneNestedA);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromMenuOptionWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromLaunchIconWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromChromeAppsWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromMenuOptionWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromLaunchIconWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromChromeAppsWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromMenuOptionWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromLaunchIconWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoWithShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromChromeAppsWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromMenuOptionWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromLaunchIconWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromMenuOptionWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromChromeAppsWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromMenuOptionWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromLaunchIconWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromLaunchIconWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromChromeAppsWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromMenuOptionWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromLaunchIconWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_InstallPolicyAppWcoNoShortcutWindowedWebApp_MaybeClosePwa_LaunchFromChromeAppsWco_ManifestUpdateDisplayWcoStandalone_MaybeClosePwa_LaunchFromChromeAppsWco) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Gardeners: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.CheckMenuButtonPendingUpdate(MenuButtonState::kNotExpanded);
  helper_.MaybeClosePwa();
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

}  // namespace
}  // namespace web_app::integration_tests
