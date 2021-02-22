// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OSSettingsAccessibilityV3Test fixture.
GEN_INCLUDE([
  'os_settings_accessibility_v3_test.js',
]);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "content/public/test/browser_test.h"');

// TODO(crbug.com/1002627): This block prevents generation of a
// link-in-text-block browser-test. This can be removed once the bug is
// addressed, and usage should be replaced with
// OSSettingsAccessibilityV3Test.axeOptions
const axeOptionsExcludeLinkInTextBlock =
    Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions, {
      'rules':
          Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions.rules, {
            'link-in-text-block': {enabled: false},
          })
    });

// TODO(crbug.com/1180696): This block prevents generation of a
// document-title browser-test. This can be removed once the bug is
// addressed, and usage should be replaced with
// OSSettingsAccessibilityV3Test.axeOptions
const axeOptionsDocumentTitle =
    Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions, {
      'rules':
          Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions.rules, {
            'document-title': {enabled: false},
          })
    });

[[
  'Basic', 'basic_a11y_v3_test.js', {options: axeOptionsExcludeLinkInTextBlock}
],
 [
   'GoogleAssistant', 'google_assistant_a11y_v3_test.js',
   {options: axeOptionsDocumentTitle}
 ],
 [
   'ManageAccessibility', 'manage_accessibility_a11y_v3_test.js',
   {options: axeOptionsDocumentTitle}
 ],
].forEach(test => defineTest(...test));

function defineTest(testName, module, config) {
  const className = `OSSettingsA11y${testName}V3`;
  this[className] = class extends OSSettingsAccessibilityV3Test {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/a11y/${
          module}`;
    }
  };

  const filter = config && config.filter ?
      config.filter :
      OSSettingsAccessibilityV3Test.violationFilter;
  const options = config && config.options ?
      config.options :
      OSSettingsAccessibilityV3Test.axeOptions;
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