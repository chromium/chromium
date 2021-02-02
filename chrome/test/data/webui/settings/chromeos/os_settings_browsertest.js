// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for Chrome OS settings page. */

// Path to general chrome browser settings and associated utilities.
const BROWSER_SETTINGS_PATH = '../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/public/cpp/ash_features.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ui/display/display_features.h"');

// Most tests only run in release builds because we frequently see test timeouts
// in debug. We suspect this is because the settings page loads slowly in debug.
// https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_AllJsTests DISABLED_AllJsTests');
GEN('#else');
GEN('#define MAYBE_AllJsTests AllJsTests');
GEN('#endif');

// Generic test fixture for CrOS Polymer Settings elements to be overridden by
// individual element tests.
const OSSettingsBrowserTest = class extends Polymer2DeprecatedTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
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

// Tests for the settings-localized-link element.
// eslint-disable-next-line no-var
var SettingsLocalizedLinkTest = class extends OSSettingsBrowserTest {
  get browsePreload() {
    return super.browsePreload + 'chromeos/localized_link/localized_link.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'localized_link_test.js',
    ]);
  }
};

TEST_F('SettingsLocalizedLinkTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the About section.
// eslint-disable-next-line no-var
var OSSettingsAboutPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_about_page/os_about_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kEnableHostnameSetting']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      'test_about_page_browser_proxy_chromeos.js',
      'test_device_name_browser_proxy.js',
      'os_about_page_tests.js',
    ]);
  }
};

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_AllBuilds DISABLED_AllBuilds');
GEN('#else');
GEN('#define MAYBE_AllBuilds AllBuilds');
GEN('#endif');
TEST_F('OSSettingsAboutPageTest', 'MAYBE_AllBuilds', () => {
  mocha.grep('/^(?!AboutPageTest_OfficialBuild).*$/').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_OfficialBuild DISABLED_OfficialBuild');
GEN('#else');
GEN('#define MAYBE_OfficialBuild OfficialBuild');
GEN('#endif');
TEST_F('OSSettingsAboutPageTest', 'MAYBE_OfficialBuild', () => {
  mocha.grep('AboutPageTest_OfficialBuild').run();
});
GEN('#endif');

// Test fixture for the chrome://os-settings/controls/settings_slider
// eslint-disable-next-line no-var
var OSSettingsSliderTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'controls/settings_slider.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + 'settings_slider_tests.js',
    ]);
  }
};

TEST_F('OSSettingsSliderTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the chrome://os-settings/controls/settings_textarea
// eslint-disable-next-line no-var
var OSSettingsTextAreaTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'controls/settings_textarea.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'settings_textarea_tests.js',
    ]);
  }
};

TEST_F('OSSettingsTextAreaTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the chrome://os-settings/controls/settings_toggle_button
// eslint-disable-next-line no-var
var OSSettingsToggleButtonTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'controls/settings_toggle_button.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'settings_toggle_button_tests.js',
    ]);
  }
};

TEST_F('OSSettingsToggleButtonTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the chrome://os-settings/prefs/pref_util page
// eslint-disable-next-line no-var
var OSSettingsPrefUtilTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'prefs/pref_util.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'pref_util_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrefUtilTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the chrome://os-settings/prefs/prefs page
// eslint-disable-next-line no-var
var OSSettingsPrefsTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'prefs/prefs.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + 'prefs_test_cases.js',
      BROWSER_SETTINGS_PATH + 'prefs_tests.js'
    ]);
  }
};

TEST_F('OSSettingsPrefsTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

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

TEST_F('OSSettingsAddUsersTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for settings-lock-screen
// eslint-disable-next-line no-var
var OSSettingsLockScreenPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_people_page/lock_screen.html';
  }

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'lock_screen_tests.js',
    ]);
  }
};

TEST_F('OSSettingsLockScreenPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsLockScreenPageTestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsLockScreenPageTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsLockScreenPageTestWithAccountManagementFlowsV2Enabled',
    'MAYBE_AllJsTests', () => {
      mocha.run();
    });

// Tests for settings-user-page
// eslint-disable-next-line no-var
var OSSettingsUserPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'accounts.html';
  }

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      'fake_users_private.js',
      'user_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsUserPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsUserPageTestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsUserPageTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsUserPageTestWithAccountManagementFlowsV2Enabled',
    'MAYBE_AllJsTests', () => {
      mocha.run();
    });

