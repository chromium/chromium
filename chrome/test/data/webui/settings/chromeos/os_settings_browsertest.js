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

[['AboutPage', 'os_about_page_tests.js'],
 [
   'ApnSubpage', 'apn_subpage_test.js', {enabled: ['ash::features::kApnRevamp']}
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
 ['OsResetPage', 'os_reset_page/os_reset_page_test.js'],
 [
   'OsResetPageResetSettingsCardWithSanitize',
   'os_reset_page/reset_settings_card_test.js',
   {
     disabled: ['ash::features::kOsSettingsRevampWayfinding'],
     enabled: ['ash::features::kSanitize']
   },
 ],
 [
   'OsResetPageResetSettingsCardWithoutSanitize',
   'os_reset_page/reset_settings_card_test.js',
   {
     disabled: [
       'ash::features::kOsSettingsRevampWayfinding', 'ash::features::kSanitize'
     ]
   },
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
  } else {
    TEST_F(className, 'All', () => mocha.run());
  }
}
