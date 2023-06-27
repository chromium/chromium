// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "build/build_config.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "build/config/coverage/buildflags.h"');
GEN('#include "chrome/browser/preloading/preloading_features.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/content_settings/core/common/features.h"');
GEN('#include "components/performance_manager/public/features.h"');
GEN('#include "components/privacy_sandbox/privacy_sandbox_features.h"');
GEN('#include "components/password_manager/core/common/password_manager_features.h"');
GEN('#include "components/autofill/core/common/autofill_features.h"');
GEN('#include "components/privacy_sandbox/privacy_sandbox_features.h"');
GEN('#include "content/public/common/content_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "components/permissions/features.h"');

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
    return 'chrome://settings/test_loader.html?module=settings/about_page_test.js';
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
    return 'chrome://settings/test_loader.html?module=settings/spell_check_page_test.js';
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
    return 'chrome://settings/test_loader.html?module=settings/languages_page_test.js';
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
    return 'chrome://settings/test_loader.html?module=settings/translate_page_test.js';
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

GEN('#if !BUILDFLAG(IS_CHROMEOS)');
var CrSettingsLiveCaptionSection = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/live_caption_section_test.js';
  }
};

TEST_F('CrSettingsLiveCaptionSection', 'LiveCaptionSection', function() {
  runMochaSuite('LiveCaptionSection');
});

var CrSettingsLiveTranslateSection = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/live_translate_section_test.js';
  }
};

TEST_F('CrSettingsLiveTranslateSection', 'LiveTranslateSection', function() {
  runMochaSuite('LiveTranslateSection');
});
GEN('#endif');

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

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
TEST_F(
    'CrSettingsClearBrowsingDataTest', 'ClearBrowsingDataDesktop', function() {
      runMochaSuite('ClearBrowsingDataDesktop');
    });
GEN('#endif');
TEST_F(
    'CrSettingsClearBrowsingDataTest', 'ClearBrowsingDataForSupervisedUsers',
    function() {
      runMochaSuite('ClearBrowsingDataDesktop');
    });

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

// TODO(crbug.com/1420597): Clean up this test after Password Manager redesign
// is launched.
var CrSettingsAutofillPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {disabled: ['password_manager::features::kPasswordManagerRedesign']};
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

// TODO(crbug.com/1420597): remove this test after Password Manager redesign is
// launched.
var CrSettingsPasswordsSectionTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_section_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {disabled: ['password_manager::features::kPasswordManagerRedesign']};
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

// TODO(crbug.com/1420597): remove this test after Password Manager redesign is
// launched.
var CrSettingsPasswordsDeviceSectionTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_device_section_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {disabled: ['password_manager::features::kPasswordManagerRedesign']};
  }
};

TEST_F('CrSettingsPasswordsDeviceSectionTest', 'All', function() {
  mocha.run();
});

// TODO(crbug.com/1420597): remove this test after Password Manager redesign is
// launched.
var CrSettingsPasswordEditDialogTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_edit_dialog_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {disabled: ['password_manager::features::kPasswordManagerRedesign']};
  }
};

TEST_F('CrSettingsPasswordEditDialogTest', 'All', function() {
  mocha.run();
});

