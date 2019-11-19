// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings advanced page. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsAdvancedPageBrowserTest() {}

SettingsAdvancedPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,
};

// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
GEN('#endif');

TEST_F('SettingsAdvancedPageBrowserTest', 'MAYBE_Load', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  const self = this;

  // Register mocha tests.
  suite('SettingsPage', function() {
    suiteSetup(function() {
      self.toggleAdvanced();
    });

    test('load page', function() {
      // This will fail if there are any asserts or errors in the Settings page.
    });

    test('advanced pages', function() {
      const page = self.basicPage;
      const sections =
          ['privacy', 'languages', 'downloads', 'printing', 'reset'];
      for (let i = 0; i < sections.length; i++) {
        const section = self.getSection(page, sections[i]);
        assertTrue(!!section);
        self.verifySubpagesHidden(section);
      }
    });
  });

  // Run all registered tests.
  mocha.run();
});

// TODO(crbug/950007): Remove this when the kSplitSettings flag is on by
// default
GEN('#if defined(OS_CHROMEOS)');
GEN('#include "chromeos/constants/chromeos_features.h"');

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsAdvancedPageBrowserTestCrOS() {}

SettingsAdvancedPageBrowserTestCrOS.prototype = {
  __proto__: SettingsAdvancedPageBrowserTest.prototype,

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kSplitSettings']};
  }
};

// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
GEN('#endif');

TEST_F('SettingsAdvancedPageBrowserTestCrOS', 'MAYBE_Load', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  const self = this;

  // Register mocha tests.
  suite('SettingsPage', function() {
    suiteSetup(function() {
      self.toggleAdvanced();
    });

    test('load page', function() {
      // This will fail if there are any asserts or errors in the Settings page.
    });

    test('advanced pages', function() {
      const page = self.basicPage;
      const sections = [
        'privacy', 'languages', 'downloads', 'printing', 'reset', 'dateTime',
        'a11y'
      ];
      for (let i = 0; i < sections.length; i++) {
        const section = self.getSection(page, sections[i]);
        assertTrue(!!section);
        self.verifySubpagesHidden(section);
      }
    });
  });

  // Run all registered tests.
  mocha.run();
});
GEN('#endif');
