// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#if defined(OS_CHROMEOS)');
GEN('#include "ash/public/cpp/ash_features.h"');
GEN('#include "chromeos/constants/chromeos_switches.h"');
GEN('#endif  // defined(OS_CHROMEOS)');

GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "components/autofill/core/common/autofill_features.h"');
GEN('#include "components/omnibox/common/omnibox_features.h"');

/**
 * Test fixture for Polymer Settings elements.
 * @constructor
 * @extends {PolymerTest}
 */
function CrSettingsBrowserTest() {}

CrSettingsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  },

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    'ensure_lazy_loaded.js',
  ],

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');

    settings.ensureLazyLoaded();
  },
};

GEN('#if defined(OS_CHROMEOS)');
/**
 * Test fixture for Polymer Settings elements that are used on ChromeOS.
 * @constructor
 * @extends {PolymerTest}
 */
// TODO(crbug/950007): Remove this when kSplitSettings is enabled by default.
function CrSettingsBrowserTestCrOS() {}

CrSettingsBrowserTestCrOS.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kSplitSettings']};
  },
};
GEN('#endif  // defined(OS_CHROMEOS)');

// Have to include command_line.h manually due to GEN calls below.
GEN('#include "base/command_line.h"');

function CrSettingsCheckboxTest() {}

CrSettingsCheckboxTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/settings_checkbox.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'checkbox_tests.js',
  ]),
};

TEST_F('CrSettingsCheckboxTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSliderTest() {}

CrSettingsSliderTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/settings_slider.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'settings_slider_tests.js',
  ]),
};

TEST_F('CrSettingsSliderTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsTextareaTest() {}

CrSettingsTextareaTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/settings_textarea.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'settings_textarea_tests.js',
  ]),
};

TEST_F('CrSettingsTextareaTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsToggleButtonTest() {}

CrSettingsToggleButtonTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/settings_toggle_button.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'settings_toggle_button_tests.js',
  ]),
};

TEST_F('CrSettingsToggleButtonTest', 'All', function() {
  mocha.run();
});

function CrSettingsDropdownMenuTest() {}

CrSettingsDropdownMenuTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/settings_dropdown_menu.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'dropdown_menu_tests.js',
  ]),
};

TEST_F('CrSettingsDropdownMenuTest', 'All', function() {
  mocha.run();
});

function CrSettingsPrefUtilTest() {}

CrSettingsPrefUtilTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/prefs/pref_util.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'pref_util_tests.js',
  ]),
};

TEST_F('CrSettingsPrefUtilTest', 'All', function() {
  mocha.run();
});

function CrSettingsPrefsTest() {}

CrSettingsPrefsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/prefs/prefs.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'prefs_test_cases.js',
    'prefs_tests.js',
  ]),
};

TEST_F('CrSettingsPrefsTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAboutPageTest() {}

CrSettingsAboutPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/about_page/about_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    '../test_browser_proxy.js',
    'test_lifetime_browser_proxy.js',
    'test_about_page_browser_proxy.js',
    'about_page_tests.js',
  ]),
};

TEST_F('CrSettingsAboutPageTest', 'AboutPage', function() {
  settings_about_page.registerTests();
  mocha.run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsAboutPageTest', 'AboutPage_OfficialBuild', function() {
  settings_about_page.registerOfficialBuildTests();
  mocha.run();
});
GEN('#endif');

