// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "build/build_config.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/content_settings/core/common/features.h"');
GEN('#include "components/performance_manager/public/features.h"');
GEN('#include "components/privacy_sandbox/privacy_sandbox_features.h"');
GEN('#include "components/autofill/core/common/autofill_features.h"');
GEN('#include "content/public/common/content_features.h"');
GEN('#include "content/public/test/browser_test.h"');

GEN('#if !BUILDFLAG(IS_CHROMEOS)');
GEN('#include "components/language/core/common/language_experiments.h"');
GEN('#endif');

/** Test fixture for shared Polymer 3 elements. */
var CrSettingsBrowserTest = class extends PolymerTest {
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

var CrSettingsAboutPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/about_page_tests.js';
  }
};

TEST_F('CrSettingsAboutPageTest', 'AboutPage', function() {
  mocha.grep('AboutPageTest_AllBuilds').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsAboutPageTest', 'AboutPage_OfficialBuild', function() {
  mocha.grep('AboutPageTest_OfficialBuilds').run();
});
GEN('#endif');

var CrSettingsAvatarIconTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/avatar_icon_test.js';
  }
};

TEST_F('CrSettingsAvatarIconTest', 'All', function() {
  mocha.run();
});

var CrSettingsBasicPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/basic_page_test.js';
  }
};

// TODO(crbug.com/1298753): Flaky on all platforms.
TEST_F('CrSettingsBasicPageTest', 'DISABLED_BasicPage', function() {
  runMochaSuite('SettingsBasicPage');
});

GEN('#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)');
GEN('#define MAYBE_PrivacyGuidePromo DISABLED_PrivacyGuidePromo');
GEN('#else');
GEN('#define MAYBE_PrivacyGuidePromo PrivacyGuidePromo');
GEN('#endif');
TEST_F('CrSettingsBasicPageTest', 'MAYBE_PrivacyGuidePromo', function() {
  runMochaSuite('PrivacyGuidePromo');
});

TEST_F('CrSettingsBasicPageTest', 'Performance', function() {
  runMochaSuite('SettingsBasicPagePerformance');
});

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
var CrSettingsSpellCheckPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/spell_check_page_tests.js';
  }
};

TEST_F('CrSettingsSpellCheckPageTest', 'Spellcheck', function() {
  mocha.grep(spell_check_page_tests.TestNames.Spellcheck).run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsSpellCheckPageTest', 'SpellcheckOfficialBuild', function() {
  mocha.grep(spell_check_page_tests.TestNames.SpellcheckOfficialBuild).run();
});
GEN('#endif');

var CrSettingsLanguagesPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_page_tests.js';
  }
};

TEST_F('CrSettingsLanguagesPageTest', 'AddLanguagesDialog', function() {
  mocha.grep(languages_page_tests.TestNames.AddLanguagesDialog).run();
});

TEST_F('CrSettingsLanguagesPageTest', 'LanguageMenu', function() {
  mocha.grep(languages_page_tests.TestNames.LanguageMenu).run();
});

var CrSettingsTranslatePageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/translate_page_tests.js';
  }
};

TEST_F('CrSettingsTranslatePageTest', 'TranslateSettings', function() {
  mocha.grep(translate_page_tests.TestNames.TranslateSettings).run();
});

TEST_F('CrSettingsTranslatePageTest', 'AlwaysTranslateDialog', function() {
  mocha.grep(translate_page_tests.TestNames.AlwaysTranslateDialog).run();
});

TEST_F('CrSettingsTranslatePageTest', 'NeverTranslateDialog', function() {
  mocha.grep(translate_page_tests.TestNames.NeverTranslateDialog).run();
});

var CrSettingsLanguagesPageMetricsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_page_metrics_test_browser.js';
  }
};

TEST_F(
    'CrSettingsLanguagesPageMetricsTest', 'LanguagesPageMetricsBrowser',
    function() {
      runMochaSuite('LanguagesPageMetricsBrowser');
    });

var CrSettingsTranslatePageMetricsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/translate_page_metrics_test_browser.js';
  }
};

