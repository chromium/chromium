// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for shared Polymer 3 elements. */
const OSSettingsBrowserTest = class extends PolymerTest {
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

[[
  'OsPeoplePageAccountManagerSubpageWithArcAccountRestrictionsEnabled',
  'os_people_page/account_manager_subpage_test.js',
  {
    enabled: [
      'ash::standalone_browser::features::kLacrosOnly',
      'ash::standalone_browser::features::kLacrosProfileMigrationForceOff',
    ],
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

  TEST_F(className, 'All', () => mocha.run());
}
