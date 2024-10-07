// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_test_base.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class ExtensionsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ExtensionsBrowserTest() {
    set_test_loader_host(chrome::kChromeUIExtensionsHost);
  }
};

using CrExtensionsTest = ExtensionsBrowserTest;

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, ActivityLog) {
  RunTest("extensions/activity_log_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, ActivityLogHistory) {
  RunTest("extensions/activity_log_history_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, ActivityLogHistoryItem) {
  RunTest("extensions/activity_log_history_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, ActivityLogStream) {
  RunTest("extensions/activity_log_stream_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, ActivityLogStreamItem) {
  RunTest("extensions/activity_log_stream_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, ToggleRow) {
  RunTest("extensions/toggle_row_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, RuntimeHostsDialog) {
  RunTest("extensions/runtime_hosts_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, RuntimeHostPermissions) {
  RunTest("extensions/runtime_host_permissions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, HostPermissionsToggleList) {
  RunTest("extensions/host_permissions_toggle_list_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_MAC)
#define MAYBE(test) DISABLED_##test
#else
#define MAYBE(test) test
#endif

IN_PROC_BROWSER_TEST_F(CrExtensionsTest,
                       MAYBE(ExtensionsMV2DeprecationPanelWarningStage)) {
  RunTest("extensions/mv2_deprecation_panel_warning_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest,
                       MAYBE(ExtensionsMV2DeprecationPanelDisabledStage)) {
  RunTest("extensions/mv2_deprecation_panel_disabled_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest,
                       MAYBE(ExtensionsMV2DeprecationPanelUnsupportedStage)) {
  RunTest("extensions/mv2_deprecation_panel_unsupported_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, SafetyCheckReviewPanel) {
  RunTest("extensions/review_panel_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, SitePermissions) {
  RunTest("extensions/site_permissions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, SitePermissionsBySite) {
  RunTest("extensions/site_permissions_by_site_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, SitePermissionsEditUrlDialog) {
  RunTest("extensions/site_permissions_edit_url_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, SitePermissionsList) {
  RunTest("extensions/site_permissions_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, UrlUtil) {
  RunTest("extensions/url_util_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, SitePermissionsEditPermissionsDialog) {
  RunTest("extensions/site_permissions_edit_permissions_dialog_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsTest, SitePermissionsSiteGroup) {
  RunTest("extensions/site_permissions_site_group_test.js", "mocha.run()");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Sidebar Tests

class CrExtensionsSidebarTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/sidebar_test.js",
        base::StringPrintf("runMochaTest('ExtensionSidebarTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsSidebarTest, HrefVerification) {
  RunTestCase("HrefVerification");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsSidebarTest, LayoutAndClickHandlers) {
  RunTestCase("LayoutAndClickHandlers");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsSidebarTest, SetSelected) {
  RunTestCase("SetSelected");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Toolbar Tests

class CrExtensionsToolbarTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/toolbar_test.js",
        base::StringPrintf("runMochaTest('ExtensionToolbarTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, DevModeToggle) {
  RunTestCase("DevModeToggle");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, FailedUpdateFiresLoadError) {
  RunTestCase("FailedUpdateFiresLoadError");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, NarrowModeShowsMenu) {
  RunTestCase("NarrowModeShowsMenu");
}

// TODO(crbug.com/40592901) Disabled on other platforms but MacOS due to
// timeouts.
#if !BUILDFLAG(IS_MAC)
#define MAYBE_ClickHandlers DISABLED_ClickHandlers
#else
#define MAYBE_ClickHandlers ClickHandlers
#endif
IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, MAYBE_ClickHandlers) {
  RunTestCase("ClickHandlers");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Item Tests

class CrExtensionsItemsTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/item_test.js",
        base::StringPrintf("runMochaTest('ExtensionItemTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, NormalState) {
  RunTestCase("ElementVisibilityNormalState");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, DeveloperState) {
  RunTestCase("ElementVisibilityDeveloperState");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, ClickableItems) {
  RunTestCase("ClickableItems");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, FailedReloadFiresLoadError) {
  RunTestCase("FailedReloadFiresLoadError");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, Warnings) {
  RunTestCase("Warnings");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, SourceIndicator) {
  RunTestCase("SourceIndicator");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, EnableToggle) {
  RunTestCase("EnableToggle");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, RemoveButton) {
  RunTestCase("RemoveButton");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, HtmlInName) {
  RunTestCase("HtmlInName");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, RepairButton) {
  RunTestCase("RepairButton");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, InspectableViewSortOrder) {
  RunTestCase("InspectableViewSortOrder");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, EnableExtensionToggleTooltips) {
  RunTestCase("EnableExtensionToggleTooltips");
}

class CrExtensionsDetailViewTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/detail_view_test.js",
        base::StringPrintf("runMochaTest('ExtensionDetailViewTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, LayoutSource) {
  RunTestCase("LayoutSource");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       ElementVisibilityReloadButton) {
  RunTestCase("ElementVisibilityReloadButton");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, FailedReloadFiresLoadError) {
  RunTestCase("FailedReloadFiresLoadError");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       SupervisedUserDisableReasons) {
  RunTestCase("SupervisedUserDisableReasons");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       MV2DeprecationDisabledExtension) {
  RunTestCase("MV2DeprecationDisabledExtension");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, ClickableElements) {
  RunTestCase("ClickableElements");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, Indicator) {
  RunTestCase("Indicator");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, Warnings) {
  RunTestCase("Warnings");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       NoSiteAccessWithEnhancedSiteControls) {
  RunTestCase("NoSiteAccessWithEnhancedSiteControls");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, InspectableViewSortOrder) {
  RunTestCase("InspectableViewSortOrder");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       ShowAccessRequestsInToolbar) {
  RunTestCase("ShowAccessRequestsInToolbar");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, SafetyCheckWarning) {
  RunTestCase("SafetyCheckWarning");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, Mv2DeprecationMessage_None) {
  RunTestCase("Mv2DeprecationMessage_None");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       Mv2DeprecationMessage_Warning) {
  RunTestCase("Mv2DeprecationMessage_Warning");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       Mv2DeprecationMessage_DisableWithReEnable_Visbility) {
  RunTestCase("Mv2DeprecationMessage_DisableWithReEnable_Visbility");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       Mv2DeprecationMessage_DisableWithReEnable) {
  RunTestCase("Mv2DeprecationMessage_DisableWithReEnable_Content");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       Mv2DeprecationMessage_Unsupported_Visbility) {
  RunTestCase("Mv2DeprecationMessage_Unsupported_Visbility");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       Mv2DeprecationMessage_Unsupported) {
  RunTestCase("Mv2DeprecationMessage_Unsupported_Content");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, PinnedToToolbar) {
  RunTestCase("PinnedToToolbar");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Item List Tests

class CrExtensionsItemListTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/item_list_test.js",
        base::StringPrintf("runMochaTest('ExtensionItemListTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, Filtering) {
  RunTestCase("Filtering");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, NoItems) {
  RunTestCase("NoItems");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, NoSearchResults) {
  RunTestCase("NoSearchResults");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, SectionsVisibility) {
  RunTestCase("SectionsVisibility");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, LoadTimeData) {
  RunTestCase("LoadTimeData");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, SafetyCheckPanel_Disabled) {
  RunTestCase("SafetyCheckPanel_Disabled");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest,
                       SafetyCheckPanel_EnabledSafetyCheck) {
  RunTestCase("SafetyCheckPanel_EnabledSafetyCheck");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest,
                       SafetyCheckPanel_EnabledSafetyHub) {
  RunTestCase("SafetyCheckPanel_EnabledSafetyHub");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest,
                       ManifestV2DeprecationPanel_None) {
  RunTestCase("ManifestV2DeprecationPanel_None");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest,
                       ManifestV2DeprecationPanel_Warning) {
  RunTestCase("ManifestV2DeprecationPanel_Warning");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest,
                       ManifestV2DeprecationPanel_DisableWithReEnable) {
  RunTestCase("ManifestV2DeprecationPanel_DisableWithReEnable");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest,
                       ManifestV2DeprecationPanel_Unsupported) {
  RunTestCase("ManifestV2DeprecationPanel_Unsupported");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest,
                       ManifestV2DeprecationPanel_TitleVisibility) {
  RunTestCase("ManifestV2DeprecationPanel_TitleVisibility");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Load Error Tests

class CrExtensionsLoadErrorTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/load_error_test.js",
        base::StringPrintf("runMochaTest('ExtensionLoadErrorTests', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsLoadErrorTest, RetryError) {
  RunTestCase("RetryError");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsLoadErrorTest, RetrySuccess) {
  RunTestCase("RetrySuccess");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsLoadErrorTest, CodeSection) {
  RunTestCase("CodeSection");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsLoadErrorTest, PathWithoutSource) {
  RunTestCase("PathWithoutSource");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsLoadErrorTest, GenericError) {
  RunTestCase("GenericError");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Manager Tests

class CrExtensionsManagerUnitTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/manager_unit_test.js",
        base::StringPrintf("runMochaTest('ExtensionManagerUnitTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, ItemOrder) {
  RunTestCase("ItemOrder");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, SetItemData) {
  RunTestCase("SetItemData");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, UpdateItemData) {
  RunTestCase("UpdateItemData");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, ProfileSettings) {
  RunTestCase("ProfileSettings");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, Uninstall) {
  RunTestCase("Uninstall");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, UninstallFocus) {
  RunTestCase("UninstallFocus");
}

// Flaky since r621915: https://crbug.com/922490
IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest,
                       DISABLED_UninstallFromDetails) {
  RunTestCase("UninstallFromDetails");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, ToggleIncognito) {
  RunTestCase("ToggleIncognito");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTest, EnableAndDisable) {
  RunTestCase("EnableAndDisable");
}

class CrExtensionsManagerTestWithMultipleExtensionTypesInstalled
    : public ExtensionSettingsTestBase {
 protected:
  CrExtensionsManagerTestWithMultipleExtensionTypesInstalled() {
    set_test_loader_host(chrome::kChromeUIExtensionsHost);
  }

  void RunTestCase(const std::string& testCase) {
    ExtensionSettingsTestBase::RunTest(
        "extensions/manager_test.js",
        base::StringPrintf("runMochaTest('ExtensionManagerTest', '%s');",
                           testCase.c_str()));
  }

  void InstallPrerequisites() {
    InstallGoodExtension();
    InstallPackagedApp();
    InstallHostedApp();
    InstallPlatformApp();
  }
};

IN_PROC_BROWSER_TEST_F(
    CrExtensionsManagerTestWithMultipleExtensionTypesInstalled,
    ItemListVisibility) {
  InstallPrerequisites();
  RunTestCase("ItemListVisibility");
}

IN_PROC_BROWSER_TEST_F(
    CrExtensionsManagerTestWithMultipleExtensionTypesInstalled,
    SplitItems) {
  InstallPrerequisites();
  RunTestCase("SplitItems");
}

IN_PROC_BROWSER_TEST_F(
    CrExtensionsManagerTestWithMultipleExtensionTypesInstalled,
    ChangePages) {
  InstallPrerequisites();
  RunTestCase("ChangePages");
}

IN_PROC_BROWSER_TEST_F(
    CrExtensionsManagerTestWithMultipleExtensionTypesInstalled,
    CloseDrawerOnNarrowModeExit) {
  InstallPrerequisites();
  RunTestCase("CloseDrawerOnNarrowModeExit");
}

IN_PROC_BROWSER_TEST_F(
    CrExtensionsManagerTestWithMultipleExtensionTypesInstalled,
    PageTitleUpdate) {
  InstallPrerequisites();
  RunTestCase("PageTitleUpdate");
}

IN_PROC_BROWSER_TEST_F(
    CrExtensionsManagerTestWithMultipleExtensionTypesInstalled,
    NavigateToSitePermissionsFail) {
  InstallPrerequisites();
  RunTestCase("NavigateToSitePermissionsFail");
}

IN_PROC_BROWSER_TEST_F(
    CrExtensionsManagerTestWithMultipleExtensionTypesInstalled,
    NavigateToSitePermissionsSuccess) {
  InstallPrerequisites();
  RunTestCase("NavigateToSitePermissionsSuccess");
}

class CrExtensionsManagerTestWithIdQueryParam
    : public ExtensionSettingsTestBase {
 protected:
  CrExtensionsManagerTestWithIdQueryParam() {
    set_test_loader_host(chrome::kChromeUIExtensionsHost);
  }

  void RunTestCase(const std::string& testCase) {
    ExtensionSettingsTestBase::RunTest(
        "extensions/manager_test_with_id_query_param.js",
        base::StringPrintf("runMochaTest('ExtensionManagerTest', '%s');",
                           testCase.c_str()));
  }

  void InstallPrerequisites() {
    InstallGoodExtension();
    SetAutoConfirmUninstall();
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerTestWithIdQueryParam,
                       UrlNavigationToDetails) {
  InstallPrerequisites();
  RunTestCase("UrlNavigationToDetails");
}

// Disabled as flaky. TODO(crbug.com/40719203): Enable this test.
IN_PROC_BROWSER_TEST_F(CrExtensionsManagerTestWithIdQueryParam,
                       DISABLED_UrlNavigationToActivityLogFail) {
  InstallPrerequisites();
  RunTestCase("UrlNavigationToActivityLogFail");
}

class CrExtensionsManagerUnitTestWithActivityLogFlag
    : public ExtensionsBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerUnitTestWithActivityLogFlag, All) {
  RunTest("extensions/manager_unit_test_with_activity_log_flag.js",
          "mocha.run()");
}

class CrExtensionsManagerTestWithActivityLogFlag
    : public ExtensionsBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsManagerTestWithActivityLogFlag, All) {
  RunTest("extensions/manager_test_with_activity_log_flag.js", "mocha.run()");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Options Dialog Tests

class CrExtensionsOptionsDialogTest : public ExtensionSettingsTestBase {
 protected:
  CrExtensionsOptionsDialogTest() {
    set_test_loader_host(chrome::kChromeUIExtensionsHost);
  }
};

// TODO(crbug.com/40109111): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(CrExtensionsOptionsDialogTest, DISABLED_Layout) {
  InstallExtensionWithInPageOptions();
  RunTest("extensions/options_dialog_test.js",
          "runMochaTest('ExtensionOptionsDialogTests', 'Layout')");
}

////////////////////////////////////////////////////////////////////////////////
// Error Console tests

class CrExtensionsErrorConsoleTest : public ExtensionSettingsTestBase {
 protected:
  CrExtensionsErrorConsoleTest() {
    set_test_loader_host(chrome::kChromeUIExtensionsHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsErrorConsoleTest, TestUpDownErrors) {
  SetDevModeEnabled(true);
  // TODO(crbug.com/40804030): Update the associated extensions to
  // Manifest V3 and stop ignoring deprecated manifest version warnings.
  SetSilenceDeprecatedManifestVersionWarnings(true);
  InstallErrorsExtension();

  RunTest("extensions/error_console_test.js", "mocha.run()");

  // Return settings to default.
  SetDevModeEnabled(false);
  SetSilenceDeprecatedManifestVersionWarnings(false);
}

////////////////////////////////////////////////////////////////////////////////
// Extension Keyboard Shortcuts Tests

class CrExtensionsShortcutTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/keyboard_shortcuts_test.js",
        base::StringPrintf("runMochaTest('ExtensionShortcutTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsShortcutTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsShortcutTest, IsValidKeyCode) {
  RunTestCase("IsValidKeyCode");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsShortcutTest, KeyStrokeToString) {
  RunTestCase("KeyStrokeToString");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsShortcutTest, ScopeChange) {
  RunTestCase("ScopeChange");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Pack Dialog Tests

class CrExtensionsPackDialogTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/pack_dialog_test.js",
        base::StringPrintf("runMochaTest('ExtensionPackDialogTests', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsPackDialogTest, Interaction) {
  RunTestCase("Interaction");
}

// Disabling on Windows due to flaky timeout on some build bots.
// http://crbug.com/832885
#if BUILDFLAG(IS_WIN)
#define MAYBE_PackSuccess DISABLED_PackSuccess
#else
#define MAYBE_PackSuccess PackSuccess
#endif
IN_PROC_BROWSER_TEST_F(CrExtensionsPackDialogTest, MAYBE_PackSuccess) {
  RunTestCase("PackSuccess");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsPackDialogTest, PackError) {
  RunTestCase("PackError");
}

// Temporarily disabling on Mac due to flakiness.
// http://crbug.com/877109
#if BUILDFLAG(IS_MAC)
#define MAYBE_PackWarning DISABLED_PackWarning
#else
#define MAYBE_PackWarning PackWarning
#endif
IN_PROC_BROWSER_TEST_F(CrExtensionsPackDialogTest, MAYBE_PackWarning) {
  RunTestCase("PackWarning");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Error Page Tests

class CrExtensionsErrorPageTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/error_page_test.js",
        base::StringPrintf("runMochaTest('ExtensionErrorPageTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsErrorPageTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsErrorPageTest, CodeSection) {
  RunTestCase("CodeSection");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsErrorPageTest, ErrorSelection) {
  RunTestCase("ErrorSelection");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsErrorPageTest, InvalidUrl) {
  RunTestCase("InvalidUrl");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsErrorPageTest, ReloadItem) {
  RunTestCase("ReloadItem");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Code Section Tests

class CrExtensionsCodeSectionTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/code_section_test.js",
        base::StringPrintf("runMochaTest('ExtensionCodeSectionTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsCodeSectionTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsCodeSectionTest, LongSource) {
  RunTestCase("LongSource");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Navigation Helper Tests

class CrExtensionsNavigationHelperTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/navigation_helper_test.js",
        base::StringPrintf(
            "runMochaTest('ExtensionNavigationHelperTest', '%s');",
            testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsNavigationHelperTest, Basic) {
  RunTestCase("Basic");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsNavigationHelperTest, Conversions) {
  RunTestCase("Conversions");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsNavigationHelperTest, PushAndReplaceState) {
  RunTestCase("PushAndReplaceState");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsNavigationHelperTest, SupportedRoutes) {
  RunTestCase("SupportedRoutes");
}