TEST_F(
    'CrSettingsTranslatePageMetricsTest', 'TranslatePageMetricsBrowser',
    function() {
      runMochaSuite('TranslatePageMetricsBrowser');
    });
var CrSettingsSpellCheckPageMetricsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/spell_check_page_metrics_test_browser.js';
  }
};

TEST_F('CrSettingsSpellCheckPageMetricsTest', 'SpellCheckMetrics', function() {
  mocha.grep(spell_check_page_metrics_test_browser.TestNames.SpellCheckMetrics).run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsSpellCheckPageMetricsTest', 'SpellCheckMetricsOfficialBuild', function() {
  mocha.grep(spell_check_page_metrics_test_browser.TestNames.SpellCheckMetricsOfficialBuild).run();
});
GEN('#endif');

GEN('#if !BUILDFLAG(IS_MAC)');
TEST_F('CrSettingsSpellCheckPageMetricsTest', 'SpellCheckMetricsNotMacOSx', function() {
  mocha.grep(spell_check_page_metrics_test_browser.TestNames.SpellCheckMetricsNotMacOSx).run();
});
GEN('#endif');

GEN('#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)');

var CrSettingsClearBrowsingDataTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/clear_browsing_data_test.js';
  }
};

// TODO(crbug.com/1107652): Flaky on Mac.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_ClearBrowsingDataAllPlatforms DISABLED_ClearBrowsingDataAllPlatforms');
GEN('#else');
GEN('#define MAYBE_ClearBrowsingDataAllPlatforms ClearBrowsingDataAllPlatforms');
GEN('#endif');
TEST_F(
    'CrSettingsClearBrowsingDataTest', 'MAYBE_ClearBrowsingDataAllPlatforms',
    function() {
      runMochaSuite('ClearBrowsingDataAllPlatforms');
    });

TEST_F('CrSettingsClearBrowsingDataTest', 'InstalledApps', () => {
  runMochaSuite('InstalledApps');
});

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
TEST_F(
    'CrSettingsClearBrowsingDataTest', 'ClearBrowsingDataDesktop', function() {
      runMochaSuite('ClearBrowsingDataDesktop');
    });
GEN('#endif');


var CrSettingsMainPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_main_test.js';
  }
};

// Copied from Polymer 2 version of tests:
// Times out on Windows Tests (dbg). See https://crbug.com/651296.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/667882
// Flaky everywhere crbug.com/1197768
TEST_F('CrSettingsMainPageTest', 'DISABLED_MainPage', function() {
  mocha.run();
});

var CrSettingsAutofillPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_page_test.js';
  }
};

TEST_F('CrSettingsAutofillPageTest', 'All', function() {
  mocha.run();
});

var CrSettingsAutofillSectionCompanyEnabledTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_section_test.js';
  }
};

TEST_F('CrSettingsAutofillSectionCompanyEnabledTest', 'All', function() {
  mocha.run();
});

var CrSettingsPasswordsSectionTest = class extends CrSettingsBrowserTest {
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
TEST_F('CrSettingsPasswordsSectionTest', 'MAYBE_All', function() {
  mocha.run();
});
GEN('#undef MAYBE_All');

var CrSettingsPasswordsDeviceSectionTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_device_section_test.js';
  }
};

TEST_F('CrSettingsPasswordsDeviceSectionTest', 'All', function() {
  mocha.run();
});

var CrSettingsPasswordEditDialogTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_edit_dialog_test.js';
  }
};

TEST_F('CrSettingsPasswordEditDialogTest', 'All', function() {
  mocha.run();
});

var CrSettingsPasswordsCheckTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_check_test.js';
  }
};

// Flaky https://crbug.com/1143801
TEST_F('CrSettingsPasswordsCheckTest', 'DISABLED_All', function() {
  mocha.run();
});

var CrSettingsSafetyCheckPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_page_test.js';
  }
};

TEST_F('CrSettingsSafetyCheckPageTest', 'All', function() {
  mocha.run();
});

var CrSettingsSafetyCheckPermissionsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_permissions_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'content_settings::features::kSafetyCheckUnusedSitePermissions',
        'features::kSafetyCheckNotificationPermissions',
      ],
    };
  }
};

TEST_F('CrSettingsSafetyCheckPermissionsTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');
var CrSettingsSafetyCheckChromeCleanerTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_chrome_cleaner_test.js';
  }
};

TEST_F('CrSettingsSafetyCheckChromeCleanerTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

var CrSettingsSiteListTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_list_tests.js';
  }
};

// Copied from Polymer 2 test:
// TODO(crbug.com/929455): flaky, fix.
TEST_F('CrSettingsSiteListTest', 'DISABLED_SiteList', function() {
  runMochaSuite('SiteList');
});

// TODO(crbug.com/929455): When the bug is fixed, merge
// SiteListEmbargoedOrigin into SiteList.
TEST_F('CrSettingsSiteListTest', 'SiteListEmbargoedOrigin', function() {
  runMochaSuite('SiteListEmbargoedOrigin');
});

// TODO(crbug.com/929455): When the bug is fixed, merge
// SiteListCookiesExceptionTypes into SiteList.
TEST_F('CrSettingsSiteListTest', 'SiteListCookiesExceptionTypes', function() {
  runMochaSuite('SiteListCookiesExceptionTypes');
});

TEST_F('CrSettingsSiteListTest', 'EditExceptionDialog', function() {
  runMochaSuite('EditExceptionDialog');
});

TEST_F('CrSettingsSiteListTest', 'AddExceptionDialog', function() {
  runMochaSuite('AddExceptionDialog');
});

var CrSettingsSiteDetailsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_details_tests.js';
  }
};

// Disabling on debug due to flaky timeout on Win7 Tests (dbg)(1) bot.
// https://crbug.com/825304 - later for other platforms in crbug.com/1021219.
// Disabling on Linux CFI due to flaky timeout (crbug.com/1031960).
GEN('#if (!defined(NDEBUG)) || ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(IS_CFI))');
GEN('#define MAYBE_SiteDetails DISABLED_SiteDetails');
GEN('#else');
GEN('#define MAYBE_SiteDetails SiteDetails');
GEN('#endif');

TEST_F('CrSettingsSiteDetailsTest', 'MAYBE_SiteDetails', function() {
  mocha.run();
});

var CrSettingsPerformanceMenuTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_performance_menu_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'performance_manager::features::kHighEfficiencyModeAvailable',
        'performance_manager::features::kBatterySaverModeAvailable',
      ],
    };
  }
};

TEST_F('CrSettingsPerformanceMenuTest', 'All', function() {
  mocha.run();
});

var CrSettingsPerformancePageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/performance_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'performance_manager::features::kHighEfficiencyModeAvailable',
      ],
    };
  }
};

TEST_F('CrSettingsPerformancePageTest', 'All', function() {
  mocha.run();
});

var CrSettingsBatteryPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/battery_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'performance_manager::features::kBatterySaverModeAvailable',
      ],
    };
  }
};

TEST_F('CrSettingsBatteryPageTest', 'All', function() {
  mocha.run();
});

var CrSettingsTabDiscardExceptionDialogTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/tab_discard_exception_dialog_test.js';
  }
};

TEST_F('CrSettingsTabDiscardExceptionDialogTest', 'All', function() {
  mocha.run();
});

var CrSettingsPersonalizationOptionsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/personalization_options_test.js';
  }
};

TEST_F('CrSettingsPersonalizationOptionsTest', 'AllBuilds', function() {
  runMochaSuite('PersonalizationOptionsTests_AllBuilds');
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsPersonalizationOptionsTest', 'OfficialBuild', function() {
  runMochaSuite('PersonalizationOptionsTests_OfficialBuild');
});
GEN('#endif');

var CrSettingsPrivacyPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'features::kPrivacyGuide2',
      ],
    };
  }

  get featuresWithParameters() {
    return [{
      featureName: 'features::kFedCm',
      parameters: [{name: 'DesktopSettings', value: true}],
    }];
  }
};

