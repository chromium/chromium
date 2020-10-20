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
GEN('#include "chrome/browser/browser_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ui/display/display_features.h"');

// Generic test fixture for CrOS Polymer Settings elements to be overridden by
// individual element tests.
const OSSettingsBrowserTest = class extends PolymerTest {
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
      'localized_link_test.js',
    ]);
  }
};

TEST_F('SettingsLocalizedLinkTest', 'AllJsTests', () => {
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
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      'test_about_page_browser_proxy_chromeos.js',
      'os_about_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAboutPageTest', 'AllBuilds', () => {
  mocha.grep('/^(?!AboutPageTest_OfficialBuild).*$/').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('OSSettingsAboutPageTest', 'AboutPage_OfficialBuild', () => {
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

TEST_F('OSSettingsSliderTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsTextAreaTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsToggleButtonTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsPrefUtilTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsPrefsTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsAddUsersTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsAmbientModePageTest', 'AllJsTests', () => {
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
      'ambient_mode_photos_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsAmbientModePhotosPageTest', 'AllJsTests', () => {
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
      BROWSER_SETTINGS_PATH + '../test_util.js',
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

TEST_F('OSSettingsAppManagementPwaDetailViewTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsAppManagementArcDetailViewTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsAppManagementPluginVmDetailViewTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Plugin VM shared paths page.
// eslint-disable-next-line no-var
var OSSettingsAppManagementPluginVmSharedPathsTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'app_management/plugin_vm_page/plugin_vm_shared_paths.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'app_management/plugin_vm_shared_paths_test.js',
    ]);
  }
};

TEST_F('OSSettingsAppManagementPluginVmSharedPathsTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Plugin VM shared USB devices page.
// eslint-disable-next-line no-var
var OSSettingsAppManagementPluginVmSharedUsbDevicesTest =
    class extends OSSettingsAppManagementBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'app_management/plugin_vm_page/plugin_vm_shared_usb_devices.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'app_management/plugin_vm_shared_usb_devices_test.js',
    ]);
  }
};

TEST_F(
    'OSSettingsAppManagementPluginVmSharedUsbDevicesTest', 'AllJsTests', () => {
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
TEST_F('OSSettingsBluetoothPageTest', 'AllJsTests', () => {
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
      'crostini_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsCrostiniPageTest', 'MainPage', function() {
  mocha.grep('\\bMainPage\\b').run();
});

TEST_F('OSSettingsCrostiniPageTest', 'SubPageDefault', function() {
  mocha.grep('\\bSubPageDefault\\b').run();
});

TEST_F('OSSettingsCrostiniPageTest', 'SubPagePortForwarding', function() {
  mocha.grep('\\bSubPagePortForwarding\\b').run();
});

TEST_F('OSSettingsCrostiniPageTest', 'DiskResize', function() {
  mocha.grep('\\bDiskResize\\b').run();
});

TEST_F('OSSettingsCrostiniPageTest', 'SubPageSharedPaths', function() {
  mocha.grep('\\bSubPageSharedPaths\\b').run();
});

TEST_F('OSSettingsCrostiniPageTest', 'SubPageSharedUsbDevices', function() {
  mocha.grep('\\bSubPageSharedUsbDevices\\b').run();
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

TEST_F('OSSettingsDateTimePageTest', 'AllJsTests', () => {
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
        'display::features::kListAllDisplayModes'
      ]
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

TEST_F('OSSettingsDevicePageTest', 'StorageTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Storage)).run();
});

TEST_F('OSSettingsDevicePageTest', 'StylusTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Stylus)).run();
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

// TODO(https://crbug.com/1121139): Re-enable flaky test.
TEST_F('OSSettingsFingerprintListTest', 'DISABLED_AllJsTests', () => {
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
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'google_assistant_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsGoogleAssistantPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsInternetConfigTest', 'All', () => {
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

TEST_F('OSSettingsInternetDetailPageTest', 'All', () => {
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
      'internet_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetSubpageTest', 'InternetSubpage', () => {
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

TEST_F(
    'OSSettingsInternetKnownNetworksPageTest', 'InternetKnownNetworksPage',
    () => {
      mocha.run();
    });

// Test fixture for settings-internet-known-networks-page.
// eslint-disable-next-line no-var
var OSSettingsCellularSetupDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload;/* +
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
      'cellular_setup_dialog_test.js',
    ]);
  }
};

TEST_F(
    'OSSettingsCellularSetupDialogTest', 'CellularSetupDialog',
    () => {
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

TEST_F('OSSettingsSearchBoxBrowserTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsMenuTest', 'AllJsTests', () => {
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
      'multidevice_feature_item_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceFeatureItemTest', 'AllJsTests', () => {
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
    'OSSettingsMultideviceNotificationAccessDialogTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsMultideviceFeatureToggleTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsMultidevicePageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsMultideviceSmartLockSubPageTest', 'AllJsTests', () => {
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
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceSubPageTest', 'AllJsTests', () => {
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
      '../../test_util.js',
      '../../test_browser_proxy.js',
      'fake_receive_manager.js',
      'nearby_share_receive_dialog_tests.js',
    ]);
  }
};

TEST_F('OSSettingsNearbyShareReceiveDialogTest', 'AllJsTests', () => {
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
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + '../test_util.js',
      '../../nearby_share/shared/fake_nearby_share_settings.js',
      '../../nearby_share/shared/fake_nearby_contact_manager.js',
      'fake_receive_manager.js',
      'nearby_share_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsNearbyShareSubPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsNetworkProxySectionTest', 'All', () => {
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

TEST_F('OSSettingsNetworkSummaryItemTest', 'All', () => {
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

TEST_F('OSSettingsNetworkSummaryTest', 'All', () => {
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

TEST_F('OSSettingsTetherConnectionDialogTest', 'All', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageAccountManagerTest =
    class extends OSSettingsBrowserTest {
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
    return super.browsePreload +
        'chromeos/os_people_page/kerberos_accounts.html';
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

TEST_F('OSSettingsPeoplePageQuickUnlockAuthenticateTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsPeoplePageSetupPinDialogTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsPeoplePagePinAutosubmitDialogTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsPeoplePageSyncControlsTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the People section.
// eslint-disable-next-line no-var
var OSSettingsPeoplePageTest = class extends OSSettingsBrowserTest {
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

TEST_F('OSSettingsPeoplePageTest', 'AllJsTests', () => {
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
      BROWSER_SETTINGS_PATH + '../test_util.js',
      'os_privacy_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPrivacyPageTest', 'AllBuilds', () => {
  mocha.grep('/^(?!PrivacePageTest_OfficialBuild).*$/').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('OSSettingsPrivacyPageTest', 'PrivacePage_OfficialBuild', () => {
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

TEST_F('OSSettingsFilesPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsParentalControlsPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsPersonalizationPageTest', 'AllBuilds', () => {
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

TEST_F('OSSettingsPrintingPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsCupsPrinterEntryTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsCupsPrinterLandingPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsCupsPrinterPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsLanguagesPageTest', 'LanguageMenu', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.LanguageMenu)).run();
});

TEST_F('OSSettingsLanguagesPageTest', 'InputMethods', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.InputMethods)).run();
});

TEST_F('OSSettingsLanguagesPageTest', 'RecordMetrics', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.RecordMetrics)).run();
});

TEST_F('OSSettingsLanguagesPageTest', 'DetailsPage', function() {
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

TEST_F('OSSettingsLanguagesPageV2Test', 'AllJsTests', () => {
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

TEST_F('OSSettingsSmartInputsPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsInputMethodOptionsPageTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsInputPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_language_page/input_page.html';
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

TEST_F('OSSettingsInputPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsEditDictionaryPageTest', 'AllJsTests', () => {
  mocha.run();
});

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
      BROWSER_SETTINGS_PATH + '../test_util.js',
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

TEST_F('OSSettingsAccessibilityPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsManageAccessibilityPageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsTextToSpeechSubpageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsSwitchAccessSubpageTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsTimezoneSelectorTest', 'AllJsTests', () => {
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

TEST_F('OSSettingsTimezoneSubpageTest', 'AllJsTests', () => {
  mocha.run();
});

GEN('#endif  // defined(NDEBUG)');
