// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"
#include "content/public/test/browser_test.h"

namespace web_app::integration_tests {
namespace {

using WebAppIntegration = WebAppIntegrationTest;

// Manual tests:

IN_PROC_BROWSER_TEST_F(WebAppIntegrationTest, LaunchFromPlatformShortcut) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.ClosePwa();
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckPlatformShortcutAndIcon) {
  helper_.CheckPlatformShortcutNotExists(Site::kStandalone);
  helper_.CheckPlatformShortcutNotExists(Site::kMinimalUi);
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckPlatformShortcutNotExists(Site::kMinimalUi);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckPolicyAppUninstallWorks) {
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.UninstallPolicyApp(Site::kStandalone);
  helper_.CheckPlatformShortcutNotExists(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckDeletePlatformShortcut) {
  helper_.DeletePlatformShortcut(Site::kStandalone);
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.DeletePlatformShortcut(Site::kStandalone);
  helper_.CheckPlatformShortcutNotExists(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckCreateShortcuts) {
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.DeletePlatformShortcut(Site::kStandalone);
  helper_.CheckPlatformShortcutNotExists(Site::kStandalone);
  helper_.CreateShortcutsFromList(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
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

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckLaunchFileExpectDialog) {
  helper_.InstallMenuOption(InstallableSite::kFileHandler);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckLaunchFileExpectNoDialog_Allow) {
  helper_.InstallOmniboxIcon(InstallableSite::kFileHandler);
  // Open the file and set AskAgainOption to kRemember.
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kRemember);
  helper_.CheckWindowCreated();
  // Open the file again.
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckLaunchFileExpectNoDialog_Deny) {
  helper_.InstallOmniboxIcon(InstallableSite::kFileHandler);
  // Open the file and set AskAgainOption to kRemember.
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kRemember);
  // No new window is created when denied.
  helper_.CheckWindowNotCreated();
  // Open the file again.
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
  // Despite previous denial, a new window should still have been created. The
  // only difference with the Allow case is that no files would have been passed
  // to the launched app.
  helper_.CheckWindowCreated();
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

IN_PROC_BROWSER_TEST_F(WebAppIntegration, MultiProfileFileHandling) {
  // Install file handling PWA in two profiles.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kBar);

  helper_.SwitchActiveProfile(ProfileName::kProfile2);
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kBar);

  // Disabling file handling in one profile should not disable it in the other.
  helper_.DisableFileHandling(Site::kMinimalUi);
  helper_.SwitchActiveProfile(ProfileName::kDefault);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kMinimalUi, FileExtension::kBar);

  // Disabling in both should disable file handling.
  helper_.DisableFileHandling(Site::kMinimalUi);
  helper_.CheckSiteNotHandlesFile(Site::kMinimalUi, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kMinimalUi, FileExtension::kBar);
}

// TODO(https://crbug.com/1382214): Migrate this to a CUJ.
IN_PROC_BROWSER_TEST_F(WebAppIntegration, MultiProfileFileLaunch) {
  // Install file handling PWA in two profiles.
  helper_.InstallMenuOption(InstallableSite::kFileHandler);
  helper_.ClosePwa();

  helper_.SwitchActiveProfile(ProfileName::kProfile2);
  helper_.InstallMenuOption(InstallableSite::kFileHandler);
  helper_.ClosePwa();
  helper_.DisableFileHandling(Site::kFileHandler);

  // File handling is disabled in second profile, launching should open
  // in first profile.
  helper_.SwitchActiveProfile(ProfileName::kDefault);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowCreated();
}

// TODO(https://crbug.com/1382214): Migrate this to a CUJ.
IN_PROC_BROWSER_TEST_F(WebAppIntegration, MultiProfileFileLaunchWhileOpen) {
  // Install file handling PWA in two profiles.
  helper_.InstallMenuOption(InstallableSite::kFileHandler);
  helper_.ClosePwa();

  helper_.SwitchActiveProfile(ProfileName::kProfile2);
  helper_.InstallMenuOption(InstallableSite::kFileHandler);
  helper_.DisableFileHandling(Site::kFileHandler);

  // File handling is disabled in second profile, launching should open
  // in first profile, even if it wasn't closed.
  helper_.SwitchActiveProfile(ProfileName::kDefault);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, CheckFilesLoadedInSite) {
  helper_.InstallMenuOption(InstallableSite::kFileHandler);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneFooFile);
}

