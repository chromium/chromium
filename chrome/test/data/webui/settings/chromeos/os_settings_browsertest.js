// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/browser/ash/crostini/fake_crostini_features.h"');
GEN('#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"');
GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "chrome/common/buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "components/app_restore/features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ui/accessibility/accessibility_features.h"');
GEN('#include "ui/base/ui_base_features.h"');

/** Test fixture for shared Polymer 3 elements. */
var OSSettingsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kEnableHostnameSetting',
        // TODO(b/217560706): Remove this explicit enabled flag when rollout
        // completed.
        'ash::features::kDiacriticsOnPhysicalKeyboardLongpress',
      ],
    };
  }
};

var OSSettingsDevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/device_page/device_page_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kInputDeviceSettingsSplit',
        'ash::features::kPeripheralCustomization',
      ],
    };
  }
};

// TODO(https://crbug.com/1422799): The test is flaky on ChromeOS debug.
TEST_F_WITH_PREAMBLE(
    `
#if !defined(NDEBUG)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
#endif
    `,
    'OSSettingsDevicePageTest', 'MAYBE_All',
    () => mocha.grep('/^((?!arrow_key_arrangement_disabled).)*$/').run());

// TODO(crbug.com/1234871) Move this test back into the list of tests below once
// Fast pair is launched.
var OSSettingsOsBluetoothPageOsBluetoothDevicesSubpageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_bluetooth_page/os_bluetooth_devices_subpage_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kFastPair',
        'ash::features::kFastPairSavedDevices',
        'ash::features::kFastPairSoftwareScanning',
      ]),
    };
  }
};

TEST_F(
    'OSSettingsOsBluetoothPageOsBluetoothDevicesSubpageTest', 'AllJsTests',
    () => {
      mocha.run();
    });

// TODO (b/238647706) Move this test back into the list of tests below once
// Fast pair is launched.
var OSSettingsOsBluetoothPageOsBluetoothSavedDevicesSubpageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_bluetooth_page/os_bluetooth_saved_devices_subpage_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kFastPair',
        'ash::features::kFastPairSavedDevices',
        'ash::features::kFastPairSoftwareScanning',
      ]),
    };
  }
};

// TODO (b/238647706) Move this test back into the list of tests below once
// Fast pair is launched.
TEST_F(
    'OSSettingsOsBluetoothPageOsBluetoothSavedDevicesSubpageTest', 'AllJsTests',
    () => {
      mocha.run();
    });

// TODO(crbug.com/1234871) Move this test back into the list of tests below once
// Fast pair is launched.
var OSSettingsOsBluetoothPageOsBluetoothSavedDevicesListTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_bluetooth_page/os_saved_devices_list_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kFastPair',
        'ash::features::kFastPairSavedDevices',
        'ash::features::kFastPairSoftwareScanning',
      ]),
    };
  }
};

TEST_F(
    'OSSettingsOsBluetoothPageOsBluetoothSavedDevicesListTest', 'AllJsTests',
    () => {
      mocha.run();
    });

function crostiniTestGenPreamble() {
  GEN('crostini::FakeCrostiniFeatures fake_crostini_features;');
  GEN('fake_crostini_features.SetAll(true);');
}

var OSSettingsCrostiniPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/crostini_page/crostini_page_test.js';
  }

  /** @override */
  testGenPreamble() {
    return crostiniTestGenPreamble();
  }
};

// TODO(crbug.com/1504815): This test is flaky.
TEST_F('OSSettingsCrostiniPageTest', 'DISABLED_AllJsTests', () => {
  mocha.run();
});

var OSSettingsCrostiniPageCrostiniSettingsCardTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/crostini_page/crostini_settings_card_test.js';
  }

  /** @override */
  testGenPreamble() {
    return crostiniTestGenPreamble();
  }
};

TEST_F('OSSettingsCrostiniPageCrostiniSettingsCardTest', 'AllJsTests', () => {
  mocha.run();
});

var OSSettingsCrostiniPageCrostiniExtraContainersSubpageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/crostini_page/crostini_extra_containers_subpage_test.js';
  }

  /** @override */
  testGenPreamble() {
    return crostiniTestGenPreamble();
  }
};

