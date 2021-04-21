// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "ash/public/cpp/ash_features.h"');
GEN('#include "chrome/common/buildflags.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var OSSettingsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kEnableHostnameSetting',
        'chromeos::features::kUpdatedCellularActivationUi',
        'features::kCrostini',
      ],
    };
  }
};

// TODO(crbug/1109431): Remove this test once migration is complete.
// eslint-disable-next-line no-var
var OSSettingsOsLanguagesPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_languages_page_tests.m.js';
  }
};

TEST_F('OSSettingsOsLanguagesPageV3Test', 'All', () => mocha.run());

// eslint-disable-next-line no-var
var OSSettingsDevicePageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/device_page_tests.m.js';
  }
};

TEST_F(
    'OSSettingsDevicePageV3Test', 'All',
    () => mocha.grep('/^((?!arrow_key_arrangement_disabled).)*$/').run());

// eslint-disable-next-line no-var
var OSSettingsDevicePageKeyboardArrangementDisabledV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/device_page_tests.m.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled,
      disabled: ['ash::features::kKeyboardBasedDisplayArrangementInSettings']
    };
  }
};

TEST_F(
    'OSSettingsDevicePageKeyboardArrangementDisabledV3Test', 'All',
    () => mocha.grep('/.*arrow_key_arrangement_disabled.*/').run());

// TODO(crbug/1146900): Move this test down to the bottom where the rest are
// once the FullRestore flag is enabled by default.
// eslint-disable-next-line no-var
var OSSettingsOnStartupPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/on_startup_page_tests.m.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled,
      disabled: ['ash::features::kFullRestore']
    };
  }
};

TEST_F('OSSettingsOnStartupPageV3Test', 'All', () => mocha.run());

// eslint-disable-next-line no-var
var OSSettingsNearbyShareSubPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/nearby_share_subpage_tests.m.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(['features::kNearbySharing'])
    };
  }
};

TEST_F('OSSettingsNearbyShareSubPageV3Test', 'All', () => mocha.run());

// eslint-disable-next-line no-var
var OSSettingsPrivacyPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_privacy_page_test.m.js';
  }

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

// eslint-disable-next-line no-var
var OSSettingsPeoplePageAccountManagerV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/people_page_account_manager_test.m.js';
  }

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F('OSSettingsPeoplePageAccountManagerV3Test', 'All', () => mocha.run());

// eslint-disable-next-line no-var
var OSSettingsPeoplePageAccountManagerV3TestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsPeoplePageAccountManagerV3Test {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsPeoplePageAccountManagerV3TestWithAccountManagementFlowsV2Enabled',
    'All', () => mocha.run());

TEST_F('OSSettingsPrivacyPageV3Test', 'AllBuilds', () => {
  mocha.grep('/^(?!PrivacePageTest_OfficialBuild).*$/').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('OSSettingsPrivacyPageV3Test', 'PrivacePage_OfficialBuild', () => {
  mocha.grep('PrivacePageTest_OfficialBuild').run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var OSSettingsPrivacyPageV3TestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsPrivacyPageV3Test {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsPrivacyPageV3TestWithAccountManagementFlowsV2Enabled',
    'AllBuilds', () => {
      mocha.grep('/^(?!PrivacePageTest_OfficialBuild).*$/').run();
    });

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F(
    'OSSettingsPrivacyPageV3TestWithAccountManagementFlowsV2Enabled',
    'PrivacePage_OfficialBuild', () => {
      mocha.grep('PrivacePageTest_OfficialBuild').run();
    });
GEN('#endif');

// eslint-disable-next-line no-var
var OSSettingsLockScreenPageV3Test = class extends OSSettingsPrivacyPageV3Test {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/lock_screen_tests.m.js';
  }

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F('OSSettingsLockScreenPageV3Test', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsLockScreenPageV3TestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsLockScreenPageV3Test {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsLockScreenPageV3TestWithAccountManagementFlowsV2Enabled',
    'AllJsTests', () => {
      mocha.run();
    });

// eslint-disable-next-line no-var
var OSSettingsUserPageV3TestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsLockScreenPageV3Test {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'OSSettingsUserPageV3TestWithAccountManagementFlowsV2Enabled', 'AllJsTests',
    () => {
      mocha.run();
    });

// eslint-disable-next-line no-var
var OSSettingsPeoplePageOsSyncV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_sync_controls_test.m.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(
          ['chromeos::features::kSplitSettingsSync']),
    };
  }
};

TEST_F('OSSettingsPeoplePageOsSyncV3Test', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_people_page_test.m.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled,
      disabled: ['chromeos::features::kAccountManagementFlowsV2'],
    };
  }
};

