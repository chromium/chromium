// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "build/chromeos_buildflags.h"');

/**
 * Basic test fixture for the MD chrome://extensions page. Installs no
 * extensions.
 */
const CrExtensionsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions';
  }

  /** @override */
  get typedefCppFixture() {
    return 'ExtensionSettingsUIBrowserTest';
  }

  // The name of the mocha suite. Should be overriden by subclasses.
  get suiteName() {
    return null;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

/**
 * Test fixture with one installed extension.
 */
const CrExtensionsBrowserTestWithInstalledExtension =
    class extends CrExtensionsBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
    GEN('  SetAutoConfirmUninstall();');
  }
};

////////////////////////////////////////////////////////////////////////////////
// Extension Manager Tests

var CrExtensionsManagerUnitTestWithActivityLogFlag =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/manager_unit_test_with_activity_log_flag.js';
  }

  /** @override */
  get suiteName() {
    return extension_manager_unit_tests.suiteName;
  }

  /** @override */
  get commandLineSwitches() {
    return [{
      switchName: 'enable-extension-activity-logging',
    }];
  }
};

TEST_F(
    'CrExtensionsManagerUnitTestWithActivityLogFlag', 'UpdateFromActivityLog',
    function() {
      this.runMochaTest(
          extension_manager_unit_tests.TestNames.UpdateFromActivityLog);
    });

var CrExtensionsManagerTestWithMultipleExtensionTypesInstalled =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/manager_test.js';
  }

  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
    GEN('  InstallPackagedApp();');
    GEN('  InstallHostedApp();');
    GEN('  InstallPlatformApp();');
  }

  /** @override */
  get suiteName() {
    return extension_manager_tests.suiteName;
  }
};

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'ItemListVisibility', function() {
      this.runMochaTest(extension_manager_tests.TestNames.ItemListVisibility);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled', 'SplitItems',
    function() {
      this.runMochaTest(extension_manager_tests.TestNames.SplitItems);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled', 'ChangePages',
    function() {
      this.runMochaTest(extension_manager_tests.TestNames.ChangePages);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'CloseDrawerOnNarrowModeExit', function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.CloseDrawerOnNarrowModeExit);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'PageTitleUpdate', function() {
      this.runMochaTest(extension_manager_tests.TestNames.PageTitleUpdate);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'NavigateToSitePermissionsFail', function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.NavigateToSitePermissionsFail);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'NavigateToSitePermissionsSuccess', function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.NavigateToSitePermissionsSuccess);
    });

var CrExtensionsManagerTestWithIdQueryParam =
    class extends CrExtensionsBrowserTestWithInstalledExtension {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/manager_test_with_id_query_param.js';
  }

  /** @override */
  get suiteName() {
    return extension_manager_tests.suiteName;
  }
};

TEST_F(
    'CrExtensionsManagerTestWithIdQueryParam', 'NavigationToDetails',
    function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.UrlNavigationToDetails);
    });

// Disabled as flaky. TODO(crbug.com/1127741): Enable this test.
TEST_F(
    'CrExtensionsManagerTestWithIdQueryParam',
    'DISABLED_UrlNavigationToActivityLogFail', function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.UrlNavigationToActivityLogFail);
    });

CrExtensionsManagerTestWithActivityLogFlag =
    class extends CrExtensionsManagerTestWithIdQueryParam {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/manager_test_with_activity_log_flag.js';
  }

  /** @override */
  get commandLineSwitches() {
    return [{
      switchName: 'enable-extension-activity-logging',
    }];
  }
};

TEST_F(
    'CrExtensionsManagerTestWithActivityLogFlag',
    'UrlNavigationToActivityLogSuccess', function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.UrlNavigationToActivityLogSuccess);
    });

////////////////////////////////////////////////////////////////////////////////
// Extension Options Dialog Tests

var CrExtensionsOptionsDialogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('  InstallExtensionWithInPageOptions();');
  }

  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/options_dialog_test.js';
  }

  /** @override */
  get suiteName() {
    return extension_options_dialog_tests.suiteName;
  }
};

TEST_F('CrExtensionsOptionsDialogTest', 'DISABLED_Layout', function() {
  this.runMochaTest(extension_options_dialog_tests.TestNames.Layout);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Navigation Helper Tests

var CrExtensionsNavigationHelperTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/navigation_helper_test.js';
  }

  /** @override */
  get suiteName() {
    return extension_navigation_helper_tests.suiteName;
  }
};

TEST_F('CrExtensionsNavigationHelperTest', 'Basic', function() {
  this.runMochaTest(extension_navigation_helper_tests.TestNames.Basic);
});

TEST_F('CrExtensionsNavigationHelperTest', 'Conversion', function() {
  this.runMochaTest(extension_navigation_helper_tests.TestNames.Conversions);
});