/**
 * Test fixture for
 * chrome/browser/resources/settings/autofill_page/autofill_page.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAutofillPageTest() {}

CrSettingsAutofillPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/autofill_page/autofill_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    '../test_browser_proxy.js',
    'autofill_page_test.js',
    'ensure_lazy_loaded.js',
    'fake_settings_private.js',
    'passwords_and_autofill_fake_data.js',
    'test_open_window_proxy.js',
    'test_password_manager_proxy.js',
  ]),
};

TEST_F('CrSettingsAutofillPageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/autofill_page/autofill_section.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAutofillSectionCompanyEnabledTest() {}

CrSettingsAutofillSectionCompanyEnabledTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/autofill_page/autofill_section.html',

  featureList: {enabled: ['autofill::features::kAutofillEnableCompanyName']},

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'passwords_and_autofill_fake_data.js',
    '../test_util.js',
    'autofill_section_test.js',
  ]),
};

TEST_F('CrSettingsAutofillSectionCompanyEnabledTest', 'All', function() {
  // Use 'EnableCompanyName' to inform tests that the feature is enabled.
  const loadTimeDataOverride = {};
  loadTimeDataOverride['EnableCompanyName'] = true;
  loadTimeData.overrideValues(loadTimeDataOverride);
  mocha.run();
});

function CrSettingsAutofillSectionCompanyDisabledTest() {}

CrSettingsAutofillSectionCompanyDisabledTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/autofill_page/autofill_section.html',

  featureList: {disabled: ['autofill::features::kAutofillEnableCompanyName']},

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'passwords_and_autofill_fake_data.js',
    '../test_util.js',
    'autofill_section_test.js',
  ]),
};

TEST_F('CrSettingsAutofillSectionCompanyDisabledTest', 'All', function() {
  // Use 'EnableCompanyName' to inform tests that the feature is disabled.
  const loadTimeDataOverride = {};
  loadTimeDataOverride['EnableCompanyName'] = false;
  loadTimeData.overrideValues(loadTimeDataOverride);
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/autofill_page/passwords_section.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPasswordsSectionTest() {}

CrSettingsPasswordsSectionTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/autofill_page/passwords_section.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../mock_timer.js',
    '../test_browser_proxy.js',
    'passwords_and_autofill_fake_data.js',
    'passwords_section_test.js',
    'sync_test_util.js',
    'test_password_manager_proxy.js',
    '../test_util.js',
  ]),
};

TEST_F('CrSettingsPasswordsSectionTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
/**
 * Test fixture for CrOS specific behavior in
 * chrome/browser/resources/settings/autofill_page/passwords_section.html.
 * See http://crbug.com/917178 for details.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPasswordsSectionTest_Cros() {}

CrSettingsPasswordsSectionTest_Cros.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/autofill_page/passwords_section.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'passwords_and_autofill_fake_data.js',
    'passwords_section_test_cros.js',
    'test_password_manager_proxy.js',
  ]),
};


TEST_F('CrSettingsPasswordsSectionTest_Cros', 'DISABLED_All', function() {
  mocha.run();
});
GEN('#endif  // defined(OS_CHROMEOS)');

/**
 * Test fixture for
 * chrome/browser/resources/settings/autofill_page/payments_section.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPaymentsSectionTest() {}

CrSettingsPaymentsSectionTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/autofill_page/payments_section.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'passwords_and_autofill_fake_data.js',
    'sync_test_util.js',
    'test_sync_browser_proxy.js',
    '../test_util.js',
    'payments_section_test.js',
  ]),
};

TEST_F('CrSettingsPaymentsSectionTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/lock_screen_password_prompt_dialog.html.
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageQuickUnlockAuthenticateTest() {}

CrSettingsPeoplePageQuickUnlockAuthenticateTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/people_page/lock_screen_password_prompt_dialog.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js', 'chromeos/fake_quick_unlock_private.js',
    'chromeos/fake_quick_unlock_uma.js',
    'chromeos/quick_unlock_authenticate_browsertest_chromeos.js'
  ]),
};

TEST_F('CrSettingsPeoplePageQuickUnlockAuthenticateTest', 'All', function() {
  settings_people_page_quick_unlock.registerAuthenticateTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/lock_screen.html
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsPeoplePageLockScreenTest() {}

CrSettingsPeoplePageLockScreenTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/lock_screen.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'chromeos/fake_quick_unlock_private.js',
    'fake_settings_private.js',
    'chromeos/fake_quick_unlock_uma.js',
    'chromeos/quick_unlock_authenticate_browsertest_chromeos.js',
    '../test_util.js',
  ]),
};

TEST_F('CrSettingsPeoplePageLockScreenTest', 'All', function() {
  settings_people_page_quick_unlock.registerLockScreenTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/setup_pin_dialog.html.
 *
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsPeoplePageSetupPinDialogTest() {}

CrSettingsPeoplePageSetupPinDialogTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/setup_pin_dialog.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../fake_chrome_event.js', 'chromeos/fake_quick_unlock_private.js',
    'fake_settings_private.js', 'chromeos/fake_quick_unlock_uma.js',
    'chromeos/quick_unlock_authenticate_browsertest_chromeos.js'
  ]),
};

TEST_F('CrSettingsPeoplePageSetupPinDialogTest', 'All', function() {
  settings_people_page_quick_unlock.registerSetupPinDialogTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/fingerprint_list.html.
 *
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsFingerprintListTest() {}

CrSettingsFingerprintListTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/fingerprint_list.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../test_util.js',
    '../test_browser_proxy.js',
    'chromeos/fingerprint_browsertest_chromeos.js',
  ]),
};

TEST_F('CrSettingsFingerprintListTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/change_picture.html.
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsPeoplePageChangePictureTest() {}

CrSettingsPeoplePageChangePictureTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/change_picture.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'chromeos/people_page_change_picture_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageChangePictureTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/account_manager.html.
 *
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsPeoplePageAccountManagerTest() {}

CrSettingsPeoplePageAccountManagerTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/account_manager.html',

  /** @override */
  featureList: {
    disabled: ['chromeos::features::kSplitSettings'],
  },

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'chromeos/people_page_account_manager_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageAccountManagerTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/kerberos_accounts.html.
 *
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsPeoplePageKerberosAccountsTest() {}

CrSettingsPeoplePageKerberosAccountsTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/kerberos_accounts.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    '../test_util.js',
    'chromeos/people_page_kerberos_accounts_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageKerberosAccountsTest', 'All', function() {
  mocha.run();
});

GEN('#else  // !defined(OS_CHROMEOS)');

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/manage_profile.html.
 * This is non-ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageManageProfileTest() {}

CrSettingsPeoplePageManageProfileTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/manage_profile.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'people_page_manage_profile_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageManageProfileTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // defined(OS_CHROMEOS)');

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/people_page.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageTest() {}

CrSettingsPeoplePageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/people_page.html',

  // Omnibox's Document Provider adds a dynamic setting to a list which is
  // length-checked in this test. The feature is defaulted on here during
  // rollout, reflecting its value in fieldtrial_testing_config.json.
  // See crbug.com/908435.
  featureList: {enabled: ['omnibox::kDocumentProvider']},

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'sync_test_util.js',
    'test_profile_info_browser_proxy.js',
    'test_sync_browser_proxy.js',
    'people_page_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageTest', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');
/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/sync_account_control.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageSyncAccountControlTest() {}

CrSettingsPeoplePageSyncAccountControlTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/sync_account_control.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'sync_test_util.js',
    'test_sync_browser_proxy.js',
    'sync_account_control_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageSyncAccountControlTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OS_CHROMEOS)');

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/sync_controls.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageSyncControlsTest() {}

CrSettingsPeoplePageSyncControlsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/sync_controls.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'sync_test_util.js',
    'test_sync_browser_proxy.js',
    'test_util.js',
    'people_page_sync_controls_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageSyncControlsTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/sync_page.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageSyncPageTest() {}

CrSettingsPeoplePageSyncPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/sync_page.html',

  // Omnibox's Document Provider adds a dynamic setting to a list which is
  // length-checked in this test. The feature is defaulted on here during
  // rollout, reflecting its value in fieldtrial_testing_config.json.
  // See crbug.com/908435.
  featureList: {enabled: ['omnibox::kDocumentProvider']},

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'sync_test_util.js',
    'test_sync_browser_proxy.js',
    '../test_util.js',
    'people_page_sync_page_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageSyncPageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/reset_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsResetPageTest() {}

CrSettingsResetPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/reset_page/reset_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_lifetime_browser_proxy.js',
    'test_reset_browser_proxy.js',
    '../test_util.js',
    'reset_page_test.js',
  ]),
};

TEST_F('CrSettingsResetPageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/reset_page/reset_profile_banner.html
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsResetProfileBannerTest() {}

CrSettingsResetProfileBannerTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/reset_page/reset_profile_banner.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_reset_browser_proxy.js',
    'reset_profile_banner_test.js',
  ]),
};

TEST_F('CrSettingsResetProfileBannerTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAppearancePageTest() {}

CrSettingsAppearancePageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/appearance_page/appearance_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_browser_proxy.js',
    'test_wallpaper_browser_proxy.js',
    'appearance_page_test.js',
  ]),
};

TEST_F('CrSettingsAppearancePageTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAppearanceFontsPageTest() {}

CrSettingsAppearanceFontsPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/appearance_page/appearance_fonts_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_browser_proxy.js',
    'appearance_fonts_page_test.js',
  ]),
};

TEST_F('CrSettingsAppearanceFontsPageTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsChromeCleanupPageTest() {}

CrSettingsChromeCleanupPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/chrome_cleanup_page/chrome_cleanup_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'chrome_cleanup_page_test.js',
  ]),
};

TEST_F('CrSettingsChromeCleanupPageTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsIncompatibleApplicationsPageTest() {}

CrSettingsIncompatibleApplicationsPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/incompatible_applications_page/incompatible_applications_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'incompatible_applications_page_test.js',
  ]),
};

TEST_F('CrSettingsIncompatibleApplicationsPageTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsDownloadsPageTest() {}

CrSettingsDownloadsPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/downloads_page/downloads_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_browser_proxy.js',
    'downloads_page_test.js',
  ]),
};