// TODO(crbug.com/1351019): Flaky on Linux Tests(dbg).
GEN('#if BUILDFLAG(IS_LINUX)');
GEN('#define MAYBE_PrivacyPageTests DISABLED_PrivacyPageTests');
GEN('#else');
GEN('#define MAYBE_PrivacyPageTests PrivacyPageTests');
GEN('#endif');
TEST_F('CrSettingsPrivacyPageTest', 'MAYBE_PrivacyPageTests', function() {
  runMochaSuite('PrivacyPage');
});

TEST_F('CrSettingsPrivacyPageTest', 'PrivacySandboxEnabled', function() {
  runMochaSuite('PrivacySandboxEnabled');
});

TEST_F('CrSettingsPrivacyPageTest', 'PrivacyGuideRowTests', function() {
  runMochaSuite('PrivacyGuideRowTests');
});

TEST_F('CrSettingsPrivacyPageTest', 'PrivacyGuide2Disabled', function() {
  runMochaSuite('PrivacyGuide2Disabled');
});

TEST_F('CrSettingsPrivacyPageTest', 'NotificationPermissionReview', function() {
  runMochaSuite('NotificationPermissionReview');
});

// TODO(crbug.com/1043665): flaky crash on Linux Tests (dbg).
TEST_F(
    'CrSettingsPrivacyPageTest', 'DISABLED_PrivacyPageSoundTests', function() {
      runMochaSuite('PrivacyPageSound');
    });

// TODO(crbug.com/1113912): flaky failure on multiple platforms
TEST_F(
    'CrSettingsPrivacyPageTest', 'DISABLED_HappinessTrackingSurveysTests',
    function() {
      runMochaSuite('HappinessTrackingSurveys');
    });

GEN('#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)');
// TODO(crbug.com/1043665): disabling due to failures on several builders.
TEST_F(
    'CrSettingsPrivacyPageTest', 'DISABLED_CertificateManagerTests',
    function() {
      runMochaSuite('NativeCertificateManager');
    });
GEN('#endif');

var CrSettingsPrivacyPageWithPrivacySandbox4Test =
    class extends CrSettingsPrivacyPageTest {
  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'features::kPrivacyGuide2',
        'privacy_sandbox::kPrivacySandboxSettings4',
      ],
    };
  }
};

TEST_F(
    'CrSettingsPrivacyPageWithPrivacySandbox4Test', 'PrivacySandbox4Enabled',
    function() {
      runMochaSuite('PrivacySandbox4Enabled');
    });

var CrSettingsPrivacySandboxPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_sandbox_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'privacy_sandbox::kPrivacySandboxSettings4',
      ],
    };
  }
};

TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'PrivacySandboxPageTests', function() {
      runMochaSuite('PrivacySandboxPageTests');
    });

// TODO(crbug.com/1400768): Flaky on Mac.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_TopicsSubpageTests DISABLED_TopicsSubpageTests');
GEN('#else');
GEN('#define MAYBE_TopicsSubpageTests TopicsSubpageTests');
GEN('#endif');
TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'MAYBE_TopicsSubpageTests', function() {
      runMochaSuite('PrivacySandboxTopicsSubpageTests');
    });

TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'AdMeasurementSubpageTests',
    function() {
      runMochaSuite('PrivacySandboxAdMeasurementSubpageTests');
    });

var CrSettingsPrivacyGuidePageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_guide_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'features::kPrivacyGuide2',
      ],
    };
  }
};

TEST_F('CrSettingsPrivacyGuidePageTest', 'PrivacyGuidePageTests', function() {
  runMochaSuite('PrivacyGuidePageTests');
});

