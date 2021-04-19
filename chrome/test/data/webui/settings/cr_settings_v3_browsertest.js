// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/autofill/core/common/autofill_features.h"');
GEN('#include "components/password_manager/core/common/password_manager_features.h"');
GEN('#include "content/public/test/browser_test.h"');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)');
GEN('#include "components/language/core/common/language_experiments.h"');
GEN('#endif');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrSettingsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings';
  }

  /** @override */
  get featureList() {
    if (!this.featureListInternal.enabled &&
        !this.featureListInternal.disabled) {
      return null;
    }
    return this.featureListInternal;
  }

  /** @return {!{enabled: !Array<string>, disabled: !Array<string>}} */
  get featureListInternal() {
    return {};
  }
};

// eslint-disable-next-line no-var
var CrSettingsAboutPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/about_page_tests.js';
  }
};

TEST_F('CrSettingsAboutPageV3Test', 'AboutPage', function() {
  mocha.grep('AboutPageTest_AllBuilds').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsAboutPageV3Test', 'AboutPage_OfficialBuild', function() {
  mocha.grep('AboutPageTest_OfficialBuilds').run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsAvatarIconV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/avatar_icon_test.js';
  }
};

TEST_F('CrSettingsAvatarIconV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsLanguagesPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_page_tests.js';
  }
};

TEST_F('CrSettingsLanguagesPageV3Test', 'Spellcheck', function() {
  mocha.grep(languages_page_tests.TestNames.Spellcheck).run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsLanguagesPageV3Test', 'SpellcheckOfficialBuild', function() {
  mocha.grep(languages_page_tests.TestNames.SpellcheckOfficialBuild).run();
});
GEN('#endif');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)');
// eslint-disable-next-line no-var
var CrSettingsLanguagesPageRestructuredV3Test =
    class extends CrSettingsLanguagesPageV3Test {
  /** @override */
  get featureListInternal() {
    return {enabled: ['language::kDesktopRestructuredLanguageSettings']};
  }
};
TEST_F(
    'CrSettingsLanguagesPageRestructuredV3Test', 'RestructuredLanguageSettings',
    function() {
      mocha.grep(languages_page_tests.TestNames.RestructuredLanguageSettings)
          .run();
    });
GEN('#endif');

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
TEST_F(
    'CrSettingsLanguagesPageV3Test', 'ChromeOSLanguagesSettingsUpdate',
    function() {
      mocha.grep(languages_page_tests.TestNames.ChromeOSLanguagesSettingsUpdate)
          .run();
    });
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsLanguagesSubpageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_subpage_tests.js';
  }
};

TEST_F('CrSettingsLanguagesSubpageV3Test', 'AddLanguagesDialog', function() {
  mocha.grep(languages_subpage_tests.TestNames.AddLanguagesDialog).run();
});

TEST_F('CrSettingsLanguagesSubpageV3Test', 'LanguageMenu', function() {
  mocha.grep(languages_subpage_tests.TestNames.LanguageMenu).run();
});

// eslint-disable-next-line no-var
var CrSettingsLanguagesPageMetricsV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_page_metrics_test_browser.js';
  }
};

TEST_F(
    'CrSettingsLanguagesPageMetricsV3Test', 'LanguagesPageMetricsBrowser',
    function() {
      runMochaSuite('LanguagesPageMetricsBrowser');
    });

// eslint-disable-next-line no-var
var CrSettingsClearBrowsingDataV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/clear_browsing_data_test.js';
  }
};

// TODO(crbug.com/1107652): Flaky on Mac.
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_ClearBrowsingDataAllPlatforms DISABLED_ClearBrowsingDataAllPlatforms');
GEN('#else');
GEN('#define MAYBE_ClearBrowsingDataAllPlatforms ClearBrowsingDataAllPlatforms');
GEN('#endif');
TEST_F(
    'CrSettingsClearBrowsingDataV3Test', 'MAYBE_ClearBrowsingDataAllPlatforms',
    function() {
      runMochaSuite('ClearBrowsingDataAllPlatforms');
    });

TEST_F('CrSettingsClearBrowsingDataV3Test', 'InstalledApps', () => {
  runMochaSuite('InstalledApps');
});

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
TEST_F(
    'CrSettingsClearBrowsingDataV3Test', 'ClearBrowsingDataDesktop',
    function() {
      runMochaSuite('ClearBrowsingDataDesktop');
    });