TEST_F('CrSettingsDownloadsPageTest', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');

/**
 * Test fixture for chrome/browser/resources/settings/default_browser_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsDefaultBrowserTest() {}

CrSettingsDefaultBrowserTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/default_browser_page/default_browser_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'default_browser_browsertest.js',
  ]),
};

TEST_F('CrSettingsDefaultBrowserTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/import_data_dialog.html
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsImportDataDialogTest() {}

CrSettingsImportDataDialogTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/import_data_dialog.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'import_data_dialog_test.js',
  ]),
};

TEST_F('CrSettingsImportDataDialogTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OS_CHROMEOS)');

/**
 * Test fixture for chrome/browser/resources/settings/search_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSearchPageTest() {}

CrSettingsSearchPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/search_page/search_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_search_engines_browser_proxy.js',
    'search_page_test.js',
  ]),
};

TEST_F('CrSettingsSearchPageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/search_engines_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSearchEnginesTest() {}

CrSettingsSearchEnginesTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/search_engines_page/search_engines_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    '../test_util.js',
    'test_extension_control_browser_proxy.js',
    'test_search_engines_browser_proxy.js',
    'search_engines_page_test.js',
  ]),
};

TEST_F('CrSettingsSearchEnginesTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(USE_NSS_CERTS)');

/**
 * Test fixture for chrome://settings/certificates. This tests the
 * certificate-manager component in the context of the Settings privacy page.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsCertificateManagerTest() {}

CrSettingsCertificateManagerTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /**
   * The certificate-manager subpage is embedded in privacy_page.html.
   * @override
   */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    '../test_browser_proxy.js',
    'certificate_manager_test.js',
  ]),
};

TEST_F('CrSettingsCertificateManagerTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // defined(USE_NSS_CERTS)');

