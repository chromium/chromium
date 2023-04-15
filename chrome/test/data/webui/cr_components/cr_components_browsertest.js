// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/browser_features.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "crypto/crypto_buildflags.h"');

/** Test fixture for shared Polymer 3 components. */
var CrComponentsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

var CrComponentsColorChangeListenerTest =
    class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/color_change_listener_test.js';
  }
};

TEST_F('CrComponentsColorChangeListenerTest', 'All', function() {
  mocha.run();
});

var CrComponentsManagedFootnoteTest = class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/managed_footnote_test.js';
  }
};

TEST_F('CrComponentsManagedFootnoteTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(USE_NSS_CERTS)');

/**
 * Test fixture for chrome://settings/certificates. This tests the
 * certificate-manager component in the context of the Settings privacy page.
 */
var CrComponentsCertificateManagerTest = class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=cr_components/certificate_manager_test.js';
  }
};

TEST_F('CrComponentsCertificateManagerTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // BUILDFLAG(USE_NSS_CERTS)');


GEN('#if BUILDFLAG(USE_NSS_CERTS) && BUILDFLAG(IS_CHROMEOS)');

/**
 * ChromeOS specific test fixture for chrome://settings/certificates, testing
 * the certificate provisioning UI. This tests the certificate-manager component
 * in the context of the Settings privacy page.
 */
var CrComponentsCertificateManagerProvisioningTest =
    class extends CrComponentsCertificateManagerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=cr_components/certificate_manager_provisioning_test.js';
  }
};

TEST_F('CrComponentsCertificateManagerProvisioningTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // BUILDFLAG(USE_NSS_CERTS) && BUILDFLAG(IS_CHROMEOS)');

var CrComponentsManagedDialogTest = class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/managed_dialog_test.js';
  }
};

TEST_F('CrComponentsManagedDialogTest', 'All', function() {
  mocha.run();
});

var CrComponentsLocalizedLinkTest = class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/localized_link_test.js';
  }
};

TEST_F('CrComponentsLocalizedLinkTest', 'All', function() {
  mocha.run();
});

var CrComponentsAppManagementPermissionItemTest =
    class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/app_management/permission_item_test.js';
  }
};

TEST_F('CrComponentsAppManagementPermissionItemTest', 'All', function() {
  mocha.run();
});

var CrComponentsAppManagementFileHandlingItemTest =
    class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/app_management/file_handling_item_test.js';
  }
};

TEST_F('CrComponentsAppManagementFileHandlingItemTest', 'All', function() {
  mocha.run();
});

var CrComponentsAppManagementWindowModeTest =
    class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/app_management/window_mode_item_test.js';
  }
};

TEST_F('CrComponentsAppManagementWindowModeTest', 'All', function() {
  mocha.run();
});

var CrComponentsAppManagementUninstallButtonTest =
    class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_components/app_management/uninstall_button_test.js';  //  presubmit: ignore-long-line
  }
};

TEST_F('CrComponentsAppManagementUninstallButtonTest', 'All', function() {
  mocha.run();
});

/**
 * This is tested from chrome://settings/ so that the test has access to the
 * chrome.settingsPrivate permission.
 */
var CrComponentsSettingsPrefsTest = class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=cr_components/settings_prefs_test.js';  //  presubmit: ignore-long-line
  }
};

TEST_F('CrComponentsSettingsPrefsTest', 'All', function() {
  mocha.run();
});

var CrComponentsSettingsPrefUtilsTest = class extends CrComponentsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=cr_components/settings_pref_util_test.js';  //  presubmit: ignore-long-line
  }
};

TEST_F('CrComponentsSettingsPrefUtilsTest', 'All', function() {
  mocha.run();
});