TEST_F(
    'OSSettingsCrostiniPageCrostiniExtraContainersSubpageTest', 'AllJsTests',
    () => {
      mocha.run();
    });

[['AboutPage', 'os_about_page_tests.js'],
 ['ApnDetailDialog', 'apn_detail_dialog_test.js'],
 // TODO(crbug.com/1497312): Enable the ApnSubpage test.
 // [
 //   'ApnSubpage', 'apn_subpage_tests.js',
 //   {enabled: ['ash::features::kApnRevamp']}
 // ],
 ['AppManagementFileHandlingItem', 'app_management/file_handling_item_test.js'],
 ['AppManagementManagedApps', 'app_management/managed_apps_test.js'],
 ['AppManagementToggleRow', 'app_management/toggle_row_test.js'],
 ['CellularNetworksList', 'cellular_networks_list_test.js'],
 ['CellularRoamingToggleButton', 'cellular_roaming_toggle_button_test.js'],
 ['DateTimePage', 'date_time_page/date_time_page_test.js'],
 [
   'DateTimePageDateTimeSettingsCard',
   'date_time_page/date_time_settings_card_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['DateTimePageTimezoneSelector', 'date_time_page/timezone_selector_test.js'],
 ['DateTimePageTimezoneSubpage', 'date_time_page/timezone_subpage_test.js'],
 ['DevicePageAudioPage', 'device_page/audio_page_test.js'],
 [
   'DevicePageCustomizeButtonRow', 'device_page/customize_button_row_test.js', {
     enabled: [
       'ash::features::kPeripheralCustomization',
       'ash::features::kInputDeviceSettingsSplit'
     ]
   }
 ],
 [
   'DevicePageCustomizeButtonsSubsection',
   'device_page/customize_buttons_subsection_test.js', {
     enabled: [
       'ash::features::kPeripheralCustomization',
       'ash::features::kInputDeviceSettingsSplit'
     ]
   }
 ],
 [
   'DevicePageCustomizeMouseButtonsSubpage',
   'device_page/customize_mouse_buttons_subpage_test.js', {
     enabled: [
       'ash::features::kPeripheralCustomization',
       'ash::features::kInputDeviceSettingsSplit'
     ]
   }
 ],
 [
   'DevicePageCustomizePenButtonsSubpage',
   'device_page/customize_pen_buttons_subpage_test.js', {
     enabled: [
       'ash::features::kPeripheralCustomization',
       'ash::features::kInputDeviceSettingsSplit'
     ]
   }
 ],
 [
   'DevicePageCustomizeTabletButtonsSubpage',
   'device_page/customize_tablet_buttons_subpage_test.js', {
     enabled: [
       'ash::features::kPeripheralCustomization',
       'ash::features::kInputDeviceSettingsSplit'
     ]
   }
 ],
 ['DevicePageDisplayPage', 'device_page/display_page_test.js'],
 [
   'DevicePageDragAndDropManager', 'device_page/drag_and_drop_manager_test.js',
   {
     enabled: [
       'ash::features::kPeripheralCustomization',
       'ash::features::kInputDeviceSettingsSplit'
     ]
   }
 ],
 [
   'DevicePageFakeCrosAudioConfig', 'device_page/fake_cros_audio_config_test.js'
 ],
 [
   'DevicePageFakeInputDeviceSettingsProvider',
   'device_page/fake_input_device_settings_provider_test.js'
 ],
 [
   'DevicePageInputDeviceMojoInterfaceProvider',
   'device_page/input_device_mojo_interface_provider_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePageDisplaySettingsMojoInterfaceProvider',
   'device_page/display_settings_mojo_interface_provider_test.js'
 ],
 [
   'DevicePageKeyCombinationInputDialog',
   'device_page/key_combination_input_dialog_test.js', {
     enabled: [
       'ash::features::kPeripheralCustomization',
       'ash::features::kInputDeviceSettingsSplit'
     ]
   }
 ],
 [
   'DevicePageKeyboardSixPackKeyRow',
   'device_page/keyboard_six_pack_key_row_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceKeyboard', 'device_page/per_device_keyboard_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceKeyboardRemapKeys',
   'device_page/per_device_keyboard_remap_keys_test.js', {
     enabled: [
       'ash::features::kInputDeviceSettingsSplit',
       'ash::features::kAltClickAndSixPackCustomization'
     ]
   }
 ],
 [
   'DevicePagePerDeviceKeyboardSubsection',
   'device_page/per_device_keyboard_subsection_test.js',
   {
     enabled: [
       'ash::features::kInputDeviceSettingsSplit',
       'ash::features::kAltClickAndSixPackCustomization'
     ]
   },
 ],
 [
   'DevicePagePerDeviceMouse', 'device_page/per_device_mouse_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceMouseSubsection',
   'device_page/per_device_mouse_subsection_test.js', {
     enabled: [
       'ash::features::kInputDeviceSettingsSplit',
       'ash::features::kPeripheralCustomization',
     ]
   }
 ],
 [
   'DevicePagePerDevicePointingStick',
   'device_page/per_device_pointing_stick_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceTouchpad',
   'device_page/per_device_touchpad_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']},
 ],
 [
   'DevicePagePerDeviceTouchpadSubsection',
   'device_page/per_device_touchpad_subsection_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDevicePointingStickSubsection',
   'device_page/per_device_pointing_stick_subsection_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']},
 ],
 [
   'DevicePageFKeyRow',
   'device_page/fkey_row_test.js',
   {
     enabled: [
       'ash::features::kInputDeviceSettingsSplit',
       'ash::features::kAltClickAndSixPackCustomization',
       'features::kSupportF11AndF12KeyShortcuts',
     ],
   },
 ],
 [
   'DevicePagePower',
   'device_page/power_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'DevicePagePowerRevamp',
   'device_page/power_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'DevicePagePrintingSettingsCard',
   'os_printing_page/printing_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'DevicePageRevamp',
   'device_page/device_page_revamp_test.js',
   {
     enabled: [
       'ash::features::kOsSettingsRevampWayfinding',
     ],
   },
 ],
 ['EsimRemoveProfileDialog', 'esim_remove_profile_dialog_test.js'],
 ['GuestOsSharedPaths', 'guest_os/guest_os_shared_paths_test.js'],
 ['GuestOsSharedUsbDevices', 'guest_os/guest_os_shared_usb_devices_test.js'],
 [
   'InternetPage', 'internet_page_tests.js', {
     enabled: [
       'ash::features::kApnRevamp',
       'ash::features::kHotspot',
       'ash::features::kPasspointSettings',
       'ash::features::kPasspointARCSupport',
     ]
   }
 ],
 [
   'InternetPageCellularSetupDialog',
   'internet_page/cellular_setup_dialog_test.js'
 ],
 [
   'InternetPageEsimInstallErrorDialog',
   'internet_page/esim_install_error_dialog_test.js'
 ],
 ['InternetPageEsimRenameDialog', 'internet_page/esim_rename_dialog_test.js'],
 [
   'InternetPageHotspotConfigDialog',
   'internet_page/hotspot_config_dialog_test.js',
   {enabled: ['ash::features::kHotspot']},
 ],
 [
   'InternetPageHotspotSubpage',
   'internet_page/hotspot_subpage_test.js',
   {enabled: ['ash::features::kHotspot']},
 ],
 [
   'InternetPageHotspotSummaryItem',
   'internet_page/hotspot_summary_item_test.js',
   {enabled: ['ash::features::kHotspot']},
 ],
 ['InternetPageInternetConfig', 'internet_page/internet_config_test.js'],
 [
   'InternetPageInternetDetailMenu',
   'internet_page/internet_detail_menu_test.js'
 ],
 [
   'InternetPageInternetDetailSubpage',
   'internet_page/internet_detail_subpage_tests.js', {
     enabled: [
       'ash::features::kApnRevamp',
       'ash::features::kPasspointARCSupport',
       'ash::features::kPasspointSettings',
     ]
   }
 ],
 [
   'InternetPageInternetKnownNetworksSubpage',
   'internet_page/internet_known_networks_subpage_test.js', {
     enabled: [
       'ash::features::kPasspointARCSupport',
       'ash::features::kPasspointSettings',
     ]
   }
 ],
 [
   'InternetPageInternetSubpageMenu',
   'internet_page/internet_subpage_menu_test.js'
 ],
 ['InternetPageInternetSubpage', 'internet_page/internet_subpage_test.js'],
 [
   'InternetPageNetworkAlwaysOnVpn',
   'internet_page/network_always_on_vpn_test.js'
 ],
 [
   'InternetPageNetworkDeviceInfoDialog',
   'internet_page/network_device_info_dialog_test.js'
 ],
 [
   'InternetPageNetworkProxySection',
   'internet_page/network_proxy_section_test.js'
 ],
 ['InternetPageNetworkSummary', 'internet_page/network_summary_test.js'],
 [
   'InternetPageNetworkSummaryItem',
   'internet_page/network_summary_item_test.js'
 ],
 [
   'InternetPagePasspointSubpage', 'internet_page/passpoint_subpage_test.js', {
     enabled: [
       'ash::features::kPasspointARCSupport',
       'ash::features::kPasspointSettings',
     ]
   }
 ],
 [
   'InternetPagePasspointRemoveDialog',
   'internet_page/passpoint_remove_dialog_test.js', {
     enabled: [
       'ash::features::kPasspointARCSupport',
       'ash::features::kPasspointSettings',
     ]
   }
 ],
 [
   'InternetPageSettingsTrafficCounters',
   'internet_page/settings_traffic_counters_test.js'
 ],
 [
   'InternetPageTetherConnectionDialog',
   'internet_page/tether_connection_dialog_test.js'
 ],
 ['KerberosPage', 'kerberos_page/kerberos_page_test.js'],
 [
   'KerberosPageKerberosAccountsSubpage',
   'kerberos_page/kerberos_accounts_subpage_test.js',
 ],
 [
   'KerberosPageKerberosAddAccountDialog',
   'kerberos_page/kerberos_add_account_dialog_test.js',
 ],
 [
   'KeyboardShortcutBanner',
   'keyboard_shortcut_banner/keyboard_shortcut_banner_test.js'
 ],
 ['LockScreenSubpage', 'lock_screen_subpage_test.js'],
 [
   'MainPageContainer',
   'main_page_container/main_page_container_test.js',
   {
     disabled: [
       'ash::features::kOsSettingsRevampWayfinding',
     ],
   },
 ],
 [
   'MainPageContainerRevamp',
   'main_page_container/main_page_container_test.js',
   {
     enabled: [
       'ash::features::kOsSettingsRevampWayfinding',
     ],
   },
 ],
 [
   'MainPageContainerPageDisplayer',
   'main_page_container/page_displayer_test.js',
 ],
 [
   'MainPageContainerRouteNavigation',
   'main_page_container/route_navigation_test.js',
   {
     enabled: [
       'ash::features::kOsSettingsRevampWayfinding',
     ],
   },
 ],
 ['MultidevicePage', 'multidevice_page/multidevice_page_test.js'],
 [
   'MultidevicePageMultideviceFeatureItem',
   'multidevice_page/multidevice_feature_item_test.js'
 ],
 [
   'MultidevicePageMultideviceFeatureToggle',
   'multidevice_page/multidevice_feature_toggle_test.js'
 ],
 [
   'MultidevicePageMultideviceNotificationAccessSetupDialog',
   'multidevice_page/multidevice_notification_access_setup_dialog_test.js',
 ],
 [
   'MultidevicePageMultidevicePermissionsSetupDialog',
   'multidevice_page/multidevice_permissions_setup_dialog_test.js',
 ],
 [
   'MultidevicePageMultideviceSmartlockItem',
   'multidevice_page/multidevice_smartlock_item_test.js'
 ],
 [
   'MultidevicePageMultideviceSubPage',
   'multidevice_page/multidevice_subpage_test.js'
 ],
 [
   'MultiDevicePageMultideviceCombinedSetupItem',
   'multidevice_page/multidevice_combined_setup_item_test.js'
 ],
 [
   'MultidevicePageMultideviceTaskContinuationDisabledLink',
   'multidevice_page/multidevice_task_continuation_disabled_link_test.js',
 ],
 [
   'MultidevicePageMultideviceTaskContinuationItem',
   'multidevice_page/multidevice_task_continuation_item_test.js',
 ],
 [
   'MultidevicePageMultideviceWifiSyncDisabledLink',
   'multidevice_page/multidevice_wifi_sync_disabled_link_test.js',
 ],
 [
   'MultidevicePageMultideviceWifiSyncItem',
   'multidevice_page/multidevice_wifi_sync_item_test.js'
 ],
 [
   'NearbySharePageNearbyShareConfirmPage',
   'nearby_share_page/nearby_share_confirm_page_test.js'
 ],
 [
   'NearbySharePageNearbyShareHighVisibilityPage',
   'nearby_share_page/nearby_share_high_visibility_page_test.js'
 ],
 [
   'NearbySharePageNearbyShareReceiveDialog',
   'nearby_share_page/nearby_share_receive_dialog_test.js'
 ],
 [
   'NearbySharePageNearbyShareSubpage',
   'nearby_share_page/nearby_share_subpage_test.js',
   {enabled: ['features::kNearbySharing']},
 ],
 ['OncMojoTest', 'onc_mojo_test.js'],
 [
   'OsA11yPage',
   'os_a11y_page/os_a11y_page_test.js',
   {enabled: ['features::kPdfOcr']},
 ],
 [
   'OsA11yPageAudioAndCaptionsPage',
   'os_a11y_page/audio_and_captions_page_test.js',
 ],
 [
   'OsA11yPageChromeVoxSubpage',
   'os_a11y_page/chromevox_subpage_test.js',
 ],
 [
   'OsA11yPageCursorAndTouchpadPage',
   'os_a11y_page/cursor_and_touchpad_page_test.js',
 ],
 [
   'OsA11yPageChangeDictationLocaleDialog',
   'os_a11y_page/change_dictation_locale_dialog_test.js',
 ],
 [
   'OsA11yPageDisplayAndMagnificationSubpage',
   'os_a11y_page/display_and_magnification_subpage_test.js',
 ],
 [
   'OsA11yPageKeyboardAndTextInputPage',
   'os_a11y_page/keyboard_and_text_input_page_test.js',
 ],
 [
   'OsA11yPageKioskMode',
   'os_a11y_page/os_a11y_page_kiosk_mode_test.js',
 ],
 [
   'OsA11yPageSelectToSpeakSubpage',
   'os_a11y_page/select_to_speak_subpage_test.js',
 ],
 [
   'OsA11yPageSwitchAccessActionAssignmentDialog',
   'os_a11y_page/switch_access_action_assignment_dialog_test.js',
 ],
 [
   'OsA11yPageSwitchAccessSetupGuideDialog',
   'os_a11y_page/switch_access_setup_guide_dialog_test.js'
 ],
 [
   'OsA11yPageSwitchAccessSubpage', 'os_a11y_page/switch_access_subpage_test.js'
 ],
 [
   'OsA11yPageTextToSpeechSubpage',
   'os_a11y_page/text_to_speech_subpage_test.js',
   {enabled: ['features::kPdfOcr']},
 ],
 [
   'OsA11yPageTtsVoiceSubpage',
   'os_a11y_page/tts_voice_subpage_test.js',
 ],
 [
   'OsAppsPageAppManagementPageAppDetailsItem',
   'os_apps_page/app_management_page/app_details_item_test.js',
   {enabled: ['features::kAppManagementAppDetails']},
 ],
 [
   'OsAppsPageAppManagementPageAppDetailView',
   'os_apps_page/app_management_page/app_detail_view_test.js'
 ],
 [
   'OsAppsPageAppManagementPageAppItem',
   'os_apps_page/app_management_page/app_item_test.js'
 ],
 [
   'OsAppsPageAppManagementPage',
   'os_apps_page/app_management_page/app_management_page_test.js'
 ],
 [
   'OsAppsPageAppManagementPageArcDetailView',
   'os_apps_page/app_management_page/arc_detail_view_test.js'
 ],
 [
   'OsAppsPageAppManagementPageBorealisDetailView',
   'os_apps_page/app_management_page/borealis_detail_view_test.js',
 ],
 [
   'OsAppsPageAppManagementPageChromeAppDetailView',
   'os_apps_page/app_management_page/chrome_app_detail_view_test.js',
 ],
 [
   'OsAppsPageAppManagementPageDomSwitch',
   'os_apps_page/app_management_page/dom_switch_test.js'
 ],
 [
   'OsAppsPageAppManagementPageMainView',
   'os_apps_page/app_management_page/main_view_test.js'
 ],
 [
   'OsAppsPageAppManagementPagePinToShelfItem',
   'os_apps_page/app_management_page/pin_to_shelf_item_test.js'
 ],
 [
   'OsAppsPageAppManagementPagePluginVmDetailView',
   'os_apps_page/app_management_page/plugin_vm_detail_view_test.js',
 ],
 [
   'OsAppsPageAppManagementPagePwaDetailView',
   'os_apps_page/app_management_page/pwa_detail_view_test.js'
 ],
 [
   'OsAppsPageAppManagementPageReducers',
   'os_apps_page/app_management_page/reducers_test.js'
 ],
 [
   'OsAppsPageAppManagementPageResizeLockItem',
   'os_apps_page/app_management_page/resize_lock_item_test.js'
 ],
 [
   'OsAppsPageAppManagementPageSupportedLinksItem',
   'os_apps_page/app_management_page/supported_links_item_test.js',
 ],
 [
   'OsAppsPageAppNotificationsSubpageWithRevamp',
   'os_apps_page/app_notifications_page/app_notifications_subpage_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsAppsPageAppNotificationsSubpageWithoutRevamp',
   'os_apps_page/app_notifications_page/app_notifications_subpage_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['OsAppsPage', 'os_apps_page/os_apps_page_test.js'],
 [
   'OsAppsPageAppNotificationsPageAppNotificationsManagerSubpage',
   'os_apps_page/app_notifications_page/app_notifications_manager_subpage_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']}
 ],
 [
   'OsAppsPageManageIsolatedWebAppsPageManageIsolatedWebAppsSubpage',
   'os_apps_page/manage_isolated_web_apps_page/manage_isolated_web_apps_subpage_test.js'
 ],
 ['OsBluetoothPage', 'os_bluetooth_page/os_bluetooth_page_test.js'],
 [
   'OsBluetoothPageOsBluetoothChangeDeviceNameDialog',
   'os_bluetooth_page/os_bluetooth_change_device_name_dialog_test.js',
 ],
 [
   'OsBluetoothPageOsBluetoothDeviceDetailSubpage',
   'os_bluetooth_page/os_bluetooth_device_detail_subpage_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'OsBluetoothPageOsBluetoothPairingDialog',
   'os_bluetooth_page/os_bluetooth_pairing_dialog_test.js'
 ],
 [
   'OsBluetoothPageOsBluetoothSummary',
   'os_bluetooth_page/os_bluetooth_summary_test.js'
 ],
 [
   'OsBluetoothPageOsBluetoothTrueWirelessImages',
   'os_bluetooth_page/os_bluetooth_true_wireless_images_tests.js',
 ],
 [
   'OsBluetoothPageOsPairedBluetoothList',
   'os_bluetooth_page/os_paired_bluetooth_list_test.js'
 ],
 [
   'OsBluetoothPageOsPairedBluetoothListItem',
   'os_bluetooth_page/os_paired_bluetooth_list_item_test.js'
 ],
 ['OsFilesPage', 'os_files_page/os_files_page_test.js'],
 [
   'OsFilesPageFilesSettingsCard',
   'os_files_page/files_settings_card_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['OsFilesPageGoogleDrivePage', 'os_files_page/google_drive_page_test.js'],
 ['OsFilesPageOneDrivePage', 'os_files_page/one_drive_page_test.js'],
 ['OsFilesPageOfficePage', 'os_files_page/office_page_test.js'],
 ['OsFilesPageSmbSharesPage', 'os_files_page/smb_shares_page_test.js'],
 [
   'OsFilesPageSmbSharesPageJelly',
   'os_files_page/smb_shares_page_test.js',
   {
     enabled:
         ['chromeos::features::kCrosComponents', 'chromeos::features::kJelly']
   },
 ],
 [
   'OsLanguagesPageInputMethodOptionsPage',
   'os_languages_page/input_method_options_page_test.js'
 ],
 ['OsLanguagesPageInputPage', 'os_languages_page/input_page_test.js'],
 [
   'OsLanguagesPageLanguageSettingsCard',
   'os_languages_page/language_settings_card_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsLanguagesPageOsClearPersonalizationDataPage',
   'os_languages_page/os_clear_personalization_data_page_test.js'
 ],
 ['OsLanguagesPageV2', 'os_languages_page/os_languages_page_v2_test.js'],
 [
   'OsLanguagesPageOsEditDictionaryPage',
   'os_languages_page/os_edit_dictionary_page_test.js'
 ],
 [
   'OsPageAvailability',
   'os_page_availability_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsPageAvailabilityRevamp',
   'os_page_availability_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['OsPeoplePage', 'os_people_page/os_people_page_test.js'],
 ['OsPeoplePageAddUserDialog', 'os_people_page/add_user_dialog_test.js'],
 [
   'OsPeoplePageFingerprintListSubpage',
   'os_people_page/fingerprint_list_subpage_test.js'
 ],
 [
   'OsPeoplePageOsSyncControlsSubpage',
   'os_people_page/os_sync_controls_subpage_test.js'
 ],
 [
   'OsPeoplePagePersonalizationOptions',
   'os_people_page/personalization_options_test.js',
 ],
 ['OsPrintingPage', 'os_printing_page/os_printing_page_test.js'],
 [
   'OsPrintingPagePrintingSettingsCard',
   'os_printing_page/printing_settings_card_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsPrintingPageCupsPrintServer', 'os_printing_page/cups_print_server_test.js'
 ],
 [
   'OsPrintingPageCupsPrinterDialog',
   'os_printing_page/cups_printer_dialog_test.js'
 ],
 [
   'OsPrintingPageCupsPrinterLandingPage',
   'os_printing_page/cups_printer_landing_page_test.js', {
     enabled: [
       'ash::features::kPrinterSettingsRevamp',
       'ash::features::kPrinterSettingsPrinterStatus'
     ]
   }
 ],
 [
   'OsPrintingPageCupsPrinterPage',
   'os_printing_page/cups_printer_page_test.js',
   {enabled: ['ash::features::kPrinterSettingsRevamp']}
 ],
 [
   'OsPrintingPageCupsPrintersEntry',
   'os_printing_page/cups_printers_entry_test.js', {
     enabled: [
       'ash::features::kPrinterSettingsRevamp',
       'ash::features::kPrinterSettingsPrinterStatus'
     ]
   }
 ],
 [
   'OsPrintingPagePrinterStatus',
   'os_printing_page/printer_status_test.js',
 ],
 ['OsPrivacyPage', 'os_privacy_page/os_privacy_page_test.js'],
 [
   'OsPrivacyPageManageUsersSubpage',
   'os_privacy_page/manage_users_subpage_test.js'
 ],
 [
   'OsPrivacyPagePrivacyHubMicrophoneSubpage',
   'os_privacy_page/privacy_hub_microphone_subpage_test.js',
   {
     enabled: [
       'ash::features::kCrosPrivacyHubV0',
       'ash::features::kCrosPrivacyHubAppPermissions'
     ]
   },
 ],
 [
   'OsPrivacyPagePrivacyHubSubpage',
   'os_privacy_page/privacy_hub_subpage_test.js',
   {enabled: ['ash::features::kCrosPrivacyHubV0']},
 ],
 [
   'OsPrivacyPageSmartPrivacySubpage',
   'os_privacy_page/smart_privacy_subpage_test.js'
 ],
 ['OsResetPage', 'os_reset_page/os_reset_page_test.js'],
 [
   'OsResetPageResetSettingsCard',
   'os_reset_page/reset_settings_card_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['OsSearchPage', 'os_search_page/os_search_page_test.js'],
 [
   'OsSearchPageGoogleAssistantSubpage',
   'os_search_page/google_assistant_subpage_test.js'
 ],
 [
   'OsSearchPageSearchAndAssistantSettingsCard',
   'os_search_page/search_and_assistant_settings_card_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['OsSearchPageSearchSubpage', 'os_search_page/search_subpage_test.js'],
 ['OsSettingsHatsUi', 'os_settings_ui/os_settings_hats_ui_test.js'],
 ['OsSettingsMain', 'os_settings_main/os_settings_main_test.js'],
 [
   'OsSettingsMenu',
   'os_settings_menu/os_settings_menu_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsMenuRevamp',
   'os_settings_menu/os_settings_menu_revamp_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsSearchBox',
   'os_settings_search_box/os_settings_search_box_test.js'
 ],
 ['OsSettingsUi', 'os_settings_ui/os_settings_ui_test.js'],
 [
   'OsSettingsUiAboutPage',
   'os_settings_ui/os_settings_ui_about_page_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['OsSettingsUiMenu', 'os_settings_ui/os_settings_ui_menu_test.js'],
 [
   'OsSettingsUiPageAvailability',
   'os_settings_ui/os_settings_ui_page_availability_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiPageAvailabilityRevamp',
   'os_settings_ui/os_settings_ui_page_availability_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiPageVisibilityRevamp',
   'os_settings_ui/os_settings_ui_page_visibility_revamp_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiScrollRestoration',
   'os_settings_ui/scroll_restoration_test.js',
 ],
 ['OsSettingsUiToolbar', 'os_settings_ui/os_settings_ui_toolbar_test.js'],
 [
   'OsSettingsUiUserActionRecorder',
   'os_settings_ui/user_action_recorder_test.js'
 ],
 [
   'ParentalControlsPage',
   'parental_controls_page/parental_controls_page_test.js'
 ],
 [
   'ParentalControlsSettingsCard',
   'parental_controls_page/parental_controls_settings_card_test.js'
 ],
 [
   'OsPeoplePageAccountManagerSettingsCard',
   'os_people_page/account_manager_settings_card_test.js',
 ],
 [
   'OsPeoplePageAccountManagerSubpage',
   'os_people_page/account_manager_subpage_test.js',
   {disabled: ['ash::standalone_browser::features::kLacrosOnly']},
 ],
 [
   'OsPeoplePageAccountManagerSubpageWithArcAccountRestrictionsEnabled',
   'os_people_page/account_manager_subpage_test.js',
   {
     enabled: [
       'ash::standalone_browser::features::kLacrosOnly',
       'ash::standalone_browser::features::kLacrosProfileMigrationForceOff'
     ]
   },
 ],
 [
   'OsPeoplePageAdditionalAccountsSettingsCard',
   'os_people_page/additional_accounts_settings_card_test.js',
 ],
 [
   'PersonalizationPageWithPersonalizationHub',
   'personalization_page/personalization_page_with_personalization_hub_test.js',
 ],
 [
   'SettingsSchedulerSlider',
   'settings_scheduler_slider/settings_scheduler_slider_test.js'
 ],
 [
   'SystemPreferencesPage',
   'system_preferences_page/system_preferences_page_test.js',
   {
     enabled: [
       'ash::features::kOsSettingsRevampWayfinding',
     ],
   },
 ],
 [
   'SystemPreferencesPageDateTimeSettingsCard',
   'date_time_page/date_time_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'SystemPreferencesPageFilesSettingsCard',
   'os_files_page/files_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'SystemPreferencesPageLanguageSettingsCard',
   'os_languages_page/language_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'SystemPreferencesPageResetSettingsCard',
   'os_reset_page/reset_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'SystemPreferencesPageSearchAndAssistantSettingsCard',
   'os_search_page/search_and_assistant_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'SystemPreferencesPageStartupSettingsCard',
   'system_preferences_page/startup_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'SystemPreferencesPageStorageAndPowerSettingsCard',
   'system_preferences_page/storage_and_power_settings_card_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
].forEach(test => registerTest(...test));

function registerTest(testName, module, featureList) {
  if (testName.startsWith('DISABLED')) {
    return;
  }

  const className = `OSSettings${testName}Test`;
  this[className] = class extends OSSettingsBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/${
          module}`;
    }
  };

  if (featureList) {
    Object.defineProperty(this[className].prototype, 'featureList', {
      get() {
        return featureList;
      },
    });
  }

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
  } else if (testName === 'OsPrivacyPagePrivacyHubSubpage') {
    // PrivacyHubSubpage has a test suite that can only succeed on official
    // builds where the is_chrome_branded build flag is enabled.
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!<os-settings-privacy-page> OfficialBuild).*$/').run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('<os-settings-privacy-page> OfficialBuild').run();
    });
    GEN('#endif');
  } else if (testName === 'OsSettingsSearchBox') {
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha
          .grep(
              '/^(?!(<os-settings-search-box> SearchFeedback_OfficialBuild)).*$/')
          .run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('SearchFeedback_OfficialBuild').run();
    });
    GEN('#endif');
  } else {
    TEST_F(className, 'All', () => mocha.run());
  }
}