/**
 * Test fixture for
 * chrome/browser/resources/settings/privacy_page/personalization_options.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPersonalizationOptionsTest() {}

CrSettingsPersonalizationOptionsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/personalization_options.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    'test_util.js',
    '../test_browser_proxy.js',
    'test_privacy_page_browser_proxy.js',
    'personalization_options_test.js',
  ]),
};

TEST_F('CrSettingsPersonalizationOptionsTest', 'AllBuilds', function() {
  mocha.grep('PersonalizationOptionsTests_AllBuilds').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsPersonalizationOptionsTest', 'OfficialBuild', function() {
  mocha.grep('PersonalizationOptionsTests_OfficialBuild').run();
});
GEN('#endif');

/**
 * Test fixture for
 * chrome/browser/resources/settings/privacy_page/privacy_page.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPrivacyPageTest() {}

CrSettingsPrivacyPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_util.js',
    '../test_browser_proxy.js',
    'test_privacy_page_browser_proxy.js',
    'test_sync_browser_proxy.js',
    'privacy_page_test.js',
  ]),
};
// Disabling on Mac due to flakiness.
// https://crbug.com/877109
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_All2 DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All2 All');
GEN('#endif');
TEST_F('CrSettingsPrivacyPageTest', 'MAYBE_All2', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteDataDetailsTest() {}

CrSettingsSiteDataDetailsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_local_data_browser_proxy.js',
    'site_data_details_subpage_tests.js',
  ]),
};

TEST_F('CrSettingsSiteDataDetailsTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsChooserExceptionListTest() {}

CrSettingsChooserExceptionListTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/site_settings/chooser_exception_list.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'chooser_exception_list_tests.js',
  ]),
};

TEST_F('CrSettingsChooserExceptionListTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsChooserExceptionListEntryTest() {}

CrSettingsChooserExceptionListEntryTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/site_settings/chooser_exception_list_entry.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../cr_elements/cr_policy_strings.js',
    '../test_browser_proxy.js',
    '../test_util.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'chooser_exception_list_entry_tests.js',
  ]),
};

TEST_F('CrSettingsChooserExceptionListEntryTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsCategoryDefaultSettingTest() {}

CrSettingsCategoryDefaultSettingTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'category_default_setting_tests.js',
  ]),
};

TEST_F('CrSettingsCategoryDefaultSettingTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsCategorySettingExceptionsTest() {}

CrSettingsCategorySettingExceptionsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'category_setting_exceptions_tests.js',
  ]),
};

TEST_F('CrSettingsCategorySettingExceptionsTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteEntryTest() {}

CrSettingsSiteEntryTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_local_data_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'site_entry_tests.js',
  ]),
};

TEST_F('CrSettingsSiteEntryTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAllSitesTest() {}

CrSettingsAllSitesTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_local_data_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'all_sites_tests.js',
  ]),
};

TEST_F('CrSettingsAllSitesTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteDetailsTest() {}

CrSettingsSiteDetailsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'site_details_tests.js',
  ]),
};

// Disabling on debug due to flaky timeout on Win7 Tests (dbg)(1) bot.
// https://crbug.com/825304 - later for other platforms in crbug.com/1021219.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('CrSettingsSiteDetailsTest', 'MAYBE_All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteDetailsPermissionTest() {}

CrSettingsSiteDetailsPermissionTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'site_details_permission_tests.js',
  ]),
};

TEST_F('CrSettingsSiteDetailsPermissionTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteListTest() {}

CrSettingsSiteListTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_util.js',
    '../test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'chromeos/test_multidevice_browser_proxy.js',
    'site_list_tests.js',
  ]),
};

// TODO(crbug.com/929455): flaky, fix.
TEST_F('CrSettingsSiteListTest', 'DISABLED_SiteList', function() {
  mocha.grep('SiteList').run();
});

TEST_F('CrSettingsSiteListTest', 'EditExceptionDialog', function() {
  mocha.grep('EditExceptionDialog').run();
});

TEST_F('CrSettingsSiteListTest', 'AddExceptionDialog', function() {
  mocha.grep('AddExceptionDialog').run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteListEntryTest() {}

CrSettingsSiteListEntryTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/site_settings/site_list_entry.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../cr_elements/cr_policy_strings.js',
    '../test_browser_proxy.js',
    '../test_util.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'site_list_entry_tests.js',
  ]),
};

TEST_F('CrSettingsSiteListEntryTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsZoomLevelsTest() {}

CrSettingsZoomLevelsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    '../test_util.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'zoom_levels_tests.js',
  ]),
};

TEST_F('CrSettingsZoomLevelsTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsProtocolHandlersTest() {}

CrSettingsProtocolHandlersTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_util.js',
    'test_site_settings_prefs_browser_proxy.js',
    'protocol_handlers_tests.js',
  ]),
};

TEST_F('CrSettingsProtocolHandlersTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/privacy_page/security_keys_subpage.html
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSecurityKeysSubpageTest() {}

CrSettingsSecurityKeysSubpageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/security_keys_subpage.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    '../test_browser_proxy.js',
    'security_keys_subpage_test.js',
  ]),
};

TEST_F('CrSettingsSecurityKeysSubpageTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteDataTest() {}

CrSettingsSiteDataTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  browsePreload: 'chrome://settings/site_settings/site_data.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    '../test_browser_proxy.js',
    'test_local_data_browser_proxy.js',
    'site_data_test.js',
  ]),
};

TEST_F('CrSettingsSiteDataTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');

/**
 * Test fixture for device-page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsDevicePageTest() {}

CrSettingsDevicePageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/device_page/device_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/assert.js',
    '../test_util.js',
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'chromeos/fake_system_display.js',
    'chromeos/device_page_tests.js',
  ]),
};

TEST_F('CrSettingsDevicePageTest', 'DevicePageTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.DevicePage)).run();
});

TEST_F('CrSettingsDevicePageTest', 'DisplayTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Display)).run();
});

TEST_F('CrSettingsDevicePageTest', 'KeyboardTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Keyboard)).run();
});

TEST_F('CrSettingsDevicePageTest', 'PointersTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Pointers)).run();
});

TEST_F('CrSettingsDevicePageTest', 'PowerTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Power)).run();
});

TEST_F('CrSettingsDevicePageTest', 'StylusTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Stylus)).run();
});

/**
 * Test fixture for device-page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsBluetoothPageTest() {}

CrSettingsBluetoothPageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/bluetooth_page/bluetooth_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/assert.js',
    '../fake_chrome_event.js',
    '../test_util.js',
    'chromeos/fake_bluetooth.js',
    'chromeos/fake_bluetooth_private.js',
    'chromeos/bluetooth_page_tests.js',
  ]),
};

TEST_F('CrSettingsBluetoothPageTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // defined(OS_CHROMEOS)');

/**
 * Test fixture for chrome/browser/resources/settings/settings_menu/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsMenuTest() {}

CrSettingsMenuTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/settings_menu/settings_menu.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'settings_menu_test.js',
  ]),
};

TEST_F('CrSettingsMenuTest', 'SettingsMenu', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/settings_page/settings_subpage.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSubpageTest() {}

CrSettingsSubpageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/settings_page/settings_subpage.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'settings_subpage_test.js',
  ]),
};

TEST_F('CrSettingsSubpageTest', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');
/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSystemPageTest() {}

CrSettingsSystemPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/system_page/system_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_lifetime_browser_proxy.js',
    'system_page_tests.js',
  ]),
};

TEST_F('CrSettingsSystemPageTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OS_CHROMEOS)');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsStartupUrlsPageTest() {}

CrSettingsStartupUrlsPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  browsePreload: 'chrome://settings/on_startup_page/startup_urls_page.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'startup_urls_page_test.js',
  ]),
};

TEST_F('CrSettingsStartupUrlsPageTest', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_MACOSX)');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsEditDictionaryPageTest() {}

CrSettingsEditDictionaryPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/languages_page/edit_dictionary_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    '../test_browser_proxy.js',
    'fake_language_settings_private.js',
    'edit_dictionary_page_test.js',
  ]),
};

TEST_F('CrSettingsEditDictionaryPageTest', 'All', function() {
  mocha.run();
});

GEN('#endif  //!defined(OS_MACOSX)');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsLanguagesTest() {}

CrSettingsLanguagesTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/languages_page/languages.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../fake_chrome_event.js',
    '../test_util.js',
    '../test_browser_proxy.js',
    'fake_language_settings_private.js',
    'fake_settings_private.js',
    'chromeos/fake_input_method_private.js',
    'test_languages_browser_proxy.js',
    'languages_tests.js',
  ]),
};

TEST_F('CrSettingsLanguagesTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsLanguagesPageTest() {}

CrSettingsLanguagesPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/languages_page/languages_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    '../test_util.js',
    '../test_browser_proxy.js',
    'fake_settings_private.js',
    'fake_language_settings_private.js',
    'chromeos/fake_input_method_private.js',
    'test_languages_browser_proxy.js',
    'languages_page_tests.js',
  ]),
};

TEST_F('CrSettingsLanguagesPageTest', 'AddLanguagesDialog', function() {
  mocha.grep(assert(languages_page_tests.TestNames.AddLanguagesDialog)).run();
});

TEST_F('CrSettingsLanguagesPageTest', 'LanguageMenu', function() {
  mocha.grep(assert(languages_page_tests.TestNames.LanguageMenu)).run();
});

TEST_F('CrSettingsLanguagesPageTest', 'Spellcheck', function() {
  mocha.grep(assert(languages_page_tests.TestNames.Spellcheck)).run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsLanguagesPageTest', 'SpellcheckOfficialBuild', function() {
  mocha.grep(assert(languages_page_tests.TestNames.SpellcheckOfficialBuild))
      .run();
});
GEN('#endif');

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var CrSettingsLanguagesPageTestCrOS = class extends CrSettingsBrowserTestCrOS {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/languages_page/languages_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../fake_chrome_event.js',
      '../test_util.js',
      '../test_browser_proxy.js',
      'fake_settings_private.js',
      'fake_language_settings_private.js',
      'chromeos/fake_input_method_private.js',
      'test_languages_browser_proxy.js',
      'languages_page_tests.js',
    ]);
  }
};

TEST_F('CrSettingsLanguagesPageTestCrOS', 'InputMethods', function() {
  mocha.grep(assert(languages_page_tests.TestNames.InputMethods)).run();
});

GEN('#endif  // defined(OS_CHROMEOS)');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsRouteTest() {}

CrSettingsRouteTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/route.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'route_tests.js',
  ]),
};

TEST_F('CrSettingsRouteTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function CrSettingsNonExistentRouteTest() {}

CrSettingsNonExistentRouteTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/non/existent/route',
};

// Failing on ChromiumOS dbg. https://crbug.com/709442
GEN('#if (defined(OS_WIN) || defined(OS_CHROMEOS)) && !defined(NDEBUG)');
GEN('#define MAYBE_NonExistentRoute DISABLED_NonExistentRoute');
GEN('#else');
GEN('#define MAYBE_NonExistentRoute NonExistentRoute');
GEN('#endif');
TEST_F('CrSettingsNonExistentRouteTest', 'MAYBE_NonExistentRoute', function() {
  suite('NonExistentRoutes', function() {
    test('redirect to basic', function() {
      assertEquals(settings.routes.BASIC, settings.getCurrentRoute());
      assertEquals('/', location.pathname);
    });
  });
  mocha.run();
});

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function CrSettingsRouteDynamicParametersTest() {}

CrSettingsRouteDynamicParametersTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/search?guid=a%2Fb&foo=42',
};

TEST_F('CrSettingsRouteDynamicParametersTest', 'All', function() {
  suite('DynamicParameters', function() {
    test('get parameters from URL and navigation', function(done) {
      assertEquals(settings.routes.SEARCH, settings.getCurrentRoute());
      assertEquals('a/b', settings.getQueryParameters().get('guid'));
      assertEquals('42', settings.getQueryParameters().get('foo'));

      const params = new URLSearchParams();
      params.set('bar', 'b=z');
      params.set('biz', '3');
      settings.navigateTo(settings.routes.SEARCH_ENGINES, params);
      assertEquals(settings.routes.SEARCH_ENGINES, settings.getCurrentRoute());
      assertEquals('b=z', settings.getQueryParameters().get('bar'));
      assertEquals('3', settings.getQueryParameters().get('biz'));
      assertEquals('?bar=b%3Dz&biz=3', window.location.search);

      window.addEventListener('popstate', function(event) {
        assertEquals('/search', settings.getCurrentRoute().path);
        assertEquals(settings.routes.SEARCH, settings.getCurrentRoute());
        assertEquals('a/b', settings.getQueryParameters().get('guid'));
        assertEquals('42', settings.getQueryParameters().get('foo'));
        done();
      });
      window.history.back();
    });
  });
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/settings_main/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsMainPageTest() {}

CrSettingsMainPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/settings_main/settings_main.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    '../test_util.js',
    'settings_main_test.js',
  ]),
};

// Times out on Windows Tests (dbg). See https://crbug.com/651296.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/667882
// Times out on Linux CFI. See http://crbug.com/929288.
GEN('#if !defined(NDEBUG) || defined(OS_LINUX)');
GEN('#define MAYBE_MainPage DISABLED_MainPage');
GEN('#else');
GEN('#define MAYBE_MainPage MainPage');
GEN('#endif');
TEST_F('CrSettingsMainPageTest', 'MAYBE_MainPage', function() {
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/search_settings.js.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSearchTest() {}

CrSettingsSearchTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/settings_page/settings_section.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'search_settings_test.js',
  ]),
};

TEST_F('CrSettingsSearchTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrControlledButtonTest() {}

CrControlledButtonTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/controlled_button.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'controlled_button_tests.js',
  ]),
};

TEST_F('CrControlledButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrControlledRadioButtonTest() {}

CrControlledRadioButtonTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/controlled_radio_button.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'controlled_radio_button_tests.js',
  ]),
};

TEST_F('CrControlledRadioButtonTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)');

function CrSettingsMetricsReportingTest() {}

CrSettingsMetricsReportingTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/privacy_page.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    '../test_browser_proxy.js',
    'test_privacy_page_browser_proxy.js',
    'metrics_reporting_tests.js',
  ]),
};

TEST_F('CrSettingsMetricsReportingTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)');

GEN('#if defined(OS_CHROMEOS)');

/**
 * Test fixture for the Smb Shares page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsSmbPageTest() {}

CrSettingsSmbPageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/downloads_page/smb_shares_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    'test_util.js',
    '../test_browser_proxy.js',
    'chromeos/smb_shares_page_tests.js',
  ]),
};

TEST_F('CrSettingsSmbPageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for the multidevice settings subpage feature item.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsMultideviceFeatureItemTest() {}

CrSettingsMultideviceFeatureItemTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/multidevice_page/multidevice_feature_item.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    'chromeos/multidevice_feature_item_tests.js',
  ]),
};

TEST_F('CrSettingsMultideviceFeatureItemTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for the multidevice settings subpage feature toggle.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsMultideviceFeatureToggleTest() {}

CrSettingsMultideviceFeatureToggleTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/multidevice_page/multidevice_feature_toggle.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    'chromeos/multidevice_feature_toggle_tests.js',
  ]),
};

TEST_F('CrSettingsMultideviceFeatureToggleTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for the multidevice settings page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsMultidevicePageTest() {}

CrSettingsMultidevicePageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/multidevice_page/multidevice_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'chromeos/test_multidevice_browser_proxy.js',
    'chromeos/multidevice_page_tests.js',
  ]),
};

TEST_F('CrSettingsMultidevicePageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for the multidevice Smart Lock subpage.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsMultideviceSmartLockSubpageTest() {}

CrSettingsMultideviceSmartLockSubpageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/multidevice_page/multidevice_smartlock_subpage.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'chromeos/test_multidevice_browser_proxy.js',
    '../test_util.js',
    'chromeos/multidevice_smartlock_subpage_test.js',
  ]),
};

TEST_F('CrSettingsMultideviceSmartLockSubpageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for the multidevice settings subpage.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsMultideviceSubpageTest() {}

CrSettingsMultideviceSubpageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/multidevice_page/multidevice_subpage.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'chromeos/test_multidevice_browser_proxy.js',
    'chromeos/multidevice_subpage_tests.js',
  ]),
};

TEST_F('CrSettingsMultideviceSubpageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for the Linux for Chromebook (Crostini) page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsCrostiniPageTest() {}

CrSettingsCrostiniPageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/crostini_page/crostini_page.html',

  /** @override */
  featureList: {
    enabled: ['features::kCrostini'],
    disabled: ['chromeos::features::kSplitSettings']
  },

  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_browser_proxy.js',
    'chromeos/test_crostini_browser_proxy.js',
    'chromeos/crostini_page_test.js',
  ]),
};