// TODO(crbug.com/1420597): remove this test after Password Manager redesign is
// launched.
var CrSettingsPasswordsCheckTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_check_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {disabled: ['password_manager::features::kPasswordManagerRedesign']};
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

  /** @override */
  get featureListInternal() {
    // TODO(crbug.com/1420597): Clean up this after Password Manager redesign is
    // launched.
    return {disabled: ['password_manager::features::kPasswordManagerRedesign']};
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

var CrSettingsSiteListTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_list_test.js';
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

TEST_F('CrSettingsSiteListTest', 'SiteList', function() {
  runMochaSuite('SiteList');
});

// TODO(crbug.com/929455, crbug.com/1064002): Flaky test. When it is fixed,
// merge SiteListDisabled back into SiteList.
TEST_F('CrSettingsSiteListTest', 'DISABLED_SiteListDisabled', function() {
  runMochaSuite('DISABLED_SiteList');
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

TEST_F('CrSettingsSiteListTest', 'SiteListSearchTests', function() {
  runMochaSuite('SiteListSearchTests');
});

TEST_F('CrSettingsSiteListTest', 'EditExceptionDialog', function() {
  runMochaSuite('EditExceptionDialog');
});

TEST_F('CrSettingsSiteListTest', 'AddExceptionDialog', function() {
  runMochaSuite('AddExceptionDialog');
});

// TODO(crbug.com/1378703): Remove after crbug/1378703 launched.
TEST_F(
    'CrSettingsSiteListTest', 'AddExceptionDialog_PrivacySandbox4Disabled',
    function() {
      runMochaSuite('AddExceptionDialog_PrivacySandbox4Disabled');
    });

var CrSettingsSiteDetailsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_details_test.js';
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
};

TEST_F('CrSettingsPerformanceMenuTest', 'All', function() {
  mocha.run();
});

var CrSettingsPerformancePageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/performance_page_test.js';
  }
};

TEST_F('CrSettingsPerformancePageTest', 'Controls', function() {
  runMochaSuite('PerformancePage');
});

TEST_F('CrSettingsPerformancePageTest', 'ExceptionList', function() {
  runMochaSuite('TabDiscardExceptionList');
});

var CrSettingsPerformancePageMultistateTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/performance_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'performance_manager::features::kHighEfficiencyMultistateMode',
      ],
    };
  }
};

TEST_F('CrSettingsPerformancePageMultistateTest', 'Controls', function() {
  runMochaSuite('PerformancePageMultistate');
});

TEST_F('CrSettingsPerformancePageMultistateTest', 'ExceptionList', function() {
  runMochaSuite('TabDiscardExceptionList');
});

var CrSettingsPerformancePageDiscardExceptionImprovementsTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/performance_page_test.js';
  }

  /** @override */
  get featureListInternal() {
    return {
      enabled: [
        'performance_manager::features::kDiscardExceptionsImprovements',
      ],
    };
  }
};

TEST_F(
    'CrSettingsPerformancePageDiscardExceptionImprovementsTest', 'Controls',
    function() {
      runMochaSuite('PerformancePage');
    });

TEST_F(
    'CrSettingsPerformancePageDiscardExceptionImprovementsTest',
    'ExceptionList', function() {
      runMochaSuite('TabDiscardExceptionList');
    });

var CrSettingsBatteryPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/battery_page_test.js';
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
        'privacy_sandbox::kPrivacySandboxSettings4',
        'permissions::features::kPermissionStorageAccessAPI',
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

// TODO(crbug.com/1378703): Remove once PrivacySandboxSettings4 has been rolled
// out.
TEST_F('CrSettingsPrivacyPageTest', 'PrivacySandbox4Disabled', function() {
  runMochaSuite('PrivacySandbox4Disabled');
});

TEST_F('CrSettingsPrivacyPageTest', 'PrivacySandbox4Enabled', function() {
  runMochaSuite('PrivacySandbox4Enabled');
});

TEST_F('CrSettingsPrivacyPageTest', 'PrivacyGuideRowTests', function() {
  runMochaSuite('PrivacyGuideRowTests');
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

TEST_F(
    'CrSettingsPrivacyPageTest', 'enableWebBluetoothNewPermissionsBackendTests',
    function() {
      runMochaSuite('enableWebBluetoothNewPermissionsBackend');
    });

var CrSettingsPrivacyPagePrivacySandboxRestrictedTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_page_test.js';
  }
  get featuresWithParameters() {
    return [{
      featureName: 'privacy_sandbox::kPrivacySandboxSettings4',
      parameters: [{name: 'force-restricted-user', value: true}]
    }];
  }
};

TEST_F(
    'CrSettingsPrivacyPagePrivacySandboxRestrictedTest', 'Restricted',
    function() {
      runMochaSuite('PrivacySandbox4EnabledButRestricted');
    });

