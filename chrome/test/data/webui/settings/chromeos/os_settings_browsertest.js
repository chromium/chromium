// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"');
GEN('#include "chrome/common/buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "components/app_restore/features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ui/accessibility/accessibility_features.h"');

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
      ],
      disabled: [],
    };
  }
};

var OSSettingsDevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/device_page/device_page_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kInputDeviceSettingsSplit',
        'ash::features::kPeripheralCustomization',
      ],
      disabled: [
        'ash::features::kOsSettingsRevampWayfinding',
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

var OSSettingsDevicePageRevampTest = class extends OSSettingsDevicePageTest {
  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kOsSettingsRevampWayfinding',
      ]),
    };
  }
};

TEST_F('OSSettingsDevicePageRevampTest', 'AllJsTests', () => {
  mocha.run();
});

[['AboutPage', 'os_about_page_tests.js'],
 [
   'ApnSubpage', 'apn_subpage_tests.js',
   {enabled: ['ash::features::kApnRevamp']}
 ],
 ['CellularNetworksList', 'cellular_networks_list_test.js'],
 ['CellularRoamingToggleButton', 'cellular_roaming_toggle_button_test.js'],
 ['EsimRemoveProfileDialog', 'esim_remove_profile_dialog_test.js'],
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
   'OsAppsPage',
   'os_apps_page/os_apps_page_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsAppsPageRevamp',
   'os_apps_page/os_apps_page_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
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
   'OsAppsPageAppManagementPageAppLanguageItem',
   'os_apps_page/app_management_page/app_language_item_test.js'
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
   'OsBluetoothPageOsBluetoothDevicesSubpage',
   'os_bluetooth_page/os_bluetooth_devices_subpage_test.js'
 ],
 [
   'OsBluetoothPageOsBluetoothPairingDialog',
   'os_bluetooth_page/os_bluetooth_pairing_dialog_test.js'
 ],
 [
   'OsBluetoothPageOsBluetoothSavedDevicesList',
   'os_bluetooth_page/os_saved_devices_list_test.js'
 ],
 [
   'OsBluetoothPageOsBluetoothSavedDevicesSubpage',
   'os_bluetooth_page/os_bluetooth_saved_devices_subpage_test.js'
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
   'OsLanguagesPageAppLanguagesPage',
   'os_languages_page/app_languages_page_test.js',
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
 [
   'OsPeoplePage',
   'os_people_page/os_people_page_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsPeoplePageRevamp',
   'os_people_page/os_people_page_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
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
   'OsPrivacyPagePrivacyHubAppPermissionRow',
   'os_privacy_page/privacy_hub_app_permission_row_test.js'
 ],
 [
   'OsPrivacyPagePrivacyHubCameraSubpage',
   'os_privacy_page/privacy_hub_camera_subpage_test.js',
   {
     enabled: [
       'ash::features::kCrosPrivacyHubV0',
       'ash::features::kCrosPrivacyHubAppPermissions'
     ]
   },
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
   'OsPrivacyPagePrivacyHubGeolocationSubpage',
   'os_privacy_page/privacy_hub_geolocation_subpage_test.js',
   {
     enabled: [
       'ash::features::kCrosPrivacyHubV0',
       'ash::features::kCrosPrivacyHub',
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
 [
   'OsSearchPageSearchEngine',
   'os_search_page/search_engine_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSearchPageSearchEngineRevamp',
   'os_search_page/search_engine_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 ['OsSearchPageSearchSubpage', 'os_search_page/search_subpage_test.js'],
 [
   'OsSettingsMain',
   'os_settings_main/os_settings_main_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsMainRevamp',
   'os_settings_main/os_settings_main_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
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
 [
   'OsSettingsUi',
   'os_settings_ui/os_settings_ui_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiRevamp',
   'os_settings_ui/os_settings_ui_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiAboutPage',
   'os_settings_ui/os_settings_ui_about_page_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiHats',
   'os_settings_ui/os_settings_ui_hats_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiHatsRevamp',
   'os_settings_ui/os_settings_ui_hats_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiMenu',
   'os_settings_ui/os_settings_ui_menu_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiMenuRevamp',
   'os_settings_ui/os_settings_ui_menu_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
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
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiToolbar',
   'os_settings_ui/os_settings_ui_toolbar_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiToolbarRevamp',
   'os_settings_ui/os_settings_ui_toolbar_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiUserActionRecorder',
   'os_settings_ui/user_action_recorder_test.js',
   {disabled: ['ash::features::kOsSettingsRevampWayfinding']},
 ],
 [
   'OsSettingsUiUserActionRecorderRevamp',
   'os_settings_ui/user_action_recorder_test.js',
   {enabled: ['ash::features::kOsSettingsRevampWayfinding']},
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