// Tests for ambient mode page.
// eslint-disable-next-line no-var
var OSSettingsAmbientModePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'ambient_mode_page/ambient_mode_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'ambient_mode_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsAmbientModePageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for ambient mode photos page.
// eslint-disable-next-line no-var
var OSSettingsAmbientModePhotosPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'ambient_mode_page/ambient_mode_photos_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'ambient_mode_photos_page_test.js',
    ]);
  }
};

// TODO(https://crbug.com/1173526): Reenable flaky test.
TEST_F('OSSettingsAmbientModePhotosPageTest', 'DISABLED_AllJsTests', () => {
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

TEST_F('OSSettingsPageTest', 'MAYBE_AllJsTests', () => {
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
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + 'chromeos/test_android_apps_browser_proxy.js',
      'apps_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppsPageTest', 'MAYBE_AllJsTests', () => {
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
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/cr/ui/store.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_store.js',
      'app_management/test_util.js',
      'app_management/test_store.js',
    ]);
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

TEST_F('OSSettingsAppManagementDomSwitchTest', 'MAYBE_AllJsTests', function() {
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

TEST_F('OSSettingsAppManagementPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the app management pwa detail view element.
// eslint-disable-next-line no-var
var OSSettingsAppManagementPwaDetailViewTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/pwa_detail_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/pwa_detail_view_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementPwaDetailViewTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the app management arc detail view element.
// eslint-disable-next-line no-var
var OSSettingsAppManagementArcDetailViewTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/arc_detail_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/arc_detail_view_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementArcDetailViewTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the app management Plugin VM detail view element.
// eslint-disable-next-line no-var
var OSSettingsAppManagementPluginVmDetailViewTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/plugin_vm_detail_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'app_management/test_plugin_vm_browser_proxy.js',
      'app_management/plugin_vm_detail_view_test.js',
    ]);
  }
};

TEST_F(
    'OSSettingsAppManagementPluginVmDetailViewTest', 'MAYBE_AllJsTests', () => {
      mocha.run();
    });

// Test fixture for the app management managed app view.
// eslint-disable-next-line no-var
var OSSettingsAppManagementManagedAppTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'app_management/pwa_detail_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_management/managed_apps_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementManagedAppTest', 'MAYBE_AllJsTests', () => {
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

TEST_F('OSSettingsAppManagementReducersTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Bluetooth page.
// eslint-disable-next-line no-var
var OSSettingsBluetoothPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/bluetooth_page/bluetooth_page.html';
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

// Flaky. https://crbug.com/1035378
TEST_F('OSSettingsBluetoothPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Crostini page.
// eslint-disable-next-line no-var
var OSSettingsCrostiniPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/crostini_page/crostini_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kCrostini']};
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_crostini_browser_proxy.js',
      'test_guest_os_browser_proxy.js',
      'crostini_page_test.js',
    ]);
  }
};

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_MainPage DISABLED_MainPage');
GEN('#else');
GEN('#define MAYBE_MainPage MainPage');
GEN('#endif');
TEST_F('OSSettingsCrostiniPageTest', 'MAYBE_MainPage', function() {
  mocha.grep('\\bMainPage\\b').run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_SubPageDefault DISABLED_SubPageDefault');
GEN('#else');
GEN('#define MAYBE_SubPageDefault SubPageDefault');
GEN('#endif');
TEST_F('OSSettingsCrostiniPageTest', 'MAYBE_SubPageDefault', function() {
  mocha.grep('\\bSubPageDefault\\b').run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_SubPagePortForwarding DISABLED_SubPagePortForwarding');
GEN('#else');
GEN('#define MAYBE_SubPagePortForwarding SubPagePortForwarding');
GEN('#endif');
TEST_F('OSSettingsCrostiniPageTest', 'MAYBE_SubPagePortForwarding', function() {
  mocha.grep('\\bSubPagePortForwarding\\b').run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_DiskResize DISABLED_DiskResize');
GEN('#else');
GEN('#define MAYBE_DiskResize DiskResize');
GEN('#endif');
TEST_F('OSSettingsCrostiniPageTest', 'MAYBE_DiskResize', function() {
  mocha.grep('\\bDiskResize\\b').run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_SubPageSharedPaths DISABLED_SubPageSharedPaths');
GEN('#else');
GEN('#define MAYBE_SubPageSharedPaths SubPageSharedPaths');
GEN('#endif');
TEST_F('OSSettingsCrostiniPageTest', 'MAYBE_SubPageSharedPaths', function() {
  mocha.grep('\\bSubPageSharedPaths\\b').run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_SubPageSharedUsbDevices DISABLED_SubPageSharedUsbDevices');
GEN('#else');
GEN('#define MAYBE_SubPageSharedUsbDevices SubPageSharedUsbDevices');
GEN('#endif');
TEST_F(
    'OSSettingsCrostiniPageTest', 'MAYBE_SubPageSharedUsbDevices', function() {
      mocha.grep('\\bSubPageSharedUsbDevices\\b').run();
    });

// Test fixture for the Guest OS shared paths page.
// eslint-disable-next-line no-var
var OSSettingsGuestOsSharedPathsTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'guest_os/guest_os_shared_paths.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'guest_os_shared_paths_test.js',
    ]);
  }
};

TEST_F('OSSettingsGuestOsSharedPathsTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Guest OS shared USB devices page.
// eslint-disable-next-line no-var
var OSSettingsGuestOsSharedUsbDevicesTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'guest_os/guest_os_shared_usb_devices.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'guest_os_shared_usb_devices_test.js',
    ]);
  }
};

TEST_F('OSSettingsGuestOsSharedUsbDevicesTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the On Startup page.
// eslint-disable-next-line no-var
var OSSettingsOnStartupPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_search_page/on_startup_page.html';
  }

  get featureList() {
    return {
      enabled: [
        'ash::features::kFullRestore',
      ]
    };
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'on_startup_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsOnStartupPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Date and Time page.
// eslint-disable-next-line no-var
var OSSettingsDateTimePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/date_time_page/date_time_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'date_time_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsDateTimePageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Device page.
// eslint-disable-next-line no-var
var OSSettingsDevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/device_page/device_page.html';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kDisplayIdentification',
        'ash::features::kKeyboardBasedDisplayArrangementInSettings',
        'display::features::kListAllDisplayModes'
      ],
    };
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

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_DevicePageTest DISABLED_DevicePageTest');
GEN('#else');
GEN('#define MAYBE_DevicePageTest DevicePageTest');
GEN('#endif');
TEST_F('OSSettingsDevicePageTest', 'MAYBE_DevicePageTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.DevicePage)).run();
});

// Fails after https://chromium-review.googlesource.com/c/chromium/src/+/2640774
TEST_F('OSSettingsDevicePageTest', 'DISABLED_DisplayTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Display)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_KeyboardTest DISABLED_KeyboardTest');
GEN('#else');
GEN('#define MAYBE_KeyboardTest KeyboardTest');
GEN('#endif');
TEST_F('OSSettingsDevicePageTest', 'MAYBE_KeyboardTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Keyboard)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_NightLightTest DISABLED_NightLightTest');
GEN('#else');
GEN('#define MAYBE_NightLightTest NightLightTest');
GEN('#endif');
TEST_F('OSSettingsDevicePageTest', 'MAYBE_NightLightTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.NightLight)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_PointersTest DISABLED_PointersTest');
GEN('#else');
GEN('#define MAYBE_PointersTest PointersTest');
GEN('#endif');
TEST_F('OSSettingsDevicePageTest', 'MAYBE_PointersTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Pointers)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_PointersWithPointingStickTest \\');
GEN('    DISABLED_PointersWithPointingStickTest');
GEN('#else');
GEN('#define MAYBE_PointersWithPointingStickTest \\');
GEN('    PointersWithPointingStickTest');
GEN('#endif');
TEST_F(
    'OSSettingsDevicePageTest', 'MAYBE_PointersWithPointingStickTest', () => {
      mocha.grep(assert(device_page_tests.TestNames.PointingStick)).run();
    });

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_PowerTest DISABLED_PowerTest');
GEN('#else');
GEN('#define MAYBE_PowerTest PowerTest');
GEN('#endif');
TEST_F('OSSettingsDevicePageTest', 'MAYBE_PowerTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Power)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_StorageTest DISABLED_StorageTest');
GEN('#else');
GEN('#define MAYBE_StorageTest StorageTest');
GEN('#endif');
TEST_F('OSSettingsDevicePageTest', 'MAYBE_StorageTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Storage)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_StylusTest DISABLED_StylusTest');
GEN('#else');
GEN('#define MAYBE_StylusTest StylusTest');
GEN('#endif');
TEST_F('OSSettingsDevicePageTest', 'MAYBE_StylusTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Stylus)).run();
});

// Tests for the Device page with keyboard arrangement flag disabled.
// eslint-disable-next-line no-var
var OSSettingsDevicePageKeyboardArrangementDisabledTest =
    class extends OSSettingsDevicePageTest {
  /** @override */
  get featureList() {
    return {
      disabled: [
        'ash::features::kKeyboardBasedDisplayArrangementInSettings',
      ]
    };
  }
};

TEST_F(
    'OSSettingsDevicePageKeyboardArrangementDisabledTest', 'MAYBE_AllJsTests',
    () => {
      mocha
          .grep(assert(device_page_tests.TestNames.KeyboardArrangementDisabled))
          .run();
    });

// Tests for the Fingerprint page.
// eslint-disable-next-line no-var
var OSSettingsFingerprintListTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_people_page/fingerprint_list.html';
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

TEST_F('OSSettingsFingerprintListTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for Google Assistant Page.
// eslint-disable-next-line no-var
var OSSettingsGoogleAssistantPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/google_assistant_page/google_assistant_page.html';
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
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'google_assistant_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsGoogleAssistantPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-detail-page.
// eslint-disable-next-line no-var
var OSSettingsInternetConfigTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      'internet_config_test.js',
    ]);
  }
};

TEST_F('OSSettingsInternetConfigTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-detail-page.
// eslint-disable-next-line no-var
var OSSettingsInternetDetailPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/internet_page/internet_detail_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kOsSettingsDeepLinking']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      'internet_detail_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetDetailPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-detail-menu.
// eslint-disable-next-line no-var
var OSSettingsInternetDetailMenuTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/internet_page/internet_detail_menu.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kUpdatedCellularActivationUi']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      'internet_detail_menu_test.js',
    ]);
  }
};

TEST_F('OSSettingsInternetDetailMenuTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-page.
// eslint-disable-next-line no-var
var OSSettingsInternetPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/internet_page/internet_page.html';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kOsSettingsDeepLinking',
        'chromeos::features::kUpdatedCellularActivationUi',
      ]
    };
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH +
          '../cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js',
      'internet_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-subpage.
// eslint-disable-next-line no-var
var OSSettingsInternetSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/internet_page/internet_subpage.html';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kUpdatedCellularActivationUi',
        'chromeos::features::kOsSettingsDeepLinking'
      ]
    };
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH +
          '../cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js',
      'internet_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetSubpageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-known-networks-page.
// eslint-disable-next-line no-var
var OSSettingsInternetKnownNetworksPageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/internet_page/internet_known_networks_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kOsSettingsDeepLinking']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      'internet_known_networks_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetKnownNetworksPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-known-cellular-networks-page.
// eslint-disable-next-line no-var
var OSSettingsCellularNetworksListTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload; /* +
         'chromeos/internet_page/cellular_setup_dialog.html';*/
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kUpdatedCellularActivationUi']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH +
          '../cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js',
      'cellular_networks_list_test.js',
    ]);
  }
};

TEST_F('OSSettingsCellularNetworksListTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for rename esim dialog page.
// eslint-disable-next-line no-var
var OSSettingsEsimRenameDialogTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/internet_page/esim_rename_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH +
          '../cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js',
      'esim_rename_dialog_test.js',
    ]);
  }
};

TEST_F('OSSettingsEsimRenameDialogTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for remove esim profile dialog page.
// eslint-disable-next-line no-var
var OSSettingsEsimRemoveProfileDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/internet_page/esim_remove_profile_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH +
          '../cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js',
      'esim_remove_profile_dialog_test.js',
    ]);
  }
};

TEST_F('OSSettingsEsimRemoveProfileDialogTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-known-networks-page.
// eslint-disable-next-line no-var
var OSSettingsCellularSetupDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/internet_page/cellular_setup_dialog.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kUpdatedCellularActivationUi']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'cellular_setup_dialog_test.js',
    ]);
  }
};

TEST_F('OSSettingsCellularSetupDialogTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Kerberos section.
// eslint-disable-next-line no-var
var OSSettingsKerberosPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/kerberos_page/kerberos_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'kerberos_page_test.js',
      'test_kerberos_accounts_browser_proxy.js',
    ]);
  }
};

TEST_F('OSSettingsKerberosPageTest', 'MAYBE_AllJsTests', () => {
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

TEST_F('OSSettingsMainTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the OS Settings Search Box
// eslint-disable-next-line no-var
var OSSettingsSearchBoxBrowserTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'fake_settings_search_handler.js',
      'fake_user_action_recorder.js',
      'os_settings_search_box_test.js',
    ]);
  }
};

