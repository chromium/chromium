// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/common/buildflags.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "chrome/browser/browser_features.h"');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var OSSettingsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kOsSettingsPolymer3',
        'chromeos::features::kUpdatedCellularActivationUi',
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

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled,
      disabled: ['chromeos::features::kLanguageSettingsUpdate']
    };
  }
};

TEST_F('OSSettingsOsLanguagesPageV3Test', 'All', () => mocha.run());

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

[['AmbientModePage', 'ambient_mode_page_test.m.js'],
 ['BluetoothPage', 'bluetooth_page_tests.m.js'],
 ['CellularSetupDialog', 'cellular_setup_dialog_test.m.js'],
 ['DateTimePage', 'date_time_page_tests.m.js'],
 ['InputMethodOptionPage', 'input_method_options_page_test.m.js'],
 ['InputPage', 'input_page_test.m.js'],
 ['InternetConfig', 'internet_config_test.m.js'],
 ['InternetDetailPage', 'internet_detail_page_tests.m.js'],
 ['InternetKnownNetworksPage', 'internet_known_networks_page_tests.m.js'],
 ['LocalizedLink', 'localized_link_test.m.js'],
 ['MultideviceFeatureItem', 'multidevice_feature_item_tests.m.js'],
 ['MultideviceFeatureToggle', 'multidevice_feature_toggle_tests.m.js'],
 ['MultidevicePage', 'multidevice_page_tests.m.js'],
 ['MultideviceSmartLockSubPage', 'multidevice_smartlock_subpage_test.m.js'],
 ['MultideviceSubPage', 'multidevice_subpage_tests.m.js'],
 ['NetworkProxySection', 'network_proxy_section_test.m.js'],
 ['NetworkSummary', 'network_summary_test.m.js'],
 ['NetworkSummaryItem', 'network_summary_item_test.m.js'],
 ['OsEditDictionaryPage', 'os_edit_dictionary_page_test.m.js'],
 ['OsLanguagesPageV2', 'os_languages_page_v2_tests.m.js'],
 ['NearbyShareReceiveDialog', 'nearby_share_receive_dialog_tests.m.js'],
 ['ParentalControlsPage', 'parental_controls_page_test.m.js'],
 ['PeoplePage', 'os_people_page_test.m.js'],
 ['PeoplePageAccountManager', 'people_page_account_manager_test.m.js'],
 ['PeoplePageChangePicture', 'people_page_change_picture_test.m.js'],
 ['PeoplePageKerberosAccounts', 'people_page_kerberos_accounts_test.m.js'],
 ['PersonalizationPage', 'personalization_page_test.m.js'],
 ['PrintingPage', 'os_printing_page_tests.m.js'],
 ['PrivacyPage', 'os_privacy_page_test.m.js'],
 ['SmbPage', 'smb_shares_page_tests.m.js'],
 ['FilesPage', 'os_files_page_test.m.js'],
 ['ResetPage', 'os_reset_page_test.m.js'],
 ['SmartInputsPage', 'smart_inputs_page_test.m.js'],
 ['TetherConnectionDialog', 'tether_connection_dialog_test.m.js'],
 ['TimezoneSelector', 'timezone_selector_test.m.js'],
 ['TimezoneSubpage', 'timezone_subpage_test.m.js'],
 ['CupsPrinterPage', 'cups_printer_page_tests.m.js'],
 ['CupsPrinterLandingPage', 'cups_printer_landing_page_tests.m.js'],
 ['CupsPrinterEntry', 'cups_printer_entry_tests.m.js'],
 ['AboutPage', 'os_about_page_tests.m.js'],
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
