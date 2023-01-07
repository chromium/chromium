// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Desktop FRE intro web UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/signin/signin_features.h"')
GEN('#include "content/public/test/browser_test.h"');

class IntroBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }

  /** @override */
  get featureList() {
    return {enabled: ['kForYouFre']};
  }
}

/**
 * Test that the Sign in and Don't sign in buttons send the correct callback
 * when clicked in chrome/browser/resources/intro/dice_app.html
 */
var DiceAppTest = class extends IntroBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://intro/test_loader.html?module=' +
        'intro/dice_app_test.js';
  }
};

TEST_F('DiceAppTest', 'All', function() {
  mocha.run();
});