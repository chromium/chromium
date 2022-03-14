// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

using WebAppIntegrationBrowserTestMacWinLinux = WebAppIntegrationBrowserTest;

// Manual tests:

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckPlatformShortcutAndIcon) {
  helper_.CheckPlatformShortcutNotExists("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteB");
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckPlatformShortcutAndIcon("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteB");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckPolicyAppUninstallWorks) {
  helper_.InstallPolicyAppWindowedShortcut("SiteA");
  helper_.CheckPlatformShortcutAndIcon("SiteA");
  helper_.UninstallPolicyApp("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckRunOnOsLoginModeOnPolicyAppWorks) {
  helper_.InstallPolicyAppTabbedNoShortcut("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.EnableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginEnabled("SiteA");
  helper_.DisableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginDisabled("SiteA");
  // Clear out installed app
  helper_.UninstallPolicyApp("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckRunOnOsLoginModeOnNormalAppWorks) {
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckPlatformShortcutAndIcon("SiteA");
  helper_.EnableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginEnabled("SiteA");
  helper_.DisableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginDisabled("SiteA");
  // Clear out installed app
  helper_.UninstallFromList("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckRunOnOsLoginWorksOnPolicyAppAllowed) {
  helper_.InstallPolicyAppWindowedShortcut("SiteA");
  helper_.ApplyRunOnOsLoginPolicyAllowed("SiteA");
  helper_.EnableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginEnabled("SiteA");
  helper_.DisableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginDisabled("SiteA");
  helper_.UninstallPolicyApp("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckRunOnOsLoginWorksOnPolicyAppBlocked) {
  helper_.InstallPolicyAppWindowedShortcut("SiteA");
  helper_.ApplyRunOnOsLoginPolicyBlocked("SiteA");
  helper_.EnableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginDisabled("SiteA");
  helper_.UninstallPolicyApp("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckRunOnOsLoginWorksOnPolicyAppRunWindowed) {
  helper_.InstallPolicyAppWindowedShortcut("SiteA");
  helper_.ApplyRunOnOsLoginPolicyRunWindowed("SiteA");
  helper_.DisableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginEnabled("SiteA");
  helper_.UninstallPolicyApp("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckRunOnOsLoginWorksOnBlockedAfterUserTurnOn) {
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.EnableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginEnabled("SiteA");
  helper_.ApplyRunOnOsLoginPolicyBlocked("SiteA");
  helper_.EnableRunOnOSLogin("SiteA");
  helper_.CheckRunOnOSLoginDisabled("SiteA");
  helper_.RemoveRunOnOsLoginPolicy("SiteA");
  helper_.CheckRunOnOSLoginEnabled("SiteA");
  helper_.UninstallFromList("SiteA");
  helper_.CheckPlatformShortcutNotExists("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckLaunchFromPlatformShortcut) {
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.ClosePwa();
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckNavigateToAppSettingsFromChromeAppsWorks) {
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.OpenAppSettingsFromChromeApps("SiteA");
  helper_.CheckBrowserNavigationIsAppSettings("SiteA");
  helper_.UninstallFromMenu("SiteA");
  helper_.CheckAppNotInList("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckNavigateToAppSettingsFromAppMenu) {
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromChromeApps("SiteA");
  helper_.OpenAppSettingsFromAppMenu("SiteA");
  helper_.CheckBrowserNavigationIsAppSettings("SiteA");
  helper_.UninstallFromMenu("SiteA");
  helper_.CheckAppNotInList("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckUninstallFromAppSettingsWorks) {
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.UninstallFromAppSettings("SiteA");
  helper_.CheckAppNotInList("SiteA");
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       CheckAppSettingsAppState) {
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.CheckAppSettingsAppState("SiteA");
  helper_.UninstallFromMenu("SiteA");
  helper_.CheckAppNotInList("SiteA");
}

// Generated tests:

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_31SiteA_24_12SiteA_43SiteA_15SiteA_37SiteA_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.UninstallFromMenu("SiteA");
  helper_.CheckAppNotInList("SiteA");
  helper_.NavigateBrowser("SiteA");
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_47SiteA_24_12SiteA_43SiteA_15SiteA_37SiteA_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.UninstallFromMenu("SiteA");
  helper_.CheckAppNotInList("SiteA");
  helper_.NavigateBrowser("SiteA");
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_30SiteA_24_12SiteA_43SiteA_15SiteA_37SiteA_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.UninstallFromMenu("SiteA");
  helper_.CheckAppNotInList("SiteA");
  helper_.NavigateBrowser("SiteA");
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_30SiteC_12SiteC_43SiteC_15SiteA) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteC");
  helper_.CheckAppInListWindowed("SiteC");
  helper_.UninstallFromMenu("SiteC");
  helper_.CheckAppNotInList("SiteA");
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_30SiteA_24_12SiteA_98SiteA_15SiteA_37SiteA_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.UninstallFromAppSettings("SiteA");
  helper_.CheckAppNotInList("SiteA");
  helper_.NavigateBrowser("SiteA");
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_31SiteA_24_12SiteA_98SiteA_15SiteA_37SiteA_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.UninstallFromAppSettings("SiteA");
  helper_.CheckAppNotInList("SiteA");
  helper_.NavigateBrowser("SiteA");
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_47SiteA_24_12SiteA_98SiteA_15SiteA_37SiteA_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.UninstallFromAppSettings("SiteA");
  helper_.CheckAppNotInList("SiteA");
  helper_.NavigateBrowser("SiteA");
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_30SiteC_12SiteC_98SiteC_15SiteA) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteC");
  helper_.CheckAppInListWindowed("SiteC");
  helper_.UninstallFromAppSettings("SiteC");
  helper_.CheckAppNotInList("SiteA");
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_30SiteA_24_12SiteA_32SiteA_12SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.InstallPolicyAppTabbedNoShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_30SiteA_24_12SiteA_48SiteA_12SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.InstallPolicyAppTabbedShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_30SiteA_24_12SiteA_28_70SiteA_1SiteA_94_24_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.ClosePwa();
  helper_.ManifestUpdateDisplayBrowser("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_30SiteA_24_12SiteA_28_36SiteA_1SiteA_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.ClosePwa();
  helper_.ManifestUpdateDisplayMinimal("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_30SiteA_24_12SiteA_1SiteA_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_30SiteA_24_12SiteA_50SiteA_11SiteA_1SiteA_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.SetOpenInTab("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_31SiteA_24_12SiteA_32SiteA_12SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.InstallPolicyAppTabbedNoShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_31SiteA_24_12SiteA_48SiteA_12SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.InstallPolicyAppTabbedShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_31SiteA_24_12SiteA_28_70SiteA_1SiteA_94_24_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.ClosePwa();
  helper_.ManifestUpdateDisplayBrowser("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_31SiteA_24_12SiteA_28_36SiteA_1SiteA_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.ClosePwa();
  helper_.ManifestUpdateDisplayMinimal("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_31SiteA_24_12SiteA_1SiteA_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_31SiteA_24_12SiteA_50SiteA_11SiteA_1SiteA_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.SetOpenInTab("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_47SiteA_24_12SiteA_32SiteA_12SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.InstallPolicyAppTabbedNoShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_47SiteA_24_12SiteA_48SiteA_12SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.InstallPolicyAppTabbedShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_47SiteA_24_12SiteA_28_70SiteA_1SiteA_94_24_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.ClosePwa();
  helper_.ManifestUpdateDisplayBrowser("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_47SiteA_24_12SiteA_28_36SiteA_1SiteA_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.ClosePwa();
  helper_.ManifestUpdateDisplayMinimal("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_47SiteA_24_12SiteA_1SiteA_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_47SiteA_24_12SiteA_50SiteA_11SiteA_1SiteA_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed("SiteA");
  helper_.SetOpenInTab("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_29SiteA_11SiteA_33SiteA_11SiteA_1SiteA_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutTabbed("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.InstallPolicyAppWindowedNoShortcut("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_29SiteA_11SiteA_49SiteA_11SiteA_1SiteA_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutTabbed("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.InstallPolicyAppWindowedShortcut("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegrationBrowserTestMacWinLinux,
    WebAppIntegration_29SiteA_11SiteA_51SiteA_12SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutTabbed("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.SetOpenInWindow("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_33SiteA_12SiteA_1SiteA_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedNoShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_33SiteA_12SiteA_50SiteA_1SiteA_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedNoShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.SetOpenInTab("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_49SiteA_12SiteA_1SiteA_24_26) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_49SiteA_12SiteA_50SiteA_1SiteA_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedShortcut("SiteA");
  helper_.CheckAppInListWindowed("SiteA");
  helper_.SetOpenInTab("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_32SiteA_11SiteA_51SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppTabbedNoShortcut("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.SetOpenInWindow("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_48SiteA_11SiteA_51SiteA_1SiteA_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppTabbedShortcut("SiteA");
  helper_.CheckAppInListTabbed("SiteA");
  helper_.SetOpenInWindow("SiteA");
  helper_.LaunchFromPlatformShortcut("SiteA");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_29SiteC_11SiteC_1SiteC_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutTabbed("SiteC");
  helper_.CheckAppInListTabbed("SiteC");
  helper_.LaunchFromPlatformShortcut("SiteC");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_30SiteC_12SiteC_1SiteC_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteC");
  helper_.CheckAppInListWindowed("SiteC");
  helper_.LaunchFromPlatformShortcut("SiteC");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_33SiteC_12SiteC_1SiteC_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedNoShortcut("SiteC");
  helper_.CheckAppInListWindowed("SiteC");
  helper_.LaunchFromPlatformShortcut("SiteC");
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_32SiteC_11SiteC_1SiteC_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppTabbedNoShortcut("SiteC");
  helper_.CheckAppInListTabbed("SiteC");
  helper_.LaunchFromPlatformShortcut("SiteC");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_48SiteC_11SiteC_1SiteC_22) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppTabbedShortcut("SiteC");
  helper_.CheckAppInListTabbed("SiteC");
  helper_.LaunchFromPlatformShortcut("SiteC");
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_30SiteB_1SiteB_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallCreateShortcutWindowed("SiteB");
  helper_.LaunchFromPlatformShortcut("SiteB");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_31SiteB_1SiteB_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon("SiteB");
  helper_.LaunchFromPlatformShortcut("SiteB");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_33SiteB_1SiteB_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedNoShortcut("SiteB");
  helper_.LaunchFromPlatformShortcut("SiteB");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_49SiteB_1SiteB_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedShortcut("SiteB");
  helper_.LaunchFromPlatformShortcut("SiteB");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_47SiteB_1SiteB_25) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption("SiteB");
  helper_.LaunchFromPlatformShortcut("SiteB");
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegrationBrowserTestMacWinLinux,
                       WebAppIntegration_49SiteC_12SiteC_1SiteC_24) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyAppWindowedShortcut("SiteC");
  helper_.CheckAppInListWindowed("SiteC");
  helper_.LaunchFromPlatformShortcut("SiteC");
  helper_.CheckWindowCreated();
}

}  // namespace
}  // namespace web_app