TEST_F('CrSettingsPrivacyGuidePageTest', 'MsbbFragmentNavigations', function() {
  runMochaSuite('MsbbFragmentNavigations');
});

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'HistorySyncFragmentNavigations',
    function() {
      runMochaSuite('HistorySyncFragmentNavigations');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'SafeBrowsingFragmentNavigations',
    function() {
      runMochaSuite('SafeBrowsingFragmentNavigations');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'CookiesFragmentNavigations', function() {
      runMochaSuite('CookiesFragmentNavigations');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'MsbbFragmentMetricsTests', function() {
      runMochaSuite('MsbbFragmentMetricsTests');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'HistorySyncFragmentMetricsTests',
    function() {
      runMochaSuite('HistorySyncFragmentMetricsTests');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'SafeBrowsingFragmentMetricsTests',
    function() {
      runMochaSuite('SafeBrowsingFragmentMetricsTests');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'CookiesFragmentMetricsTests',
    function() {
      runMochaSuite('CookiesFragmentMetricsTests');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'HistorySyncFragmentTests', function() {
      runMochaSuite('HistorySyncFragment');
    });

TEST_F('CrSettingsPrivacyGuidePageTest', 'CompletionFragmentTests', function() {
  runMochaSuite('CompletionFragment');
});

TEST_F(
    'CrSettingsPrivacyGuidePageTest',
    'CompletionFragmentPrivacySandboxRestricted', function() {
      runMochaSuite('CompletionFragmentPrivacySandboxRestricted');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest',
    'CompletionFragmentPrivacyGuide2DisabledTests', function() {
      runMochaSuite('CompletionFragmentPrivacyGuide2Disabled');
    });

TEST_F('CrSettingsPrivacyGuidePageTest', 'PrivacyGuideDialogTests', function() {
  runMochaSuite('PrivacyGuideDialog');
});

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'CardHeaderTestsPrivacyGuide2Enabled',
    function() {
      runMochaSuite('CardHeaderTestsPrivacyGuide2Enabled');
    });

TEST_F(
    'CrSettingsPrivacyGuidePageTest', 'CardHeaderTestsPrivacyGuide2Disabled',
    function() {
      runMochaSuite('CardHeaderTestsPrivacyGuide2Disabled');
    });

var CrSettingsCookiesPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/cookies_page_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'privacy_sandbox::kPrivacySandboxSettings4',
      ],
    };
  }
};

// Flaky on MacOS bots and times out on Linux Dbg: https://crbug.com/1240747
GEN('#if (BUILDFLAG(IS_MAC)) || (BUILDFLAG(IS_LINUX) && !defined(NDEBUG))');
GEN('#define MAYBE_CookiesPageTest DISABLED_CookiesPageTest');
GEN('#else');
GEN('#define MAYBE_CookiesPageTest CookiesPageTest');
GEN('#endif');
TEST_F('CrSettingsCookiesPageTest', 'MAYBE_CookiesPageTest', function() {
  runMochaSuite('CrSettingsCookiesPageTest');
});

TEST_F('CrSettingsCookiesPageTest', 'FirstPartySetsUIEnabled', function() {
  runMochaSuite('CrSettingsCookiesPageTest_FirstPartySetsUIEnabled');
});

GEN('#if BUILDFLAG(IS_CHROMEOS_LACROS)');
TEST_F('CrSettingsCookiesPageTest', 'LacrosSecondaryProfile', function() {
  runMochaSuite('CrSettingsCookiesPageTest_lacrosSecondaryProfile');
});
GEN('#endif');

GEN('#if (BUILDFLAG(IS_MAC)) || (BUILDFLAG(IS_LINUX) && !defined(NDEBUG))');
GEN('#define MAYBE_PrivacySandboxSettings4Disabled DISABLED_PrivacySandboxSettings4Disabled');
GEN('#else');
GEN('#define MAYBE_PrivacySandboxSettings4Disabled PrivacySandboxSettings4Disabled');
GEN('#endif');
TEST_F(
    'CrSettingsCookiesPageTest', 'MAYBE_PrivacySandboxSettings4Disabled',
    function() {
      runMochaSuite('PrivacySandboxSettings4Disabled');
    });
// #undef due to name collision with CrSettingsSiteSettingsPageTest.
GEN('#undef MAYBE_PrivacySandboxSettings4Disabled')

var CrSettingsRouteTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/route_tests.js';
  }
};

TEST_F('CrSettingsRouteTest', 'Basic', function() {
  runMochaSuite('route');
});