TEST_F('OSSettingsSearchBoxBrowserTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the side-nav menu.
// eslint-disable-next-line no-var
var OSSettingsMenuTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_settings_menu/os_settings_menu.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_settings_menu_test.js',
    ]);
  }
};

TEST_F('OSSettingsMenuTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage feature item.
// eslint-disable-next-line no-var
var OSSettingsMultideviceFeatureItemTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_feature_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'multidevice_feature_item_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceFeatureItemTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice Notification access dialog flow.
// eslint-disable-next-line no-var
var OSSettingsMultideviceNotificationAccessDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/multidevice_page/' +
        'multidevice_notification_access_setup_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_notification_access_setup_dialog_tests.js',
    ]);
  }
};

TEST_F(
    'OSSettingsMultideviceNotificationAccessDialogTest', 'MAYBE_AllJsTests',
    () => {
      mocha.run();
    });

// Test fixture for the multidevice settings subpage feature toggle.
// eslint-disable-next-line no-var
var OSSettingsMultideviceFeatureToggleTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_feature_toggle.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'multidevice_feature_toggle_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceFeatureToggleTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings page.
// eslint-disable-next-line no-var
var OSSettingsMultidevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultidevicePageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice Smart Lock subpage.
// eslint-disable-next-line no-var
var OSSettingsMultideviceSmartLockSubPageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_smartlock_subpage.html';
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

TEST_F('OSSettingsMultideviceSmartLockSubPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage.
// eslint-disable-next-line no-var
var OSSettingsMultideviceSubPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceSubPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice task continuation sync disabled link.
// eslint-disable-next-line no-var
var OSSettingsMultideviceTaskContinuationItemTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_task_continuation_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_sync_browser_proxy.js',
      'multidevice_task_continuation_item_tests.js',
    ]);
  }
};

TEST_F(
    'OSSettingsMultideviceTaskContinuationItemTest', 'MAYBE_AllJsTests', () => {
      mocha.run();
    });

// Test fixture for the multidevice task continuation sync disabled link.
// eslint-disable-next-line no-var
var OSSettingsMultideviceTaskContinuationDisabledLinkTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/multidevice_page/' +
        'multidevice_task_continuation_disabled_link.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'multidevice_task_continuation_disabled_link_tests.js',
    ]);
  }
};

TEST_F(
    'OSSettingsMultideviceTaskContinuationDisabledLinkTest', 'MAYBE_AllJsTests',
    () => {
      mocha.run();
    });

// Test fixture for the multidevice wifi sync disabled link.
// eslint-disable-next-line no-var
var OSSettingsMultideviceWifiSyncDisabledLinkTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_wifi_sync_disabled_link.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'multidevice_wifi_sync_disabled_link_tests.js',
    ]);
  }
};

TEST_F(
    'OSSettingsMultideviceWifiSyncDisabledLinkTest', 'MAYBE_AllJsTests', () => {
      mocha.run();
    });

// Test fixture for the multidevice wifi sync item.
// eslint-disable-next-line no-var
var OSSettingsMultideviceWifiSyncItemTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/multidevice_page/multidevice_wifi_sync_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_sync_browser_proxy.js',
      'multidevice_wifi_sync_item_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceWifiSyncItemTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});


// Test fixture for the Nearby Share receive dialog.
// eslint-disable-next-line no-var
var OSSettingsNearbyShareReceiveDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/nearby_share_page/nearby_share_receive_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../../nearby_share/shared/fake_nearby_contact_manager.js',
      '../../nearby_share/shared/fake_nearby_share_settings.js',
      '../../test_util.js',
      '../../test_browser_proxy.js',
      'fake_receive_manager.js',
      'nearby_share_receive_dialog_tests.js',
    ]);
  }
};

TEST_F('OSSettingsNearbyShareReceiveDialogTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Nearby Share settings subpage.
// eslint-disable-next-line no-var
var OSSettingsNearbyShareSubPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/nearby_share_page/nearby_share_subpage.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kNearbySharing']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      '../../nearby_share/shared/fake_nearby_share_settings.js',
      '../../nearby_share/shared/fake_nearby_contact_manager.js',
      'fake_receive_manager.js',
      'nearby_share_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsNearbyShareSubPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-detail-page.
// eslint-disable-next-line no-var
var OSSettingsNetworkProxySectionTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'network_proxy_section_test.js',
    ]);
  }
};

