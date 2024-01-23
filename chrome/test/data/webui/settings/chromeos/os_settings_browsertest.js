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
GEN('#include "components/app_restore/features.h"');
GEN('#include "content/public/test/browser_test.h"');

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
   'OsPrivacyPagePrivacyHubSubpage',
   'os_privacy_page/privacy_hub_subpage_test.js',
   {enabled: ['ash::features::kCrosPrivacyHubV0']},
 ],
 [
   'OsSettingsSearchBox',
   'os_settings_search_box/os_settings_search_box_test.js'
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
