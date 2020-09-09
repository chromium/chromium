// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://scanning.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function ScanningAppBrowserTest() {}

ScanningAppBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://scanning/test_loader.html?module=chromeos/' +
      'scanning/scanning_app_test.js',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//ui/webui/resources/js/assert.js',
    '//ui/webui/resources/js/promise_resolver.js',
  ],

  featureList: {
    enabled: [
      'chromeos::features::kScanningUI',
    ]
  },
};

TEST_F('ScanningAppBrowserTest', 'All', function() {
  mocha.run();
});