var CrSettingsPrivacyPagePrivacySandboxRestrictedWithNoticeTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_page_test.js';
  }

  get featuresWithParameters() {
    return [{
      featureName: 'privacy_sandbox::kPrivacySandboxSettings4',
      parameters: [
        {name: 'force-restricted-user', value: true},
        {name: 'restricted-notice', value: true}
      ]
    }];
  }
};

TEST_F(
    'CrSettingsPrivacyPagePrivacySandboxRestrictedWithNoticeTest',
    'RestrictedWithNotice', function() {
      runMochaSuite('PrivacySandbox4EnabledButRestrictedWithNotice');
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

TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'PrivacySandboxRestrictedEnabledTests',
    function() {
      runMochaSuite('PrivacySandboxRestrictedEnabledTests');
    });

TEST_F('CrSettingsPrivacySandboxPageTest', 'TopicsSubpageTests', function() {
  runMochaSuite('PrivacySandboxTopicsSubpageTests');
});

TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'TopicsSubpageEmptyTests', function() {
      runMochaSuite('PrivacySandboxTopicsSubpageEmptyTests');
    });

TEST_F('CrSettingsPrivacySandboxPageTest', 'FledgeSubpageTests', function() {
  runMochaSuite('PrivacySandboxFledgeSubpageTests');
});

TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'FledgeSubpageEmptyTests', function() {
      runMochaSuite('PrivacySandboxFledgeSubpageEmptyTests');
    });

TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'FledgeSubpageSeeAllSitesTests',
    function() {
      runMochaSuite('PrivacySandboxFledgeSubpageSeeAllSitesTests');
    });

TEST_F(
    'CrSettingsPrivacySandboxPageTest', 'AdMeasurementSubpageTests',
    function() {
      runMochaSuite('PrivacySandboxAdMeasurementSubpageTests');
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
        'privacy_sandbox::kPrivacySandboxFirstPartySetsUI',
        'features::kPreloadingDesktopSettingsSubPage',
      ],
    };
  }
};

GEN('#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !defined(NDEBUG)) || BUILDFLAG(USE_JAVASCRIPT_COVERAGE)');
GEN('#define MAYBE_CookiesPageTest DISABLED_CookiesPageTest');
GEN('#else');
GEN('#define MAYBE_CookiesPageTest CookiesPageTest');
GEN('#endif');
// TODO(crbug.com/1409653): fix flakiness on Linux and ChromeOS debug and
// Javascript code coverage builds and re-enable.
TEST_F('CrSettingsCookiesPageTest', 'MAYBE_CookiesPageTest', function() {
  runMochaSuite('CrSettingsCookiesPageTest');
});

TEST_F('CrSettingsCookiesPageTest', 'FirstPartySetsUIDisabled', function() {
  runMochaSuite('CrSettingsCookiesPageTest_FirstPartySetsUIDisabled');
});

GEN('#if BUILDFLAG(IS_CHROMEOS_LACROS)');
TEST_F('CrSettingsCookiesPageTest', 'LacrosSecondaryProfile', function() {
  runMochaSuite('CrSettingsCookiesPageTest_lacrosSecondaryProfile');
});
GEN('#endif');

GEN('#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG)) || BUILDFLAG(USE_JAVASCRIPT_COVERAGE)');
GEN('#define MAYBE_PrivacySandboxSettings4Disabled2 DISABLED_PrivacySandboxSettings4Disabled');
GEN('#else');
GEN('#define MAYBE_PrivacySandboxSettings4Disabled2 PrivacySandboxSettings4Disabled');
GEN('#endif');
// TODO(crbug.com/1409653): fix flakiness on Linux debug and Javascript code
// coverage builds and re-enable.
// The "MAYBE..." portion of the test has a 2 at the end because there is
// already a macro with the same name defined in this file.
TEST_F(
    'CrSettingsCookiesPageTest', 'MAYBE_PrivacySandboxSettings4Disabled2',
    function() {
      runMochaSuite(
          'CrSettingsCookiesPageTest_PrivacySandboxSettings4Disabled');
    });