TEST_F('OSSettingsNetworkProxySectionTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-detail-page.
// eslint-disable-next-line no-var
var OSSettingsNetworkSummaryItemTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'network_summary_item_test.js',
    ]);
  }
};

TEST_F('OSSettingsNetworkSummaryItemTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsNetworkSummaryTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'network_summary_test.js',
    ]);
  }
};

TEST_F('OSSettingsNetworkSummaryTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsTetherConnectionDialogTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'tether_connection_dialog_test.js',
    ]);
  }
};

TEST_F('OSSettingsTetherConnectionDialogTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageAccountManagerTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }

  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_people_page/account_manager.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'people_page_account_manager_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageAccountManagerTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageAccountManagerTestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsPeoplePageAccountManagerTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsPeoplePageAccountManagerTestWithAccountManagementFlowsV2Enabled',
    'MAYBE_AllJsTests', () => {
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

TEST_F('OSSettingsPeoplePageChangePictureTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageKerberosAccountsTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_people_page/kerberos_accounts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'people_page_kerberos_accounts_test.js',
      'test_kerberos_accounts_browser_proxy.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageKerberosAccountsTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageLockScreenTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_people_page/lock_screen.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kQuickUnlockPinAutosubmit']};
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

TEST_F('OSSettingsPeoplePageLockScreenTest', 'MAYBE_AllJsTests', () => {
  settings_people_page_quick_unlock.registerLockScreenTests();
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageQuickUnlockAuthenticateTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_people_page/lock_screen_password_prompt_dialog.html';
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

TEST_F(
    'OSSettingsPeoplePageQuickUnlockAuthenticateTest', 'MAYBE_AllJsTests',
    () => {
      settings_people_page_quick_unlock.registerAuthenticateTests();
      mocha.run();
    });

// eslint-disable-next-line no-var
var OSSettingsPeoplePageSetupPinDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_people_page/setup_pin_dialog.html';
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

TEST_F('OSSettingsPeoplePageSetupPinDialogTest', 'MAYBE_AllJsTests', () => {
  settings_people_page_quick_unlock.registerSetupPinDialogTests();
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePagePinAutosubmitDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_people_page/pin_autosubmit_dialog.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kQuickUnlockPinAutosubmit']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      'fake_quick_unlock_private.js',
      'quick_unlock_authenticate_browsertest_chromeos.js'
    ]);
  }
};

TEST_F(
    'OSSettingsPeoplePagePinAutosubmitDialogTest', 'MAYBE_AllJsTests', () => {
      settings_people_page_quick_unlock.registerAutosubmitDialogTests();
      mocha.run();
    });

// eslint-disable-next-line no-var
var OSSettingsPeoplePageSyncControlsTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_people_page/os_sync_controls.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettingsSync']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js', 'os_sync_controls_test.js'
    ]);
  }
};

TEST_F('OSSettingsPeoplePageSyncControlsTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the People section.
// eslint-disable-next-line no-var
var OSSettingsPeoplePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }

  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_people_page/os_people_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'sync_test_util.js',
      BROWSER_SETTINGS_PATH + 'test_profile_info_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_sync_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH +
          '../settings/chromeos/fake_quick_unlock_private.js',
      'os_people_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the People section with `kAccountManagementFlowsV2` flag enabled.
// eslint-disable-next-line no-var
var OSSettingsPeoplePageTestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsPeoplePageTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsPeoplePageTestWithAccountManagementFlowsV2Enabled',
    'MAYBE_AllJsTests', () => {
      mocha.run();
    });

// Tests for the Privacy section.
// eslint-disable-next-line no-var
var OSSettingsPrivacyPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_privacy_page/os_privacy_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH +
          '../settings/chromeos/fake_quick_unlock_private.js',
      'os_privacy_page_test.js',
    ]);
  }
};