// Generated tests:

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableBrowser_11NotPromotable_7NotPromotable_1NotPromotable_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromPlatformShortcut(Site::kNotPromotable);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableWindowed_12NotPromotable_7NotPromotable_43NotPromotable_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.UninstallFromMenu(Site::kNotPromotable);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29NotPromotableWindowed_12NotPromotable_7NotPromotable_98NotPromotable_15Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kNotPromotable, WindowOptions::kWindowed);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.UninstallFromAppSettings(Site::kNotPromotable);
  helper_.CheckAppNotInList(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_29MinimalUiWindowed_1MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_31MinimalUi_1MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_47MinimalUi_1MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_43Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromMenu(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_98Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromAppSettings(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_96Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenAppSettingsFromChromeApps(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_97Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenAppSettingsFromAppMenu(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_43Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromMenu(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_98Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromAppSettings(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_96Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenAppSettingsFromChromeApps(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_97Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenAppSettingsFromAppMenu(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_43Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromMenu(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_98Standalone_15Standalone_37Standalone_18_19) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.UninstallFromAppSettings(Site::kStandalone);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconShown();
  helper_.CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_96Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenAppSettingsFromChromeApps(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_97Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.OpenAppSettingsFromAppMenu(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29WcoWindowed_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kWco, WindowOptions::kWindowed);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_74Standalone_72Standalone_1Standalone_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.DeletePlatformShortcut(Site::kStandalone);
  helper_.CreateShortcutsFromList(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneBrowser_117Standalone_1Standalone_94_24_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kBrowser);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneWindowed_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneMinimalUi_117Standalone_1Standalone_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kWindowed);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kMinimalUi);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneBrowser_117Standalone_1Standalone_94_24_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kBrowser);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneMinimalUi_117Standalone_1Standalone_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kMinimalUi);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneBrowser_117Standalone_1Standalone_94_24_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kBrowser);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabNotCreated();
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Standalone_24_12Standalone_7Standalone_112StandaloneNotShown_116StandaloneMinimalUi_117Standalone_1Standalone_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone,
                                           IsShown::kNotShown);
  helper_.ManifestUpdateDisplay(Site::kStandalone, Display::kMinimalUi);
  helper_.AwaitManifestUpdate(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_1StandaloneNestedA_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneNestedAWindowed_8StandaloneNestedAStandalone_117StandaloneNestedA_1StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandaloneNestedA, WindowOptions::kWindowed);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_1StandaloneNestedA_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_1StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_1StandaloneNestedA_37Standalone_17_20) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNestedA);
  helper_.NavigateBrowser(Site::kStandalone);
  helper_.CheckInstallIconNotShown();
  helper_.CheckLaunchIconShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47StandaloneNestedA_8StandaloneNestedAStandalone_117StandaloneNestedA_1StandaloneNestedA_39StandaloneNestedAStandaloneNestedB_21) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kStandaloneNestedA);
  helper_.ManifestUpdateScopeTo(Site::kStandaloneNestedA, Site::kStandalone);
  helper_.AwaitManifestUpdate(Site::kStandaloneNestedA);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNestedA);
  helper_.NavigatePwa(Site::kStandaloneNestedA, Site::kStandaloneNestedB);
  helper_.CheckNoToolbar();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29WcoWindowed_112WcoShown_116WcoStandalone_117Wco_1Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kWco, WindowOptions::kWindowed);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31Wco_112WcoShown_116WcoStandalone_117Wco_1Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47Wco_112WcoShown_116WcoStandalone_117Wco_1Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29MinimalUiWindowed_116MinimalUiWco_117MinimalUi_1MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kMinimalUi, WindowOptions::kWindowed);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_31MinimalUi_116MinimalUiWco_117MinimalUi_1MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallOmniboxIcon(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_47MinimalUi_116MinimalUiWco_117MinimalUi_1MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webaps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallMenuOption(InstallableSite::kMinimalUi);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_11Standalone_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_11Standalone_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_11Standalone_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_11Standalone_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_11Standalone_72Standalone_7Standalone_1Standalone_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CreateShortcutsFromList(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_32StandaloneWithShortcutWindowedWebApp_7Standalone_11Standalone_1Standalone_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_32StandaloneNoShortcutWindowedWebApp_7Standalone_11Standalone_1Standalone_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_11Standalone_7Standalone_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_11Standalone_7Standalone_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_11Standalone_7Standalone_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_11Standalone_7Standalone_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableNoShortcutBrowserWebApp_11NotPromotable_1NotPromotable_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kNotPromotable);
  helper_.LaunchFromPlatformShortcut(Site::kNotPromotable);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableWithShortcutBrowserWebApp_11NotPromotable_7NotPromotable_1NotPromotable_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromPlatformShortcut(Site::kNotPromotable);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_1MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiWithShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_1MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration,
                       WAI_32MinimalUiNoShortcutWindowedWebApp_1MinimalUi_25) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowDisplayMinimal();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32MinimalUiNoShortcutWindowedWebApp_116MinimalUiWco_117MinimalUi_1MinimalUi_112MinimalUiShown_114MinimalUi_113MinimalUiOn_112MinimalUiShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kMinimalUi, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kMinimalUi, Display::kWco);
  helper_.AwaitManifestUpdate(Site::kMinimalUi);
  helper_.LaunchFromPlatformShortcut(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kMinimalUi);
  helper_.CheckWindowControlsOverlay(Site::kMinimalUi, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kMinimalUi, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebShortcut_15Standalone_75StandaloneNotStartUrl_1StandaloneNotStartUrl_133StandaloneNotStartUrl) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebShortcut);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandaloneNotStartUrl);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNotStartUrl);
  helper_.CheckAppNavigation(Site::kStandaloneNotStartUrl);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutWindowedWebApp_15StandaloneNotStartUrl_75Standalone_1Standalone_133Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppNotInList(Site::kStandaloneNotStartUrl);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckAppNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutBrowserWebApp_15StandaloneNotStartUrl_75Standalone_1Standalone_134Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppNotInList(Site::kStandaloneNotStartUrl);
  helper_.CheckAppInListIconCorrect(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckBrowserNavigation(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNotStartUrlWithShortcutBrowserWebShortcut_15Standalone_75StandaloneNotStartUrl_1StandaloneNotStartUrl_134StandaloneNotStartUrl) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandaloneNotStartUrl,
                           ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebShortcut);
  helper_.CheckAppNotInList(Site::kStandalone);
  helper_.CheckAppInListIconCorrect(Site::kStandaloneNotStartUrl);
  helper_.LaunchFromPlatformShortcut(Site::kStandaloneNotStartUrl);
  helper_.CheckBrowserNavigation(Site::kStandaloneNotStartUrl);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_72Standalone_7Standalone_1Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CreateShortcutsFromList(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_96Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.OpenAppSettingsFromChromeApps(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_97Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.OpenAppSettingsFromAppMenu(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutBrowserWebApp_11Standalone_51Standalone_1Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.SetOpenInWindow(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29StandaloneBrowser_11Standalone_7Standalone_51Standalone_12Standalone_1Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kStandalone, WindowOptions::kBrowser);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindow(Site::kStandalone);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_101Standalone_111Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_104Standalone_106Standalone_101Standalone_107Standalone_103Standalone_106Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyBlocked(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_104Standalone_106Standalone_105Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.EnableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.DisableRunOnOsLogin(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_102Standalone_106Standalone_111Standalone_103Standalone_107Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.ApplyRunOnOsLoginPolicyRunWindowed(Site::kStandalone);
  helper_.CheckRunOnOsLoginEnabled(Site::kStandalone);
  helper_.CheckUserCannotSetRunOnOsLogin(Site::kStandalone);
  helper_.RemoveRunOnOsLoginPolicy(Site::kStandalone);
  helper_.CheckRunOnOsLoginDisabled(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_96Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.OpenAppSettingsFromChromeApps(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_97Standalone_109Standalone) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.OpenAppSettingsFromAppMenu(Site::kStandalone);
  helper_.CheckBrowserNavigationIsAppSettings(Site::kStandalone);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutBrowserWebApp_11Standalone_7Standalone_51Standalone_1Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckAppInListTabbed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInWindow(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableNoShortcutWindowedWebApp_12NotPromotable_1NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.LaunchFromPlatformShortcut(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32NotPromotableWithShortcutWindowedWebApp_12NotPromotable_7NotPromotable_1NotPromotable_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kNotPromotable, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kNotPromotable);
  helper_.CheckPlatformShortcutAndIcon(Site::kNotPromotable);
  helper_.LaunchFromPlatformShortcut(Site::kNotPromotable);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoWithShortcutWindowedWebApp_116WcoStandalone_117Wco_1Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32WcoNoShortcutWindowedWebApp_116WcoStandalone_117Wco_1Wco_112WcoNotShown_113WcoOff) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.ManifestUpdateDisplay(Site::kWco, Display::kStandalone);
  helper_.AwaitManifestUpdate(Site::kWco);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kNotShown);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_50Standalone_1Standalone_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.SetOpenInTab(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_50Standalone_1Standalone_22) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.SetOpenInTab(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckTabCreated();
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_74Standalone_72Standalone_1Standalone_24) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.DeletePlatformShortcut(Site::kStandalone);
  helper_.CreateShortcutsFromList(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32StandaloneNoShortcutWindowedWebApp_12Standalone_1Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone, IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32StandaloneWithShortcutWindowedWebApp_12Standalone_7Standalone_1Standalone_24_26_112StandaloneNotShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kStandalone, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckAppInListWindowed(Site::kStandalone);
  helper_.CheckPlatformShortcutAndIcon(Site::kStandalone);
  helper_.LaunchFromPlatformShortcut(Site::kStandalone);
  helper_.CheckWindowCreated();
  helper_.CheckWindowDisplayStandalone();
  helper_.CheckWindowControlsOverlayToggle(Site::kStandalone, IsShown::kNotShown);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_69Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_35Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_34Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_115Wco_113WcoOff_112WcoShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.DisableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_69Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_35Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_34Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoWithShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kWithShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_69Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_35Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_34Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_115Wco_113WcoOff_112WcoShown) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.DisableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOff);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_69Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromMenuOption(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_35Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromLaunchIcon(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_34Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromChromeApps(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(WebAppIntegration, WAI_32WcoNoShortcutWindowedWebApp_1Wco_112WcoShown_114Wco_113WcoOn_112WcoShown_1Wco_113WcoOn) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kWco, ShortcutOptions::kNoShortcut, WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.EnableWindowControlsOverlay(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
  helper_.CheckWindowControlsOverlayToggle(Site::kWco, IsShown::kShown);
  helper_.LaunchFromPlatformShortcut(Site::kWco);
  helper_.CheckWindowControlsOverlay(Site::kWco, IsOn::kOn);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerWindowed_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowRemember_121FileHandlerOneFooFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kWindowed);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kRemember);
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerWindowed_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyAskAgain_127_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kWindowed);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerWindowed_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyRemember_127_122FileHandlerFoo_122FileHandlerBar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kWindowed);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kRemember);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerBrowser_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowRemember_121FileHandlerOneFooFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kBrowser);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kRemember);
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerBrowser_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyAskAgain_127_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kBrowser);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerBrowser_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyRemember_127_122FileHandlerFoo_122FileHandlerBar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kBrowser);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kRemember);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowRemember_121FileHandlerOneFooFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kRemember);
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyAskAgain_127_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyRemember_127_122FileHandlerFoo_122FileHandlerBar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kRemember);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowRemember_121FileHandlerOneFooFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kRemember);
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyAskAgain_127_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyRemember_127_122FileHandlerFoo_122FileHandlerBar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kRemember);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowRemember_121FileHandlerOneFooFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kRemember);
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyAskAgain_127_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyRemember_127_122FileHandlerFoo_122FileHandlerBar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kRemember);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowRemember_121FileHandlerOneFooFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kRemember);
  helper_.LaunchFileExpectNoDialog(Site::kFileHandler,
                                   FilesOptions::kOneFooFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyAskAgain_127_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileDenyRemember_127_122FileHandlerFoo_122FileHandlerBar) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kDeny,
                                 AskAgainOptions::kRemember);
  helper_.CheckWindowNotCreated();
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteNotHandlesFile(Site::kFileHandler, FileExtension::kBar);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerWindowed_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneBarFileAllowAskAgain_126FileHandlerOneBarFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kWindowed);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneBarFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneBarFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerMultipleFooFilesAllowAskAgain_126FileHandlerMultipleFooFiles) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(
      Site::kFileHandler, FilesOptions::kMultipleFooFiles,
      AllowDenyOptions::kAllow, AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler,
                                 FilesOptions::kMultipleFooFiles);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerWindowed_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain_126FileHandlerOneFooFile_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kWindowed);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneFooFile);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerWindowed_118FileHandlerFoo_118FileHandlerBar_120FileHandlerMultipleFooFilesAllowAskAgain_126FileHandlerMultipleFooFiles) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kWindowed);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(
      Site::kFileHandler, FilesOptions::kMultipleFooFiles,
      AllowDenyOptions::kAllow, AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler,
                                 FilesOptions::kMultipleFooFiles);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerBrowser_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain_126FileHandlerOneFooFile_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kBrowser);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneFooFile);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerBrowser_118FileHandlerFoo_118FileHandlerBar_120FileHandlerMultipleFooFilesAllowAskAgain_126FileHandlerMultipleFooFiles) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kBrowser);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(
      Site::kFileHandler, FilesOptions::kMultipleFooFiles,
      AllowDenyOptions::kAllow, AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler,
                                 FilesOptions::kMultipleFooFiles);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_29FileHandlerBrowser_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneBarFileAllowAskAgain_126FileHandlerOneBarFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.CreateShortcut(Site::kFileHandler, WindowOptions::kBrowser);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneBarFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneBarFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain_126FileHandlerOneFooFile_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneFooFile);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerMultipleFooFilesAllowAskAgain_126FileHandlerMultipleFooFiles) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(
      Site::kFileHandler, FilesOptions::kMultipleFooFiles,
      AllowDenyOptions::kAllow, AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler,
                                 FilesOptions::kMultipleFooFiles);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneBarFileAllowAskAgain_126FileHandlerOneBarFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneBarFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneBarFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain_126FileHandlerOneFooFile_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneFooFile);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerMultipleFooFilesAllowAskAgain_126FileHandlerMultipleFooFiles) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(
      Site::kFileHandler, FilesOptions::kMultipleFooFiles,
      AllowDenyOptions::kAllow, AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler,
                                 FilesOptions::kMultipleFooFiles);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerWithShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneBarFileAllowAskAgain_126FileHandlerOneBarFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kWithShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneBarFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneBarFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain_126FileHandlerOneFooFile_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneFooFile);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutWindowedWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneBarFileAllowAskAgain_126FileHandlerOneBarFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kWindowed, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneBarFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneBarFile);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneFooFileAllowAskAgain_126FileHandlerOneFooFile_120FileHandlerOneFooFileAllowAskAgain) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneFooFile);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneFooFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerMultipleFooFilesAllowAskAgain_126FileHandlerMultipleFooFiles) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(
      Site::kFileHandler, FilesOptions::kMultipleFooFiles,
      AllowDenyOptions::kAllow, AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler,
                                 FilesOptions::kMultipleFooFiles);
}

IN_PROC_BROWSER_TEST_F(
    WebAppIntegration,
    WAI_32FileHandlerNoShortcutBrowserWebApp_118FileHandlerFoo_118FileHandlerBar_120FileHandlerOneBarFileAllowAskAgain_126FileHandlerOneBarFile) {
  // Test contents are generated by script. Please do not modify!
  // See `docs/webapps/why-is-this-test-failing.md` or
  // `docs/webapps/integration-testing-framework` for more info.
  // Sheriffs: Disabling this test is supported.
  helper_.InstallPolicyApp(Site::kFileHandler, ShortcutOptions::kNoShortcut,
                           WindowOptions::kBrowser, InstallMode::kWebApp);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kFoo);
  helper_.CheckSiteHandlesFile(Site::kFileHandler, FileExtension::kBar);
  helper_.LaunchFileExpectDialog(Site::kFileHandler, FilesOptions::kOneBarFile,
                                 AllowDenyOptions::kAllow,
                                 AskAgainOptions::kAskAgain);
  helper_.CheckFilesLoadedInSite(Site::kFileHandler, FilesOptions::kOneBarFile);
}

}  // namespace
}  // namespace web_app::integration_tests
