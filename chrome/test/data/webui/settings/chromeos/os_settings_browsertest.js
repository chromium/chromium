// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for Chrome OS settings page. */

// Path to general chrome browser settings and associated utilities.
const BROWSER_SETTINGS_PATH = '../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

// Only run in release builds because we frequently see test timeouts in debug.
// We suspect this is because the settings page loads slowly in debug.
// https://crbug.com/1003483
GEN('#if defined(NDEBUG)');

GEN('#include "ash/public/cpp/ash_features.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

// Generic test fixture for CrOS Polymer Settings elements to be overridden by
// individual element tests.
const OSSettingsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat(
        [BROWSER_SETTINGS_PATH + 'ensure_lazy_loaded.js']);
  }

  /** @override */
  setUp() {
    super.setUp();
    settings.ensureLazyLoaded('chromeos');
  }
};

// Tests for the About section.
// eslint-disable-next-line no-var
var OSSettingsAboutPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_about_page/os_about_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_about_page_browser_proxy.js',
      'os_about_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAboutPageTest', 'AboutPage', () => {
  settings_about_page.registerTests();
  mocha.run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('OSSettingsAboutPageTest', 'AboutPage_OfficialBuild', () => {
  settings_about_page.registerOfficialBuildTests();
  mocha.run();
});
GEN('#endif');

// Test fixture for the chrome://os-settings/accounts page
// eslint-disable-next-line no-var
var OSSettingsAddUsersTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'accounts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      'fake_users_private.js',
      'add_users_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAddUsersTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the main contents of the settings page.
// eslint-disable-next-line no-var
var OSSettingsPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_settings_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPageTest', 'AllJsTests', () => {
  // Run all registered tests.
  mocha.run();
});

// Tests for the Apps section (combines Chrome and Android apps).
// eslint-disable-next-line no-var
var OSSettingsAppsPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_apps_page/os_apps_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'chromeos/test_android_apps_browser_proxy.js',
      'apps_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppsPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Generic test fixture for CrOS Polymer App Management elements to be
// overridden by individual element tests.
const OSSettingsAppManagementBrowserTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'os_apps_page.html';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(['features::kAppManagement'])
    };
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/cr/ui/store.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_store.js',
      'app_management/test_util.js',
      'app_management/test_store.js',
    ]);
  }

  /** @override */
  get runAccessibilityChecks() {
    return true;
  }
};

// Text fixture for the app management dom switch element.
// eslint-disable-next-line no-var
var OSSettingsAppManagementDomSwitchTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/dom_switch.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/dom_switch_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementDomSwitchTest', 'All', function() {
  mocha.run();
});

// Test fixture for the app management settings page.
// eslint-disable-next-line no-var
var OSSettingsAppManagementPageTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/app_management_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/app_management_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the app management pwa permission view element.
// eslint-disable-next-line no-var
var OSSettingsAppManagementPwaPermissionViewTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/pwa_permission_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/pwa_permission_view_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementPwaPermissionViewTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the app management arc permission view element.
// eslint-disable-next-line no-var
var OSSettingsAppManagementArcPermissionViewTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/arc_permission_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/arc_permission_view_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementArcPermissionViewTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the app management managed app view.
// eslint-disable-next-line no-var
var OSSettingsAppManagementManagedAppTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/pwa_permission_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/managed_apps_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementManagedAppTest', 'AllJsTests', () => {
  mocha.run();
});


// Test fixture for the app management reducers.
// eslint-disable-next-line no-var
var OSSettingsAppManagementReducersTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/reducers_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementReducersTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the Device page.
// eslint-disable-next-line no-var
var OSSettingsBluetoothPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'bluetooth_page/bluetooth_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      'fake_bluetooth.js',
      'fake_bluetooth_private.js',
      'bluetooth_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsBluetoothPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the Crostini page.
// eslint-disable-next-line no-var
var OSSettingsCrostiniPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'crostini_page/crostini_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kCrostini']};
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_crostini_browser_proxy.js',
      'crostini_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsCrostiniPageTest', 'All', () => {
  mocha.run();
});

