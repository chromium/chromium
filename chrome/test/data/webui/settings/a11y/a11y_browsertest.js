// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SettingsAccessibilityV3Test fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "build/build_config.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "content/public/test/browser_test.h"');

// TODO(crbug.com/1002627): This block prevents generation of a
// link-in-text-block browser-test. This can be removed once the bug is
// addressed, and usage should be replaced with
// SettingsAccessibilityV3Test.axeOptions
const axeOptionsExcludeLinkInTextBlock =
    Object.assign({}, SettingsAccessibilityV3Test.axeOptions, {
      'rules': Object.assign({}, SettingsAccessibilityV3Test.axeOptions.rules, {
        'link-in-text-block': {enabled: false},
      }),
    });

const violationFilterExcludeCustomInputAndTabindex =
    Object.assign({}, SettingsAccessibilityV3Test.violationFilter, {
      // Excuse custom input elements.
      'aria-valid-attr-value': function(nodeResult) {
        const describerId = nodeResult.element.getAttribute('aria-describedby');
        return describerId === '' && nodeResult.element.tagName === 'INPUT';
      },
      'tabindex': function(nodeResult) {
        // TODO(crbug.com/808276): remove this exception when bug is fixed.
        return nodeResult.element.getAttribute('tabindex') === '0';
      },
    });

[['About', 'about_a11y_test.js', {options: axeOptionsExcludeLinkInTextBlock}],
 ['Accessibility', 'accessibility_a11y_test.js'],
 ['Basic', 'basic_a11y_test.js'],
 ['Passwords', 'passwords_a11y_test.js'],
].forEach(test => defineTest(...test));

GEN('#if !BUILDFLAG(IS_CHROMEOS)');
[[
  'ManageProfile',
  'manage_profile_a11y_test.js',
  {filter: violationFilterExcludeCustomInputAndTabindex},
],
 ['Signout', 'sign_out_a11y_test.js'],
].forEach(test => defineTest(...test));
GEN('#endif');

function defineTest(testName, module, config) {
  const className = `SettingsA11y${testName}V3`;
  this[className] = class extends SettingsAccessibilityV3Test {
    /** @override */
    get browsePreload() {
      return `chrome://settings/test_loader.html?module=settings/a11y/${
          module}`;
    }
  };

  const filter = config && config.filter ?
      config.filter :
      SettingsAccessibilityV3Test.violationFilter;
  const options = config && config.options ?
      config.options :
      SettingsAccessibilityV3Test.axeOptions;
  AccessibilityTest.define(className, {
    /** @override */
    name: testName,
    /** @override */
    axeOptions: options,
    /** @override */
    tests: {'All': function() {}},
    /** @override */
    violationFilter: filter,
  });
}
