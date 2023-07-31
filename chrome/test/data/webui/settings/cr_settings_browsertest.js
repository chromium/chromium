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