GEN('#endif');


// eslint-disable-next-line no-var
var CrSettingsMainPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_main_test.js';
  }
};

// Copied from Polymer 2 version of tests:
// Times out on Windows Tests (dbg). See https://crbug.com/651296.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/667882
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_MainPageV3 DISABLED_MainPageV3');
GEN('#else');
GEN('#define MAYBE_MainPageV3 MainPageV3');
GEN('#endif');
TEST_F('CrSettingsMainPageV3Test', 'MAYBE_MainPageV3', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsAutofillPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_page_test.js';
  }
};

TEST_F('CrSettingsAutofillPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsAutofillSectionCompanyEnabledV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_section_test.js';
  }
};

TEST_F('CrSettingsAutofillSectionCompanyEnabledV3Test', 'All', function() {
  // Use 'EnableCompanyName' to inform tests that the feature is enabled.
  const loadTimeDataOverride = {};
  loadTimeDataOverride['EnableCompanyName'] = true;
  loadTimeDataOverride['showHonorific'] = true;
  loadTimeData.overrideValues(loadTimeDataOverride);
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPasswordsSectionV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_section_test.js';
  }
};

// Flaky on Debug builds https://crbug.com/1090931
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrSettingsPasswordsSectionV3Test', 'MAYBE_All', function() {
  mocha.run();
});
GEN('#undef MAYBE_All');

// eslint-disable-next-line no-var
var CrSettingsMultiStorePasswordUiEntryV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/multi_store_password_ui_entry_test.js';
  }
};

TEST_F('CrSettingsMultiStorePasswordUiEntryV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPasswordsDeviceSectionV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_device_section_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: ['password_manager::features::kEnablePasswordsAccountStorage']
    };
  }
};

TEST_F('CrSettingsPasswordsDeviceSectionV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsMultiStoreExceptionEntryV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/multi_store_exception_entry_test.js';
  }
};

TEST_F('CrSettingsMultiStoreExceptionEntryV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPasswordsCheckV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_check_test.js';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kSafetyCheckWeakPasswords']};
  }
};

// Flaky on Mac builds https://crbug.com/1143801
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrSettingsPasswordsCheckV3Test', 'MAYBE_All', function() {
  mocha.run();
});
GEN('#undef MAYBE_All');

// eslint-disable-next-line no-var
var CrSettingsSafetyCheckPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_page_test.js';
  }
};

TEST_F('CrSettingsSafetyCheckPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSafetyCheckChromeCleanerV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_chrome_cleaner_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'features::kSafetyCheckChromeCleanerChild',
      ],
    };
  }
};

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsSafetyCheckChromeCleanerV3Test', 'All', function() {
  mocha.run();
});
GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

// eslint-disable-next-line no-var
var CrSettingsSiteListV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_list_tests.js';
  }
};

// Copied from Polymer 2 test:
// TODO(crbug.com/929455): flaky, fix.
TEST_F('CrSettingsSiteListV3Test', 'DISABLED_SiteList', function() {
  runMochaSuite('SiteList');
});

// TODO(crbug.com/929455): When the bug is fixed, merge
// SiteListEmbargoedOrigin into SiteList
TEST_F('CrSettingsSiteListV3Test', 'SiteListEmbargoedOrigin', function() {
  runMochaSuite('SiteListEmbargoedOrigin');
});

TEST_F('CrSettingsSiteListV3Test', 'EditExceptionDialog', function() {
  runMochaSuite('EditExceptionDialog');
});

TEST_F('CrSettingsSiteListV3Test', 'AddExceptionDialog', function() {
  runMochaSuite('AddExceptionDialog');
});

// eslint-disable-next-line no-var
var CrSettingsSiteDetailsV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_details_tests.js';
  }
};

// Disabling on debug due to flaky timeout on Win7 Tests (dbg)(1) bot.
// https://crbug.com/825304 - later for other platforms in crbug.com/1021219.
// Disabling on Linux CFI due to flaky timeout (crbug.com/1031960).
GEN('#if (!defined(NDEBUG)) || ((defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(IS_CFI))');
GEN('#define MAYBE_SiteDetails DISABLED_SiteDetails');
GEN('#else');
GEN('#define MAYBE_SiteDetails SiteDetails');
GEN('#endif');

