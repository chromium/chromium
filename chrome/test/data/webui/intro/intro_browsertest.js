// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Desktop FRE intro web UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/signin/signin_features.h"')
GEN('#include "components/signin/public/base/signin_buildflags.h"');
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

GEN('#if BUILDFLAG(ENABLE_DICE_SUPPORT)');
/**
 * Test that the Sign in and Don't sign in buttons behave correctly when clicked
 * in chrome/browser/resources/intro/sign_in_promo.html
 */
var SignInPromoTest = class extends IntroBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://intro/test_loader.html?module=' +
        'intro/sign_in_promo_test.js';
  }
};

TEST_F('SignInPromoTest', 'All', function() {
  mocha.run();
});

/**
  Test that the correct view is loaded when we add or remove the
  noAnimation query parameter
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
GEN('#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)');

/**
  Tests intro/lacros_app.ts
 */
GEN('#if BUILDFLAG(IS_CHROMEOS_LACROS)');
var LacrosAppTest = class extends IntroBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://intro/test_loader.html?module=' +
        'intro/lacros_app_test.js';
  }
};

TEST_F('LacrosAppTest', 'All', function() {
  mocha.run();
});
GEN('#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)');