TEST_F(
    'CrSettingsCookiesPageTest', 'PreloadingDesktopSettingsSubPageDisabled',
    function() {
      runMochaSuite('PreloadingDesktopSettingsSubPageDisabled');
    });

var CrSettingsRouteTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/route_test.js';
  }
};

TEST_F('CrSettingsRouteTest', 'Basic', function() {
  runMochaSuite('route');
});

TEST_F('CrSettingsRouteTest', 'DynamicParameters', function() {
  runMochaSuite('DynamicParameters');
});

TEST_F('CrSettingsRouteTest', 'SafetyHubReachableTests', function() {
  runMochaSuite('SafetyHubReachableTests');
});

TEST_F('CrSettingsRouteTest', 'SafetyHubNotReachableTests', function() {
  runMochaSuite('SafetyHubNotReachableTests');
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
        'permissions::features::kPermissionStorageAccessAPI',
      ],
    };
  }
};

// TODO(crbug.com/1401833): Flaky.
GEN('#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)');
GEN('#define MAYBE_SiteSettingsPage DISABLED_SiteSettingsPage');
GEN('#else');
GEN('#define MAYBE_SiteSettingsPage SiteSettingsPage');
GEN('#endif');
TEST_F('CrSettingsSiteSettingsPageTest', 'MAYBE_SiteSettingsPage', function() {
  runMochaSuite('SiteSettingsPage');
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
      runMochaSuite('PrivacySandboxSettings4Disabled');
    });

// TODO(crbug.com/1401833): Flaky.
GEN('#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)');
GEN('#define MAYBE_UnusedSitePermissionsReview DISABLED_UnusedSitePermissionsReview');
GEN('#else');
GEN('#define MAYBE_UnusedSitePermissionsReview UnusedSitePermissionsReview');
GEN('#endif');
TEST_F(
    'CrSettingsSiteSettingsPageTest', 'MAYBE_UnusedSitePermissionsReview',
    function() {
      runMochaSuite('UnusedSitePermissionsReview');
    });

// TODO(crbug.com/1401833): Flaky.
GEN('#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)');
GEN('#define MAYBE_UnusedSitePermissionsReviewDisabled DISABLED_UnusedSitePermissionsReviewDisabled');
GEN('#else');
GEN('#define MAYBE_UnusedSitePermissionsReviewDisabled UnusedSitePermissionsReviewDisabled');
GEN('#endif');
TEST_F(
    'CrSettingsSiteSettingsPageTest',
    'MAYBE_UnusedSitePermissionsReviewDisabled', function() {
      runMochaSuite('UnusedSitePermissionsReviewDisabled');
    });

TEST_F(
    'CrSettingsSiteSettingsPageTest', 'PermissionStorageAccessApiDisabled',
    function() {
      runMochaSuite('PermissionStorageAccessApiDisabled');
    });

var CrSettingsMenuTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_menu_test.js';
  }
};

TEST_F('CrSettingsMenuTest', 'All', function() {
  mocha.run()
});