// Flaky in debug. See MAYBE_AllBuilds definition above.
TEST_F('OSSettingsPrivacyPageTest', 'MAYBE_AllBuilds', () => {
  mocha.grep('/^(?!PrivacePageTest_OfficialBuild).*$/').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
// Flaky in debug. See MAYBE_OfficialBuild definition above.
TEST_F('OSSettingsPrivacyPageTest', 'MAYBE_OfficialBuild', () => {
  mocha.grep('PrivacePageTest_OfficialBuild').run();
});
GEN('#endif');

// Tests for the People section with `kAccountManagementFlowsV2` flag enabled.
// eslint-disable-next-line no-var
var OSSettingsPrivacyPageTestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsPrivacyPageTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

// Flaky in debug. See MAYBE_AllBuilds definition above.
TEST_F(
    'OSSettingsPrivacyPageTestWithAccountManagementFlowsV2Enabled',
    'MAYBE_AllBuilds', () => {
      mocha.grep('/^(?!PrivacePageTest_OfficialBuild).*$/').run();
    });

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
// Flaky in debug. See MAYBE_OfficialBuild definition above.
TEST_F(
    'OSSettingsPrivacyPageTestWithAccountManagementFlowsV2Enabled',
    'MAYBE_OfficialBuild', () => {
      mocha.grep('PrivacePageTest_OfficialBuild').run();
    });
GEN('#endif');

// Tests for the Files section.
// eslint-disable-next-line no-var
var OSSettingsFilesPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_files_page/os_files_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_files_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsFilesPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsParentalControlsPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/parental_controls_page/parental_controls_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'parental_controls_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsParentalControlsPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Personalization section.
// eslint-disable-next-line no-var
var OSSettingsPersonalizationPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/personalization_page/personalization_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAmbientModeFeature']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + 'chromeos/test_wallpaper_browser_proxy.js',
      'personalization_page_test.js',
    ]);
  }
};

// Flaky in debug. See MAYBE_AllBuilds definition above.
TEST_F('OSSettingsPersonalizationPageTest', 'MAYBE_AllBuilds', () => {
  mocha.grep('/^(?!PersonalizationTest_ReleaseOnly).*$/').run();
});

// This V3 test fails in debug mode, so run only on release builds. Suspected to
// be a synchronization issue as this test passes if run by itself.
// https://crbug.com/1122752
GEN('#if defined(NDEBUG) && BUILDFLAG(OPTIMIZE_WEBUI)');
TEST_F(
    'OSSettingsPersonalizationPageTest', 'PersonalizationTest_ReleaseOnly',
    () => {
      mocha.grep('PersonalizationTest_ReleaseOnly').run();
    });
GEN('#endif');

// Tests for the OS Printing page.
// eslint-disable-next-line no-var
var OSSettingsPrintingPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_printing_page/os_printing_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_printing_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrintingPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS printer entry.
// eslint-disable-next-line no-var
var OSSettingsCupsPrinterEntryTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_printing_page/cups_printers_entry.html';
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

TEST_F('OSSettingsCupsPrinterEntryTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS printer landing page.
// eslint-disable-next-line no-var
var OSSettingsCupsPrinterLandingPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_printing_page/cups_printers.html';
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
      'cups_printer_test_utils.js',
      'test_cups_printers_browser_proxy.js',
      'cups_printer_landing_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsCupsPrinterLandingPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS page, primarily the (edit/add) dialogs.
// eslint-disable-next-line no-var
var OSSettingsCupsPrinterPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_printing_page/cups_printers.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      'cups_printer_test_utils.js',
      'test_cups_printers_browser_proxy.js',
      'cups_printer_test_utils.js',
      'cups_printer_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsCupsPrinterPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// TODO(crbug/1109431): Remove this test once migration is complete.
// eslint-disable-next-line no-var
var OSSettingsLanguagesPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_languages_page/os_languages_page.html';
  }

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kLanguageSettingsUpdate']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'fake_input_method_private.js',
      BROWSER_SETTINGS_PATH + 'fake_language_settings_private.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_languages_page_tests.js',
      'test_os_languages_browser_proxy.js',
      'test_os_languages_metrics_proxy.js',
    ]);
  }
};

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_LanguageMenu DISABLED_LanguageMenu');
GEN('#else');
GEN('#define MAYBE_LanguageMenu LanguageMenu');
GEN('#endif');
TEST_F('OSSettingsLanguagesPageTest', 'MAYBE_LanguageMenu', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.LanguageMenu)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_InputMethods DISABLED_InputMethods');
GEN('#else');
GEN('#define MAYBE_InputMethods InputMethods');
GEN('#endif');
TEST_F('OSSettingsLanguagesPageTest', 'MAYBE_InputMethods', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.InputMethods)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_RecordMetrics DISABLED_RecordMetrics');
GEN('#else');
GEN('#define MAYBE_RecordMetrics RecordMetrics');
GEN('#endif');
TEST_F('OSSettingsLanguagesPageTest', 'MAYBE_RecordMetrics', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.RecordMetrics)).run();
});