TEST_F('CrSettingsSiteDetailsV3Test', 'MAYBE_SiteDetails', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPersonalizationOptionsV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/personalization_options_test.js';
  }
};

TEST_F('CrSettingsPersonalizationOptionsV3Test', 'AllBuilds', function() {
  runMochaSuite('PersonalizationOptionsTests_AllBuilds');
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsPersonalizationOptionsV3Test', 'OfficialBuild', function() {
  runMochaSuite('PersonalizationOptionsTests_OfficialBuild');
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsPrivacyPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_page_test.js';
  }
};

TEST_F('CrSettingsPrivacyPageV3Test', 'PrivacyPageTests', function() {
  runMochaSuite('PrivacyPage');
});

TEST_F('CrSettingsPrivacyPageV3Test', 'ContentSettingsRedesign', function() {
  runMochaSuite('ContentSettingsRedesign');
});

TEST_F(
    'CrSettingsPrivacyPageV3Test', 'PrivacySandboxSettingsEnabled', function() {
      runMochaSuite('PrivacySandboxSettingsEnabled');
    });

// TODO(crbug.com/1043665): flaky crash on Linux Tests (dbg).
TEST_F(
    'CrSettingsPrivacyPageV3Test', 'DISABLED_PrivacyPageSoundTests',
    function() {
      runMochaSuite('PrivacyPageSound');
    });

// TODO(crbug.com/1113912): flaky failure on multiple platforms
TEST_F(
    'CrSettingsPrivacyPageV3Test', 'DISABLED_HappinessTrackingSurveysTests', function() {
      runMochaSuite('HappinessTrackingSurveys');
    });

GEN('#if defined(OS_MAC) || defined(OS_WIN)');
// TODO(crbug.com/1043665): disabling due to failures on several builders.
TEST_F(
    'CrSettingsPrivacyPageV3Test', 'DISABLED_CertificateManagerTests',
    function() {
      runMochaSuite('NativeCertificateManager');
    });
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsRouteV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/route_tests.js';
  }
};

TEST_F('CrSettingsRouteV3Test', 'Basic', function() {
  runMochaSuite('route');
});

TEST_F('CrSettingsRouteV3Test', 'DynamicParameters', function() {
  runMochaSuite('DynamicParameters');
});

// Copied from Polymer 2 test:
// Failing on ChromiumOS dbg. https://crbug.com/709442
GEN('#if (defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)) && !defined(NDEBUG)');
GEN('#define MAYBE_NonExistentRoute DISABLED_NonExistentRoute');
GEN('#else');
GEN('#define MAYBE_NonExistentRoute NonExistentRoute');
GEN('#endif');

TEST_F('CrSettingsRouteV3Test', 'MAYBE_NonExistentRoute', function() {
  runMochaSuite('NonExistentRoute');
});

// eslint-disable-next-line no-var
var CrSettingsAdvancedPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/advanced_page_test.js';
  }
};

// Copied from Polymer 2 test:
// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
GEN('#endif');
TEST_F('CrSettingsAdvancedPageV3Test', 'MAYBE_Load', function() {
  mocha.run();
});