// Test fixture for the Date and Time page.
// eslint-disable-next-line no-var
var OSSettingsDateTimePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'date_time_page/date_time_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'date_time_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsDateTimePageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the Device page.
// eslint-disable-next-line no-var
var OSSettingsDevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'device_page/device_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      'fake_system_display.js',
      'device_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsDevicePageTest', 'DevicePageTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.DevicePage)).run();
});

TEST_F('OSSettingsDevicePageTest', 'DisplayTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Display)).run();
});

TEST_F('OSSettingsDevicePageTest', 'KeyboardTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Keyboard)).run();
});

TEST_F('OSSettingsDevicePageTest', 'NightLightTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.NightLight)).run();
});

TEST_F('OSSettingsDevicePageTest', 'PointersTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Pointers)).run();
});

TEST_F('OSSettingsDevicePageTest', 'PowerTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Power)).run();
});

TEST_F('OSSettingsDevicePageTest', 'StylusTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Stylus)).run();
});

// Tests for the Fingerprint page.
// eslint-disable-next-line no-var
var OSSettingsFingerprintListTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/fingerprint_list.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'fingerprint_browsertest_chromeos.js',
    ]);
  }
};

TEST_F('OSSettingsFingerprintListTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for Google Assistant Page.
// eslint-disable-next-line no-var
var OSSettingsGoogleAssistantPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'google_assistant_page/google_assistant_page.html';
  }

  /** @override */
  get commandLineSwitches() {
    return [{
      switchName: 'enable-voice-interaction',
    }];
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'google_assistant_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsGoogleAssistantPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-detail-page.
// eslint-disable-next-line no-var
var OSSettingsInternetDetailPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'internet_page/internet_detail_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH + '../chromeos/cr_onc_strings.js',
      'internet_detail_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetDetailPageTest', 'InternetDetailPage', () => {
  mocha.run();
});

// Test fixture for settings-internet-page.
// eslint-disable-next-line no-var
var OSSettingsInternetPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'internet_page/internet_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH + '../chromeos/cr_onc_strings.js',
      'internet_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetPageTest', 'InternetPage', () => {
  mocha.run();
});

// Test fixture for settings-internet-subpage.
// eslint-disable-next-line no-var
var OSSettingsInternetSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'internet_page/internet_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH + '../chromeos/cr_onc_strings.js',
      'internet_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetSubpageTest', 'InternetSubpage', () => {
  mocha.run();
});

// Test fixture for the main settings page.
// eslint-disable-next-line no-var
var OSSettingsMainTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_settings_main/os_settings_main.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'os_settings_main_test.js',
    ]);
  }
};

TEST_F('OSSettingsMainTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the side-nav menu.
// eslint-disable-next-line no-var
var OSSettingsMenuTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_settings_menu_test.js',
    ]);
  }
};

TEST_F('OSSettingsMenuTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage feature item.
// eslint-disable-next-line no-var
var OSSettingsMultideviceFeatureItemTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'multidevice_page/multidevice_feature_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'multidevice_feature_item_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceFeatureItemTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage feature toggle.
// eslint-disable-next-line no-var
var OSSettingsMultideviceFeatureToggleTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'multidevice_page/multidevice_feature_toggle.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'multidevice_feature_toggle_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceFeatureToggleTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings page.
// eslint-disable-next-line no-var
var OSSettingsMultidevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'multidevice_page/multidevice_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultidevicePageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice Smart Lock subpage.
// eslint-disable-next-line no-var
var OSSettingsMultideviceSmartLockSubpageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'multidevice_page/multidevice_smartlock_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_smartlock_subpage_test.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceSmartLockSubpageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage.
// eslint-disable-next-line no-var
var OSSettingsMultideviceSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'multidevice_page/multidevice_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceSubpageTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageAccountManagerTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/account_manager.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'people_page_account_manager_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageAccountManagerTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageChangePictureTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/change_picture.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'people_page_change_picture_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageChangePictureTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageKerberosAccountsTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/kerberos_accounts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'people_page_kerberos_accounts_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageKerberosAccountsTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageLockScreenTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/lock_screen.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'fake_quick_unlock_private.js',
      'fake_quick_unlock_uma.js',
      'quick_unlock_authenticate_browsertest_chromeos.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageLockScreenTest', 'AllJsTests', () => {
  settings_people_page_quick_unlock.registerLockScreenTests();
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageQuickUnlockAuthenticateTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'people_page/lock_screen_password_prompt_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      'fake_quick_unlock_private.js', 'fake_quick_unlock_uma.js',
      'quick_unlock_authenticate_browsertest_chromeos.js'
    ]);
  }
};