TEST_F('CrExtensionsNavigationHelperTest', 'PushAndReplaceState', function() {
  this.runMochaTest(
      extension_navigation_helper_tests.TestNames.PushAndReplaceState);
});

TEST_F('CrExtensionsNavigationHelperTest', 'SupportedRoutes', function() {
  this.runMochaTest(
      extension_navigation_helper_tests.TestNames.SupportedRoutes);
});

////////////////////////////////////////////////////////////////////////////////
// Error Console tests

var CrExtensionsErrorConsoleTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/error_console_test.js';
  }

  /** @override */
  testGenPreamble() {
    GEN('  SetDevModeEnabled(true);');
    // TODO(https://crbug.com/1269161): Update the associated extensions to
    // Manifest V3 and stop ignoring deprecated manifest version warnings.
    GEN('  SetSilenceDeprecatedManifestVersionWarnings(true);');
    GEN('  InstallErrorsExtension();');
  }

  /** @override */
  testGenPostamble() {
    // Return settings to default.
    GEN('  SetDevModeEnabled(false);');
    GEN('  SetSilenceDeprecatedManifestVersionWarnings(false);');
  }
};

TEST_F('CrExtensionsErrorConsoleTest', 'TestUpDownErrors', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// extensions-toggle-row tests.

var CrExtensionsToggleRowTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/toggle_row_test.js';
  }
};

TEST_F('CrExtensionsToggleRowTest', 'ToggleRowTest', function() {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// kiosk mode tests.

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');

var CrExtensionsKioskModeTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/kiosk_mode_test.js';
  }

  /** @override */
  get suiteName() {
    return extension_kiosk_mode_tests.suiteName;
  }
};

TEST_F('CrExtensionsKioskModeTest', 'AddButton', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.AddButton);
});

TEST_F('CrExtensionsKioskModeTest', 'Layout', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.Layout);
});

TEST_F('CrExtensionsKioskModeTest', 'AutoLaunch', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.AutoLaunch);
});

TEST_F('CrExtensionsKioskModeTest', 'Bailout', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.Bailout);
});

TEST_F('CrExtensionsKioskModeTest', 'Updated', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.Updated);
});

TEST_F('CrExtensionsKioskModeTest', 'AddError', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.AddError);
});

GEN('#endif');

////////////////////////////////////////////////////////////////////////////////
// RuntimeHostsDialog tests

var CrExtensionsRuntimeHostsDialogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/runtime_hosts_dialog_test.js';
  }
};

TEST_F('CrExtensionsRuntimeHostsDialogTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// RuntimeHostPermissions tests

var CrExtensionsRuntimeHostPermissionsTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/runtime_host_permissions_test.js';
  }
};

TEST_F('CrExtensionsRuntimeHostPermissionsTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// HostPermissionsToggleList tests

var CrExtensionsHostPermissionsToggleListTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/host_permissions_toggle_list_test.js';
  }
};

TEST_F('CrExtensionsHostPermissionsToggleListTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// ExtensionReviewPanel tests

var CrExtensionsSafetyCheckReviewPanelTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/review_panel_test.js';
  }
};

TEST_F('CrExtensionsSafetyCheckReviewPanelTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// SitePermissions tests

var CrExtensionsSitePermissionsTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/site_permissions_test.js';
  }
};

TEST_F('CrExtensionsSitePermissionsTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// SitePermissionsBySite tests

var CrExtensionsSitePermissionsBySiteTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/site_permissions_by_site_test.js';
  }
};

TEST_F('CrExtensionsSitePermissionsBySiteTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// SitePermissionsEditUrlDialog tests

var CrExtensionsSitePermissionsEditUrlDialogTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/site_permissions_edit_url_dialog_test.js';
  }
};

TEST_F('CrExtensionsSitePermissionsEditUrlDialogTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// SitePermissionsList tests

var CrExtensionsSitePermissionsListTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/site_permissions_list_test.js';
  }
};

TEST_F('CrExtensionsSitePermissionsListTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// UrlUtil tests

var CrUrlUtilTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/url_util_test.js';
  }
};

TEST_F('CrUrlUtilTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// SitePermissionsEditPermissionsDialog tests

var CrExtensionsSitePermissionsEditPermissionsDialogTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/site_permissions_edit_permissions_dialog_test.js';
  }
};

TEST_F('CrExtensionsSitePermissionsEditPermissionsDialogTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// SitePermissionsSiteGroup tests

var CrExtensionsSitePermissionsSiteGroupTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/site_permissions_site_group_test.js';
  }
};

TEST_F('CrExtensionsSitePermissionsSiteGroupTest', 'All', () => {
  mocha.run();
});