TEST_F('OSSettingsPeoplePageV3Test', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageV3TestWithAccountManagementFlowsV2Enabled =
    class extends OSSettingsPeoplePageV3Test {
  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(
          ['chromeos::features::kAccountManagementFlowsV2']),
      disabled: [],
    };
  }
};

TEST_F(
    'OSSettingsPeoplePageV3TestWithAccountManagementFlowsV2Enabled',
    'AllJsTests', () => {
      mocha.run();
    });

[['AccessibilityPage', 'os_a11y_page_tests.m.js'],
 ['AboutPage', 'os_about_page_tests.m.js'],
 ['AccountsPage', 'add_users_tests.m.js'],
 ['AmbientModePage', 'ambient_mode_page_test.m.js'],
 ['AmbientModePhotosPage', 'ambient_mode_photos_page_test.m.js'],
 ['AppsPage', 'apps_page_test.m.js'],
 ['AppManagementAppDetailView', 'app_detail_view_test.m.js'],
 ['AppManagementAppItem', 'app_item_test.m.js'],
 ['AppManagementArcDetailView', 'arc_detail_view_test.m.js'],
 ['AppManagementChromeAppDetailView', 'chrome_app_detail_view_test.m.js'],
 ['AppManagementDomSwitch', 'dom_switch_test.m.js'],
 ['AppManagementMainView', 'main_view_test.m.js'],
 ['AppManagementManagedApp', 'managed_apps_test.m.js'],
 ['AppManagementPage', 'app_management_page_tests.m.js'],
 ['AppManagementPermissionItem', 'permission_item_test.m.js'],
 ['AppManagementPinToShelfItem', 'pin_to_shelf_item_test.m.js'],
 ['AppManagementPluginVmDetailView', 'plugin_vm_detail_view_test.m.js'],
 ['AppManagementPwaDetailView', 'pwa_detail_view_test.m.js'],
 ['AppManagementReducers', 'reducers_test.m.js'],
 ['AppManagementToggleRow', 'toggle_row_test.m.js'],
 ['AppManagementUninstallButton', 'uninstall_button_test.m.js'],
 ['BluetoothPage', 'bluetooth_page_tests.m.js'],
 ['CellularBanner', 'cellular_banner_test.m.js'],
 ['CellularNetworksList', 'cellular_networks_list_test.m.js'],
 ['CellularSetupDialog', 'cellular_setup_dialog_test.m.js'],
 ['CrostiniPage', 'crostini_page_test.m.js'],
 ['CupsPrinterEntry', 'cups_printer_entry_tests.m.js'],
 ['CupsPrinterLandingPage', 'cups_printer_landing_page_tests.m.js'],
 ['CupsPrinterPage', 'cups_printer_page_tests.m.js'],
 ['DateTimePage', 'date_time_page_tests.m.js'],
 ['EsimInstallErrorDialog', 'esim_install_error_dialog_test.m.js'],
 ['EsimRemoveProfileDialog', 'esim_remove_profile_dialog_test.m.js'],
 ['EsimRenameDialog', 'esim_rename_dialog_test.m.js'],
 ['FilesPage', 'os_files_page_test.m.js'],
 ['FingerprintPage', 'fingerprint_browsertest_chromeos.m.js'],
 ['GoogleAssistantPage', 'google_assistant_page_test.m.js'],
 ['GuestOsSharedPaths', 'guest_os_shared_paths_test.m.js'],
 ['GuestOsSharedUsbDevices', 'guest_os_shared_usb_devices_test.m.js'],
 ['InputMethodOptionPage', 'input_method_options_page_test.m.js'],
 ['InputPage', 'input_page_test.m.js'],
 ['InternetConfig', 'internet_config_test.m.js'],
 ['InternetDetailMenu', 'internet_detail_menu_test.m.js'],
 ['InternetDetailPage', 'internet_detail_page_tests.m.js'],
 ['InternetKnownNetworksPage', 'internet_known_networks_page_tests.m.js'],
 ['InternetSubpage', 'internet_subpage_tests.m.js'],
 ['InternetPage', 'internet_page_tests.m.js'],
 ['KerberosAccounts', 'kerberos_accounts_test.m.js'],
 ['KerberosPage', 'kerberos_page_test.m.js'],
 ['LocalizedLink', 'localized_link_test.m.js'],
 ['ManageAccessibilityPage', 'manage_accessibility_page_tests.m.js'],
 ['MultideviceFeatureItem', 'multidevice_feature_item_tests.m.js'],
 ['MultideviceFeatureToggle', 'multidevice_feature_toggle_tests.m.js'],
 [
   'MultideviceNotificationAccessSetupDialog',
   'multidevice_notification_access_setup_dialog_tests.m.js'
 ],
 ['MultidevicePage', 'multidevice_page_tests.m.js'],
 ['MultideviceSmartLockSubPage', 'multidevice_smartlock_subpage_test.m.js'],
 ['MultideviceSubPage', 'multidevice_subpage_tests.m.js'],
 [
   'MultideviceTaskContinuationItem',
   'multidevice_task_continuation_item_tests.m.js'
 ],
 [
   'MultideviceTaskContinuationDisabledLink',
   'multidevice_task_continuation_disabled_link_tests.m.js'
 ],
 [
   'MultideviceWifiSyncDisabledLink',
   'multidevice_wifi_sync_disabled_link_tests.m.js'
 ],
 ['MultideviceWifiSyncItem', 'multidevice_wifi_sync_item_tests.m.js'],
 ['NetworkProxySection', 'network_proxy_section_test.m.js'],
 ['NetworkSummary', 'network_summary_test.m.js'],
 ['NetworkSummaryItem', 'network_summary_item_test.m.js'],
 ['OsEditDictionaryPage', 'os_edit_dictionary_page_test.m.js'],
 ['OsLanguagesPageV2', 'os_languages_page_v2_tests.m.js'],
 ['OsSettingsUi', 'os_settings_ui_test.m.js'],
 ['OsSettingsUi2', 'os_settings_ui_test_2.m.js'],
 ['OsSettingsMain', 'os_settings_main_test.m.js'],
 ['OsSearchPage', 'os_search_page_test.m.js'],
 ['OsSettingsSearchBox', 'os_settings_search_box_test.m.js'],
 ['OSSettingsMenu', 'os_settings_menu_test.m.js'],
 ['OsSettingsPage', 'os_settings_page_test.m.js'],
 ['NearbyShareConfirmPage', 'nearby_share_confirm_page_test.m.js'],
 ['NearbyShareReceiveDialog', 'nearby_share_receive_dialog_tests.m.js'],
 ['ParentalControlsPage', 'parental_controls_page_test.m.js'],
 ['PeoplePageChangePicture', 'people_page_change_picture_test.m.js'],
 [
   'PeoplePageQuickUnlock',
   'quick_unlock_authenticate_browsertest_chromeos.m.js'
 ],
 ['PersonalizationPage', 'personalization_page_test.m.js'],
 ['PrintingPage', 'os_printing_page_tests.m.js'],
 ['ResetPage', 'os_reset_page_test.m.js'],
 ['SmartInputsPage', 'smart_inputs_page_test.m.js'],
 ['SmbPage', 'smb_shares_page_tests.m.js'],
 [
   'SwitchAccessActionAssignmentDialog',
   'switch_access_action_assignment_dialog_test.m.js'
 ],
 ['SwitchAccessSubpage', 'switch_access_subpage_tests.m.js'],
 ['TetherConnectionDialog', 'tether_connection_dialog_test.m.js'],
 ['TextToSpeechSubpage', 'text_to_speech_subpage_tests.m.js'],
 ['TimezoneSelector', 'timezone_selector_test.m.js'],
 ['TimezoneSubpage', 'timezone_subpage_test.m.js'],
 ['TtsSubpage', 'tts_subpage_test.m.js'],
 ['UserPage', 'user_page_tests.m.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `OSSettings${testName}V3Test`;
  this[className] = class extends OSSettingsV3BrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/${module}`;
    }
  };

  // AboutPage has a test suite that can only succeed on official builds where
  // the is_chrome_branded build flag is enabled.
  if (testName === 'AboutPage') {
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!AboutPageTest_OfficialBuild).*$/').run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('AboutPageTest_OfficialBuild').run();
    });
    GEN('#endif');
  } else if (testName === 'PrivacyPage') {
    // PrivacyPage has a test suite that can only succeed on official builds
    // where the is_chrome_branded build flag is enabled.
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!PrivacePageTest_OfficialBuild).*$/').run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('PrivacePageTest_OfficialBuild').run();
    });
    GEN('#endif');
  } else {
    TEST_F(className, caseName || 'All', () => mocha.run());
  }
}