[['AppearanceFontsPage', 'appearance_fonts_page_test.js'],
 [
   'SettingsCategoryDefaultRadioGroup',
   'settings_category_default_radio_group_test.js',
 ],
 ['AntiAbusePage', 'anti_abuse_page_test.js'],
 ['CategorySettingExceptions', 'category_setting_exceptions_test.js'],
 ['Checkbox', 'checkbox_test.js'],
 ['ChooserExceptionList', 'chooser_exception_list_test.js'],
 ['ChooserExceptionListEntry', 'chooser_exception_list_entry_test.js'],
 ['CollapseRadioButton', 'collapse_radio_button_test.js'],
 ['ControlledButton', 'controlled_button_test.js'],
 ['ControlledRadioButton', 'controlled_radio_button_test.js'],
 ['AutofillAddressValidation', 'autofill_section_address_validation_test.js'],
 ['DoNotTrackToggle', 'do_not_track_toggle_test.js'],
 ['DownloadsPage', 'downloads_page_test.js'],
 ['DropdownMenu', 'dropdown_menu_test.js'],
 ['ExtensionControlledIndicator', 'extension_controlled_indicator_test.js'],
 ['FileSystemSettingsList', 'file_system_site_list_test.js'],
 ['FileSystemSettingsListEntries', 'file_system_site_entry_test.js'],
 ['FileSystemSettingsListEntryItems', 'file_system_site_entry_item_test.js'],
 ['HelpPage', 'help_page_test.js'],
 // TODO(crbug.com/1420597): Remove this test after Password Manager redesign is
 // launched.
 ['PasswordView', 'password_view_test.js'],
 // TODO(crbug.com/1420597): Remove this test after Password Manager redesign is
 // launched.
 ['PasswordsExportDialog', 'passwords_export_dialog_test.js'],
 // TODO(crbug.com/1420597): Remove this test after Password Manager redesign is
 // launched.
 ['PasswordsImportDialog', 'passwords_import_dialog_test.js'],
 ['PaymentsSection', 'payments_section_test.js'],
 ['PaymentsSectionCardDialogs', 'payments_section_card_dialogs_test.js'],
 ['PaymentsSectionCardRows', 'payments_section_card_rows_test.js'],
 ['PaymentsSectionIban', 'payments_section_iban_test.js'],
 ['PaymentsSectionUpi', 'payments_section_upi_test.js'],
 ['PeoplePage', 'people_page_test.js'],
 ['PeoplePageSyncControls', 'people_page_sync_controls_test.js'],
 ['PreloadingPage', 'preloading_page_test.js'],
 ['ProtocolHandlers', 'protocol_handlers_test.js'],
 ['RecentSitePermissions', 'recent_site_permissions_test.js'],
 // Flaky on all OSes. TODO(crbug.com/1127733): Enable the test.
 ['ResetPage', 'reset_page_test.js', 'DISABLED_All'],
 ['ResetProfileBanner', 'reset_profile_banner_test.js'],
 ['SafetyHub', 'safety_hub_test.js'],
 ['SearchEngines', 'search_engines_page_test.js'],
 ['SearchPage', 'search_page_test.js'],
 ['Search', 'search_settings_test.js'],
 ['Section', 'settings_section_test.js'],
 ['SecurityKeysBioEnrollment', 'security_keys_bio_enrollment_test.js'],
 [
   'SecurityKeysCredentialManagement',
   'security_keys_credential_management_test.js'
 ],
 ['SecurityKeysResetDialog', 'security_keys_reset_dialog_test.js'],
 ['SecurityKeysSetPinDialog', 'security_keys_set_pin_dialog_test.js'],
 ['SecurityKeysPhonesSubpage', 'security_keys_phones_subpage_test.js'],
 ['SecureDns', 'secure_dns_test.js'],
 ['SimpleConfirmationDialog', 'simple_confirmation_dialog_test.js'],
 ['SiteDataTest', 'site_data_test.js'],
 ['SiteDetailsPermission', 'site_details_permission_test.js'],
 [
   'SiteDetailsPermissionDeviceEntry',
   'site_details_permission_device_entry_test.js'
 ],
 ['SiteEntry', 'site_entry_test.js'],
 ['SiteFavicon', 'site_favicon_test.js'],
 ['SiteListEntry', 'site_list_entry_test.js'],
 ['Slider', 'settings_slider_test.js'],
 ['StartupUrlsPage', 'startup_urls_page_test.js'],
 // Flaky on all OSes. TODO(crbug.com/1302405): Enable the test.
 ['Subpage', 'settings_subpage_test.js', 'DISABLED_All'],
 ['SyncAccountControl', 'sync_account_control_test.js'],
 ['ToggleButton', 'settings_toggle_button_test.js'],
 ['ZoomLevels', 'zoom_levels_test.js'],
].forEach(test => registerTest(...test));