// Flaky in debug. https://crbug.com/1003483
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_DetailsPage DISABLED_DetailsPage');
GEN('#else');
GEN('#define MAYBE_DetailsPage DetailsPage');
GEN('#endif');
TEST_F('OSSettingsLanguagesPageTest', 'MAYBE_DetailsPage', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.DetailsPage)).run();
});

// eslint-disable-next-line no-var
var OSSettingsLanguagesPageV2Test = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_languages_page/os_languages_page_v2.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'fake_input_method_private.js',
      BROWSER_SETTINGS_PATH + 'fake_language_settings_private.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_languages_page_v2_tests.js',
      'test_os_languages_browser_proxy.js',
      'test_os_languages_metrics_proxy.js',
      'test_os_lifetime_browser_proxy.js',
    ]);
  }
};

TEST_F('OSSettingsLanguagesPageV2Test', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsSmartInputsPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_languages_page/smart_inputs_page.html';
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'smart_inputs_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsSmartInputsPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsInputMethodOptionsPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_language_page/input_method_options_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'input_method_options_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsInputMethodOptionsPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsInputPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_languages_page/input_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + 'fake_input_method_private.js',
      BROWSER_SETTINGS_PATH + 'fake_language_settings_private.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      'input_page_test.js',
      'test_os_languages_browser_proxy.js',
      'test_os_languages_metrics_proxy.js',
    ]);
  }
};

TEST_F('OSSettingsInputPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Reset section.
// eslint-disable-next-line no-var
var OSSettingsResetPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_reset_page/os_reset_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'test_os_reset_browser_proxy.js',
      'os_reset_page_test.js',
      'test_os_lifetime_browser_proxy.js',
    ]);
  }
};

// eslint-disable-next-line no-var
var OSSettingsEditDictionaryPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_language_page/os_edit_dictionary_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'fake_input_method_private.js',
      BROWSER_SETTINGS_PATH + 'fake_language_settings_private.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      'os_edit_dictionary_page_test.js',
      'test_os_languages_browser_proxy.js',
    ]);
  }
};

TEST_F('OSSettingsEditDictionaryPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

TEST_F('OSSettingsResetPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the "Search and assistant" page.
// eslint-disable-next-line no-var
var OSSettingsOsSearchPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_search_page/os_search_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + 'test_search_engines_browser_proxy.js',
      'os_search_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsOsSearchPageTest', 'MAYBE_AllJsTests', () => {
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

TEST_F('OSSettingsSmbPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the OS Accessibility page.
// eslint-disable-next-line no-var
var OSSettingsAccessibilityPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_a11y_page/os_a11y_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_a11y_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAccessibilityPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Manage Accessibility page.
// eslint-disable-next-line no-var
var OSSettingsManageAccessibilityPageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_a11y_page/manage_a11y_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'manage_accessibility_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsManageAccessibilityPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the OS Accessibility Text-to-Speech subpage.
// eslint-disable-next-line no-var
var OSSettingsTextToSpeechSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_a11y_page/tts_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'text_to_speech_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsTextToSpeechSubpageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});


// Test fixture for the Switch Access page.
// eslint-disable-next-line no-var
var OSSettingsSwitchAccessSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_a11y_page/switch_access_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'switch_access_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsSwitchAccessSubpageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Switch Access Action Assignment dialog.
// eslint-disable-next-line no-var
var OSSettingsSwitchAccessActionAssignmentDialogTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'switch_access_action_assignment_dialog_test.js',
    ]);
  }
};

TEST_F(
    'OSSettingsSwitchAccessActionAssignmentDialogTest', 'MAYBE_AllJsTests',
    () => {
      mocha.run();
    });

// Tests for the Date Time timezone selector
// eslint-disable-next-line no-var
var OSSettingsTimezoneSelectorTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/date_time_page/timezone_selector.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat(['timezone_selector_test.js']);
  }
};

TEST_F('OSSettingsTimezoneSelectorTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Date Time subpage
// eslint-disable-next-line no-var
var OSSettingsTimezoneSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/date_time_page/timezone_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'timezone_subpage_test.js',
    ]);
  }
};

TEST_F('OSSettingsTimezoneSubpageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the TTS Subpage.
// eslint-disable-next-line no-var
var OSSettingsTtsSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_a11y_page/tts_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'tts_subpage_test.js',
    ]);
  }
};

TEST_F('OSSettingsTtsSubpageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});
