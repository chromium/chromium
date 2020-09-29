// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for shared Polymer elements.
 * @constructor
 * @extends {PolymerTest}
 */
function CrElementsBrowserTest() {}

CrElementsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
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
function CrElementsSearchFieldTest() {}

CrElementsSearchFieldTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_search_field/cr_search_field.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_search_field_tests.js',
  ]),
};

TEST_F('CrElementsSearchFieldTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsToolbarTest() {}

CrElementsToolbarTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_toolbar_tests.js',
  ]),
};

TEST_F('CrElementsToolbarTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsDrawerTest() {}

CrElementsDrawerTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_drawer/cr_drawer.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_drawer_tests.js',
  ]),
};

// https://crbug.com/1096016 - flaky on Mac
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_Drawer DISABLED_Drawer');
GEN('#else');
GEN('#define MAYBE_Drawer Drawer');
GEN('#endif');
TEST_F('CrElementsDrawerTest', 'MAYBE_Drawer', function() {
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
function CrElementsPolicyIndicatorTest() {}

CrElementsPolicyIndicatorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/policy/cr_policy_indicator.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_policy_strings.js',
    'cr_policy_indicator_tests.js',
  ]),
};

TEST_F('CrElementsPolicyIndicatorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsPolicyIndicatorBehaviorTest() {}

CrElementsPolicyIndicatorBehaviorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_policy_strings.js',
    'cr_policy_indicator_behavior_tests.js',
  ]),
};

TEST_F('CrElementsPolicyIndicatorBehaviorTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsPolicyPrefIndicatorTest() {}

CrElementsPolicyPrefIndicatorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'settings_private_test_constants.js',
    'cr_policy_strings.js',
    'cr_policy_pref_indicator_tests.js',
  ]),
};

TEST_F('CrElementsPolicyPrefIndicatorTest', 'All', function() {
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
function CrElementsSliderTest() {}

CrElementsSliderTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_slider/cr_slider.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_slider_test.js',
  ]),
};

TEST_F('CrElementsSliderTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsToastTest() {}

CrElementsToastTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_toast/cr_toast.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '//chrome/test/data/webui/mock_timer.js',
    'cr_toast_test.js',
  ]),
};

TEST_F('CrElementsToastTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsToastManagerTest() {}

CrElementsToastManagerTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_toast/cr_toast_manager.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_toast_manager_test.js',
  ]),
};

TEST_F('CrElementsToastManagerTest', 'All', function() {
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
function CrElementsLinkRowTest() {}

CrElementsLinkRowTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_link_row/cr_link_row.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_link_row_tests.js',
  ]),
};

TEST_F('CrElementsLinkRowTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsExpandButtonTest() {}

CrElementsExpandButtonTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_expand_button_tests.js',
  ]),
};

TEST_F('CrElementsExpandButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsFindShortcutBehaviorTest() {}

CrElementsFindShortcutBehaviorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /**
   * Preload a module that depends on both cr-dialog and FindShortcutBehavior.
   * cr-dialog is used in the tests.
   * @override
   */
  browsePreload: 'chrome://resources/cr_elements/find_shortcut_behavior.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'find_shortcut_behavior_test.js',
  ]),
};

TEST_F('CrElementsFindShortcutBehaviorTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsSearchableDropDownTest() {}

CrElementsSearchableDropDownTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_searchable_drop_down/' +
      'cr_searchable_drop_down.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_searchable_drop_down_tests.js',
  ]),
};

TEST_F('CrElementsSearchableDropDownTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

////////////////////////////////////////////////////////////////////////////////
// View Manager Tests

// eslint-disable-next-line no-var
var CrElementsViewManagerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'cr_view_manager_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return cr_view_manager_test.suiteName;
  }
};

TEST_F('CrElementsViewManagerTest', 'VisibilityTest', function() {
  runMochaTest(this.suiteName, cr_view_manager_test.TestNames.Visibility);
});

TEST_F('CrElementsViewManagerTest', 'EventFiringTest', function() {
  runMochaTest(this.suiteName, cr_view_manager_test.TestNames.EventFiring);
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
