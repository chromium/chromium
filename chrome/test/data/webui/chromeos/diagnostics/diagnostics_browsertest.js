// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://diagnostics.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

[['DiagnosticsApp', 'diagnostics/diagnostics_test.js']].forEach(
    test => registerTest(...test));

function registerTest(testName, module) {
  const className = `${testName}BrowserTest`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      return `chrome://diagnostics/test_loader.html?module=chromeos/${module}`;
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
          'chromeos::features::kDiagnosticsApp',
        ],
      };
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
