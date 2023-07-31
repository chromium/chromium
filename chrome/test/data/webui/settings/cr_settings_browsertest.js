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
GEN('#include "components/privacy_sandbox/privacy_sandbox_features.h"');
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
        'features::kSafetyHub',
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

TEST_F('CrSettingsSiteSettingsPageTest', 'SafetyHubDisabled', function() {
  runMochaSuite('SafetyHubDisabled');
});

// TODO(crbug.com/1403969): SecurityPage_SafeBrowsing suite is flaky on Mac.
// TODO(crbug.com/1404109): SecurityPage_SafeBrowsing suite is flaky on Linux.
GEN('#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX)');
registerTestSuites('SecurityPage', 'security_page_test.js', ['SafeBrowsing']);
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