// Timeout on Linux dbg bots: https://crbug.com/1394737
GEN('#if !(BUILDFLAG(IS_LINUX) && !defined(NDEBUG))');
[['PeoplePageSyncPage', 'people_page_sync_page_test.js']].forEach(
    test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(IS_CHROMEOS)');
// TODO(crbug.com/1420597): Remove this test after Password Manager redesign is
// launched.
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
 ['SystemPage', 'system_page_test.js'],
 // TODO(crbug.com/1350019) Test is flaky on ChromeOS
 ['AppearancePage', 'appearance_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
[['PeoplePageManageProfile', 'people_page_manage_profile_test.js'],
 ['Languages', 'languages_test.js'],
 ['RelaunchConfirmationDialog', 'relaunch_confirmation_dialog_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)');
[['PasskeysSubpage', 'passkeys_subpage_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)');
registerTest('MetricsReporting', 'metrics_reporting_test.js');
GEN('#endif');

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
registerTest('GetMostChromePage', 'get_most_chrome_page_test.js');
GEN('#endif');

function registerTest(testName, module, caseName) {
  const className = `CrSettings${testName}Test`;
  this[className] = class extends CrSettingsBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://settings/test_loader.html?module=settings/${module}`;
    }

    /** @override */
    get featureListInternal() {
      return {
        disabled: ['password_manager::features::kPasswordManagerRedesign']
      };
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}

// Some tests files are too large to run as a single "All" test (e.g. as above),
// and flake on some bots. Each test suite can instead be run as an individual
// test fixture, allowing more time to complete.
[[
  'SecurityPage', 'security_page_test.js',
  [
    'SecurityPage',
    'SecurityPage_FlagsDisabled',
  ]
],
 [
   'AllSites',
   'all_sites_test.js',
   [
     'AllSites_EnableFirstPartySets',
     'AllSites_DisableFirstPartySets',
   ],
 ],
 [
   'PrivacyGuidePage',
   'privacy_guide_page_test.js',
   [
     'PrivacyGuidePageTests',
     'MsbbCardNavigations',
     'HistorySyncCardNavigations',
     'SafeBrowsingCardNavigations',
     'CookiesCardNavigations',
     'PrivacyGuideDialogTests',
   ],
 ],
 [
   'PrivacyGuideFragments',
   'privacy_guide_fragments_test.js',
   [
     'WelcomeFragmentTests',
     'MsbbFragmentTests',
     'HistorySyncFragmentTests',
     'SafeBrowsingFragmentTests',
     'CookiesFragmentTests',
     'CompletionFragmentTests',
     'CompletionFragmentPrivacySandboxRestricted',
   ],
 ],
].forEach(test => registerTestSuites(...test));

// TODO(https://crbug.com/1426530): Re-enable when no longer flaky.
GEN('#if !BUILDFLAG(IS_LINUX) || defined(NDEBUG)');
registerTestSuites(
    'PrivacyGuideIntegration', 'privacy_guide_integration_test.js',
    ['PrivacyGuideEligibleReachedMetricsTests']);
GEN('#endif');

// TODO(crbug.com/1403969): SecurityPage_SafeBrowsing suite is flaky on Mac.
// TODO(crbug.com/1404109): SecurityPage_SafeBrowsing suite is flaky on Linux.
GEN('#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX)');
registerTestSuites(
    'SecurityPage', 'security_page_test.js', ['SecurityPage_SafeBrowsing']);
GEN('#endif');

function registerTestSuites(testName, module, suites) {
  const className = `CrSettings${testName}Test`;
  // The classname may have already been registered, such as if some suites only
  // run on some platforms.
  if (!this[className]) {
    this[className] = class extends CrSettingsBrowserTest {
      /** @override */
      get browsePreload() {
        return `chrome://settings/test_loader.html?module=settings/${module}`;
      }
    };
  }
  suites.forEach((suite) => {
    TEST_F(className, suite, () => runMochaSuite(suite));
  })
}