// Disabling on debug due to timeout.  https://crbug.com/1020318
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_Crostini_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_Crostini_All All');
GEN('#endif');

TEST_F('CrSettingsCrostiniPageTest', 'MAYBE_Crostini_All', function() {
  mocha.run();
});

/**
 * Test fixture for the Plugin VM page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsPluginVmPageTest() {}

CrSettingsPluginVmPageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/plugin_vm_page/plugin_vm_page.html',

  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_browser_proxy.js',
    'chromeos/plugin_vm_page_test.js',
  ]),
};

TEST_F('CrSettingsPluginVmPageTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for the Google Play Store (ARC) page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsAndroidAppsPageTest() {}

CrSettingsAndroidAppsPageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/android_apps_page/android_apps_page.html',

  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_browser_proxy.js',
    'chromeos/test_android_apps_browser_proxy.js',
    'chromeos/android_apps_page_test.js',
  ]),
};

// Disabled due to flakiness on linux-chromeos-rel
TEST_F('CrSettingsAndroidAppsPageTest', 'DISABLED_All', function() {
  mocha.run();
});

/**
 * Test fixture for the Date and Time page.
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsDateTimePageTest() {}

CrSettingsDateTimePageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/date_time_page/date_time_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    'chromeos/date_time_page_tests.js',
  ]),
};

TEST_F('CrSettingsDateTimePageTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 */
function CrSettingsGoogleAssistantPageTest() {}

CrSettingsGoogleAssistantPageTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/google_assistant_page/google_assistant_page.html',

  /** @override */
  commandLineSwitches: [{
    switchName: 'enable-voice-interaction',
  }],

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/promise_resolver.js',
    '../test_browser_proxy.js',
    'chromeos/google_assistant_page_test.js',
  ]),
};

TEST_F('CrSettingsGoogleAssistantPageTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // defined(OS_CHROMEOS)');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsExtensionControlledIndicatorTest() {}

CrSettingsExtensionControlledIndicatorTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/controls/extension_controlled_indicator.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_extension_control_browser_proxy.js',
    'extension_controlled_indicator_tests.js',
  ]),
};

TEST_F('CrSettingsExtensionControlledIndicatorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsChangePasswordPageTest() {}

CrSettingsChangePasswordPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/change_password_page/change_password_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'change_password_page_test.js',
  ]),
};

TEST_F('CrSettingsChangePasswordPageTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsOnStartupPageTest() {}

CrSettingsOnStartupPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/on_startup_page/on_startup_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'on_startup_page_tests.js',
  ]),
};

TEST_F('CrSettingsOnStartupPageTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSiteFaviconTest() {}

CrSettingsSiteFaviconTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/site_favicon.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'site_favicon_test.js',
  ]),
};

TEST_F('CrSettingsSiteFaviconTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');

/**
 * Test fixture for the chrome://settings/accounts page
 * @constructor
 * @extends {CrSettingsBrowserTestCrOS}
 * TODO(hsuregan): Remove when SplitSettings complete.
 */
function CrSettingsAddUsersTest() {}

CrSettingsAddUsersTest.prototype = {
  __proto__: CrSettingsBrowserTestCrOS.prototype,

  /** @override */
  browsePreload: 'chrome://settings/accounts.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTestCrOS.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'chromeos/fake_users_private.js',
    'chromeos/add_users_tests.js',
  ]),
};

// Test is consistently failing. http://crbug.com/960837
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_AddUsers DISABLED_AddUsers');
GEN('#else');
GEN('#define MAYBE_AddUsers AddUsers');
GEN('#endif');
TEST_F('CrSettingsAddUsersTest', 'MAYBE_AddUsers', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSplitSettingsFlagTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/basic_page/basic_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'split_settings_flag_test.js',
    ]);
  }
};

// Test is consistently failing. http://crbug.com/1008916
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_All3 DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All3 All');
GEN('#endif');
TEST_F('CrSettingsSplitSettingsFlagTest', 'MAYBE_All3', function() {
  mocha.run();
});

GEN('#endif  // defined(OS_CHROMEOS)');
