// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "build/chromeos_buildflags.h"');

/**
 * Test fixture for shared Polymer elements.
 * @constructor
 * @extends {PolymerTest}
 */
function CrElementsBrowserTest() {}

CrElementsBrowserTest.prototype = {
  __proto__: Polymer2DeprecatedTest.prototype,

  /** @override */
  extraLibraries: [
    ...Polymer2DeprecatedTest.prototype.extraLibraries,
    '//ui/webui/resources/js/assert.js',
  ],

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  },
};

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsLazyRenderTest() {}

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
CrElementsLazyRenderTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat(
      ['cr_lazy_render_tests.js']),
};

TEST_F('CrElementsLazyRenderTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsScrollableBehaviorTest() {}

CrElementsScrollableBehaviorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_scrollable_behavior.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_scrollable_behavior_tests.js',
  ]),
};

TEST_F('CrElementsScrollableBehaviorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsContainerShadowBehaviorTest() {}

CrElementsContainerShadowBehaviorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_container_shadow_behavior.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_container_shadow_behavior_test.js',
  ]),
};

TEST_F('CrElementsContainerShadowBehaviorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsFingerprintProgressArcTest() {}

CrElementsFingerprintProgressArcTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_fingerprint/' +
      'cr_fingerprint_progress_arc.html',

  /** @override */
  commandLineSwitches: [{
    switchName: 'enable-pixel-output-in-tests',
  }],

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../mock_controller.js',
    'cr_fingerprint_progress_arc_tests.js',
  ]),
};

// https://crbug.com/1044390 - maybe flaky on Mac?
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_Fingerprint DISABLED_Fingerprint');
GEN('#else');
GEN('#define MAYBE_Fingerprint Fingerprint');
GEN('#endif');

TEST_F('CrElementsFingerprintProgressArcTest', 'MAYBE_Fingerprint', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsDialogTest() {}

CrElementsDialogTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_dialog/cr_dialog.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_dialog_test.js',
  ]),
};

TEST_F('CrElementsDialogTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsRadioButtonTest() {}

CrElementsRadioButtonTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_radio_button_test.js',
  ]),
};

TEST_F('CrElementsRadioButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsCardRadioButtonTest() {}

CrElementsCardRadioButtonTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_radio_button/' +
      'cr_card_radio_button.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_card_radio_button_test.js',
  ]),
};

TEST_F('CrElementsCardRadioButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsRadioGroupTest() {}

CrElementsRadioGroupTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_radio_group_test.js',
  ]),
};

TEST_F('CrElementsRadioGroupTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsButtonTest() {}

CrElementsButtonTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_button/cr_button.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_button_tests.js',
  ]),
};

TEST_F('CrElementsButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsIconButtonTest() {}

CrElementsIconButtonTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_icon_button_tests.js',
  ]),
};

TEST_F('CrElementsIconButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsLottieTest() {}

CrElementsLottieTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_lottie/cr_lottie.html',

  /** @override */
  commandLineSwitches: [{
    switchName: 'enable-pixel-output-in-tests',
  }],

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../mock_controller.js',
    '../test_util.js',
    'cr_lottie_tests.js',
  ]),
};

TEST_F('CrElementsLottieTest', 'All', function() {
  mocha.run();
});
