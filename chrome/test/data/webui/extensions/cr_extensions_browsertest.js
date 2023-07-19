// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_test_base.h"');
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
    return 'ExtensionSettingsTestBase';
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