TEST_F('CrSettingsRouteTest', 'DynamicParameters', function() {
  runMochaSuite('DynamicParameters');
});

// Copied from Polymer 2 test:
// Failing on ChromiumOS dbg. https://crbug.com/709442
GEN('#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)) && !defined(NDEBUG)');
GEN('#define MAYBE_NonExistentRoute DISABLED_NonExistentRoute');
GEN('#else');
GEN('#define MAYBE_NonExistentRoute NonExistentRoute');
GEN('#endif');

TEST_F('CrSettingsRouteTest', 'MAYBE_NonExistentRoute', function() {
  runMochaSuite('NonExistentRoute');
});

var CrSettingsAdvancedPageTest = class extends CrSettingsBrowserTest {
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
TEST_F('CrSettingsAdvancedPageTest', 'MAYBE_Load', function() {
  mocha.run();
});

var CrSettingsReviewNotificationPermissionsTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/review_notification_permissions_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kSafetyCheckNotificationPermissions',
      ],
    };
  }
};

TEST_F('CrSettingsReviewNotificationPermissionsTest', 'All', function() {
  mocha.run();
});

var CrSettingsUnusedSitePermissionsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/unused_site_permissions_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'content_settings::features::kSafetyCheckUnusedSitePermissions',
      ],
    };
  }
};

TEST_F('CrSettingsUnusedSitePermissionsTest', 'All', function() {
  mocha.run();
});

var CrSettingsSiteSettingsPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_settings_page_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'privacy_sandbox::kPrivacySandboxSettings4',
        'content_settings::features::kSafetyCheckUnusedSitePermissions',
      ],
    };
  }
};

TEST_F('CrSettingsSiteSettingsPageTest', 'SiteSettingsPage', function() {
  mocha.run();
});

// TODO(crbug.com/1401833): Flaky.
GEN('#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)');
GEN('#define MAYBE_PrivacySandboxSettings4Disabled DISABLED_PrivacySandboxSettings4Disabled');
GEN('#else');
GEN('#define MAYBE_PrivacySandboxSettings4Disabled PrivacySandboxSettings4Disabled');
GEN('#endif');
TEST_F(
    'CrSettingsSiteSettingsPageTest', 'MAYBE_PrivacySandboxSettings4Disabled',
    function() {
      mocha.run();
    });
// #undef due to name collision with CrSettingsCookiesPageTest.
GEN('#undef MAYBE_PrivacySandboxSettings4Disabled')

TEST_F(
    'CrSettingsSiteSettingsPageTest', 'UnusedSitePermissionsReview',
    function() {
      mocha.run();
    });

TEST_F(
    'CrSettingsSiteSettingsPageTest', 'UnusedSitePermissionsReviewDisabled',
    function() {
      mocha.run();
    });