TEST_F('OSSettingsPeoplePageQuickUnlockAuthenticateTest', 'AllJsTests', () => {
  settings_people_page_quick_unlock.registerAuthenticateTests();
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageSetupPinDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/setup_pin_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      'fake_quick_unlock_private.js', 'fake_quick_unlock_uma.js',
      'quick_unlock_authenticate_browsertest_chromeos.js'
    ]);
  }
};

TEST_F('OSSettingsPeoplePageSetupPinDialogTest', 'AllJsTests', () => {
  settings_people_page_quick_unlock.registerSetupPinDialogTests();
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageSyncControlsTest = class extends OSSettingsBrowserTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettingsSync']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'os_sync_controls_test.js'
    ]);
  }
};

TEST_F('OSSettingsPeoplePageSyncControlsTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the People section.
// eslint-disable-next-line no-var
var OSSettingsPeoplePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'sync_test_util.js',
      BROWSER_SETTINGS_PATH + 'test_profile_info_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_sync_browser_proxy.js',
      'os_people_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the Personalization section.
// eslint-disable-next-line no-var
var OSSettingsPersonalizationPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_wallpaper_browser_proxy.js',
      'personalization_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPersonalizationPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Plugin VM page.
// eslint-disable-next-line no-var
var OSSettingsPluginVmPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'plugin_vm_page/plugin_vm_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'plugin_vm_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPluginVmPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS printer entry.
// eslint-disable-next-line no-var
var OSSettingsPrinterEntryTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'printing_page/cups_printers_entry.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'cups_printer_entry_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrinterEntryTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS printer landing page.
// eslint-disable-next-line no-var
var OSSettingsPrinterLandingPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'printing_page/cups_printers.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      'test_cups_printers_browser_proxy.js',
      'cups_printer_landing_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrinterLandingPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS page.
// eslint-disable-next-line no-var
var OSSettingsPrintingPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'printing_page/cups_printers.html';
  }

  /** @override */
  get featureList() {
    return {disabled: ['features::kCupsPrintersUiOverhaul']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_cups_printers_browser_proxy.js',
      'cups_printer_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrintingPageTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsLanguagesPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_languages_page/os_languages_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'fake_language_settings_private.js',
      BROWSER_SETTINGS_PATH + 'test_languages_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'fake_input_method_private.js',
      'os_languages_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsLanguagesPageTest', 'LanguageMenu', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.LanguageMenu)).run();
});

TEST_F('OSSettingsLanguagesPageTest', 'InputMethods', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.InputMethods)).run();
});

// Tests for the Reset section.
// eslint-disable-next-line no-var
var OSSettingsResetPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'reset_page/reset_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'test_os_reset_browser_proxy.js',
      'os_reset_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsResetPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the "Search and assistant" page.
// eslint-disable-next-line no-var
var OSSettingsSearchPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_search_page/os_search_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_search_engines_browser_proxy.js',
      'os_search_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsSearchPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Smb Shares page.
// eslint-disable-next-line no-var
var OSSettingsSmbPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_files_page/smb_shares_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'smb_shares_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsSmbPageTest', 'AllJsTests', () => {
  mocha.run();
});

GEN('#endif  // defined(NDEBUG)');