[['AllSites', 'all_sites_tests.js'],
 ['AppearanceFontsPage', 'appearance_fonts_page_test.js'],
 ['AppearancePage', 'appearance_page_test.js'],
 ['BasicPage', 'basic_page_test.js'],
 [
   'SettingsCategoryDefaultRadioGroup',
   'settings_category_default_radio_group_tests.js'
 ],
 ['CategoryDefaultSetting', 'category_default_setting_tests.js'],
 ['CategorySettingExceptions', 'category_setting_exceptions_tests.js'],
 ['Checkbox', 'checkbox_tests.js'],
 ['ChooserExceptionList', 'chooser_exception_list_tests.js'],
 ['ChooserExceptionListEntry', 'chooser_exception_list_entry_tests.js'],
 ['CollapseRadioButton', 'collapse_radio_button_tests.js'],
 ['ControlledButton', 'controlled_button_tests.js'],
 ['ControlledRadioButton', 'controlled_radio_button_tests.js'],
 ['CookiesPage', 'cookies_page_test.js'],
 ['DoNotTrackToggle', 'do_not_track_toggle_test.js'],
 ['DownloadsPage', 'downloads_page_test.js'],
 ['DropdownMenu', 'dropdown_menu_tests.js'],
 ['ExtensionControlledIndicator', 'extension_controlled_indicator_tests.js'],
 ['HelpPage', 'help_page_v3_test.js'],
 ['Languages', 'languages_tests.js'],
 ['Menu', 'settings_menu_test.js'],
 ['OnStartupPage', 'on_startup_page_tests.js'],
 ['PaymentsSection', 'payments_section_test.js'],
 ['PeoplePage', 'people_page_test.js'],
 ['PeoplePageSyncControls', 'people_page_sync_controls_test.js'],
 ['PeoplePageSyncPage', 'people_page_sync_page_test.js'],
 ['Prefs', 'prefs_tests.js'],
 ['PrefUtil', 'pref_util_tests.js'],
 ['ProtocolHandlers', 'protocol_handlers_tests.js'],
 ['RecentSitePermissions', 'recent_site_permissions_test.js'],
 // Flaky on all OSes. TODO(crbug.com/1127733): Enable the test.
 ['ResetPage', 'reset_page_test.js', 'DISABLED_All'],
 ['ResetProfileBanner', 'reset_profile_banner_test.js'],
 ['SearchEngines', 'search_engines_page_test.js'],
 ['SearchPage', 'search_page_test.js'],
 ['Search', 'search_settings_test.js'],
 ['SecurityKeysSubpage', 'security_keys_subpage_test.js'],
 ['SecureDns', 'secure_dns_test.js'],
 ['SiteData', 'site_data_test.js'],
 ['SiteDataDetails', 'site_data_details_subpage_tests.js'],
 ['SiteDetailsPermission', 'site_details_permission_tests.js'],
 ['SiteEntry', 'site_entry_tests.js'],
 ['SiteFavicon', 'site_favicon_test.js'],
 ['SiteListEntry', 'site_list_entry_tests.js'],
 ['SiteSettingsPage', 'site_settings_page_test.js'],
 ['Slider', 'settings_slider_tests.js'],
 ['StartupUrlsPage', 'startup_urls_page_test.js'],
 ['Subpage', 'settings_subpage_test.js'],
 ['SyncAccountControl', 'sync_account_control_test.js'],
 ['Textarea', 'settings_textarea_tests.js'],
 ['ToggleButton', 'settings_toggle_button_tests.js'],
 ['ZoomLevels', 'zoom_levels_tests.js'],
].forEach(test => registerTest(...test));

// Timeout on MacOS dbg bots
// https://crbug.com/1133412
GEN('#if !defined(OS_MAC) || defined(NDEBUG)');
[['SecurityPage', 'security_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // !defined(OS_MAC) || defined(NDEBUG)');

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
[['LanguagesPageMetricsChromeOS', 'languages_page_metrics_test_cros.js'],
 ['PasswordsSectionCros', 'passwords_section_test_cros.js'],
 ['PeoplePageChromeOS', 'people_page_test_cros.js'],
 // Copied from Polymer 2 test. TODO(crbug.com/929455): flaky, fix.
 ['SiteListChromeOS', 'site_list_tests_cros.js', 'DISABLED_AndroidSmsInfo'],
].forEach(test => registerTest(...test));
GEN('#endif  // BUILDFLAG(IS_CHROMEOS_ASH)');

GEN('#if !defined(OS_MAC)');
[['EditDictionaryPage', 'edit_dictionary_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif  //!defined(OS_MAC)');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)');
[['DefaultBrowser', 'default_browser_browsertest.js'],
 ['SystemPage', 'system_page_tests.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
[['ImportDataDialog', 'import_data_dialog_test.js'],
 ['PeoplePageManageProfile', 'people_page_manage_profile_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)');

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');
[['ChromeCleanupPage', 'chrome_cleanup_page_test.js'],
 ['IncompatibleApplicationsPage', 'incompatible_applications_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)');
registerTest('MetricsReporting', 'metrics_reporting_tests.js');
GEN('#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) ' +
    '&& !BUILDFLAG(IS_CHROMEOS_ASH)');

function registerTest(testName, module, caseName) {
  const className = `CrSettings${testName}V3Test`;
  this[className] = class extends CrSettingsV3BrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://settings/test_loader.html?module=settings/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