[['AppearanceFontsPage', 'appearance_fonts_page_test.js'],
 [
   'SettingsCategoryDefaultRadioGroup',
   'settings_category_default_radio_group_tests.js',
 ],
 ['CategoryDefaultSetting', 'category_default_setting_tests.js'],
 ['CategorySettingExceptions', 'category_setting_exceptions_tests.js'],
 ['Checkbox', 'checkbox_tests.js'],
 ['ChooserExceptionList', 'chooser_exception_list_tests.js'],
 ['ChooserExceptionListEntry', 'chooser_exception_list_entry_tests.js'],
 ['CollapseRadioButton', 'collapse_radio_button_tests.js'],
 ['ControlledButton', 'controlled_button_tests.js'],
 ['ControlledRadioButton', 'controlled_radio_button_tests.js'],
 ['DoNotTrackToggle', 'do_not_track_toggle_test.js'],
 ['DownloadsPage', 'downloads_page_test.js'],
 ['DropdownMenu', 'dropdown_menu_tests.js'],
 ['ExtensionControlledIndicator', 'extension_controlled_indicator_tests.js'],
 ['HelpPage', 'help_page_test.js'],
 ['Menu', 'settings_menu_test.js'],
 ['PasswordView', 'password_view_test.js'],
 ['PasswordsExportDialog', 'passwords_export_dialog_test.js'],
 ['PasswordsImportDialog', 'passwords_import_dialog_test.js'],
 ['PaymentsSection', 'payments_section_test.js'],
 ['PeoplePage', 'people_page_test.js'],
 ['PeoplePageSyncControls', 'people_page_sync_controls_test.js'],
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
 ['SecurityKeysBioEnrollment', 'security_keys_bio_enrollment_test.js'],
 [
   'SecurityKeysCredentialManagement',
   'security_keys_credential_management_test.js'
 ],
 ['SecurityKeysResetDialog', 'security_keys_reset_dialog_test.js'],
 ['SecurityKeysSetPinDialog', 'security_keys_set_pin_dialog_test.js'],
 ['SecurityKeysPhonesSubpage', 'security_keys_phones_subpage_test.js'],
 ['SecureDns', 'secure_dns_test.js'],
 ['SiteDetailsPermission', 'site_details_permission_tests.js'],
 ['SiteEntry', 'site_entry_tests.js'],
 ['SiteFavicon', 'site_favicon_test.js'],
 ['SiteListEntry', 'site_list_entry_tests.js'],
 ['Slider', 'settings_slider_tests.js'],
 ['StartupUrlsPage', 'startup_urls_page_test.js'],
 // Flaky on all OSes. TODO(crbug.com/1302405): Enable the test.
 ['Subpage', 'settings_subpage_test.js', 'DISABLED_All'],
 ['SyncAccountControl', 'sync_account_control_test.js'],
 ['ToggleButton', 'settings_toggle_button_tests.js'],
 ['ZoomLevels', 'zoom_levels_tests.js'],
].forEach(test => registerTest(...test));

// Timeout on Linux dbg bots: https://crbug.com/1133412
// Fails on Mac bots: https://crbug.com/1222886
GEN('#if !((BUILDFLAG(IS_LINUX) && !defined(NDEBUG)) || BUILDFLAG(IS_MAC))');
[['SecurityPage', 'security_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

// Timeout on Linux dbg bots: https://crbug.com/1311163
GEN('#if !(BUILDFLAG(IS_LINUX) && !defined(NDEBUG))');
[['AllSites', 'all_sites_tests.js']].forEach(test => registerTest(...test));
GEN('#endif');

// Timeout on Linux dbg bots: https://crbug.com/1394737
GEN('#if !(BUILDFLAG(IS_LINUX) && !defined(NDEBUG))');
[['PeoplePageSyncPage', 'people_page_sync_page_test.js']].forEach(
    test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(IS_CHROMEOS)');
[['PasswordsSectionCros', 'passwords_section_test_cros.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
[['PeoplePageChromeOS', 'people_page_test_cros.js'],
 // Copied from Polymer 2 test. TODO(crbug.com/929455): flaky, fix.
 ['SiteListChromeOS', 'site_list_tests_cros.js', 'DISABLED_AndroidSmsInfo'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH)');
[['EditDictionaryPage', 'edit_dictionary_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if !BUILDFLAG(IS_CHROMEOS)');
[['DefaultBrowser', 'default_browser_test.js'],
 ['ImportDataDialog', 'import_data_dialog_test.js'],
 ['SystemPage', 'system_page_tests.js'],
 // TODO(crbug.com/1350019) Test is flaky on ChromeOS
 ['AppearancePage', 'appearance_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
[['PeoplePageManageProfile', 'people_page_manage_profile_test.js'],
 ['Languages', 'languages_tests.js'],
 ['RelaunchConfirmationDialog', 'relaunch_confirmation_dialog_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)');
[['PasskeysSubpage', 'passkeys_subpage_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');
[['ChromeCleanupPage', 'chrome_cleanup_page_test.js'],
 ['IncompatibleApplicationsPage', 'incompatible_applications_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)');
registerTest('MetricsReporting', 'metrics_reporting_tests.js');
GEN('#endif');

function registerTest(testName, module, caseName) {
  const className = `CrSettings${testName}Test`;
  this[className] = class extends CrSettingsBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://settings/test_loader.html?module=settings/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
