// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements which rely on focus. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

function CrElementsFocusTest() {}

CrElementsFocusTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,
};

function CrElementsActionMenuTest() {}

CrElementsActionMenuTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.html',

  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_action_menu_test.js',
  ]),
};

TEST_F('CrElementsActionMenuTest', 'All', function() {
  mocha.run();
});

function CrElementsProfileAvatarSelectorFocusTest() {}

CrElementsProfileAvatarSelectorFocusTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_profile_avatar_selector/' +
      'cr_profile_avatar_selector.html',

  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    'cr_profile_avatar_selector_tests.js',
  ]),
};

TEST_F('CrElementsProfileAvatarSelectorFocusTest', 'All', function() {
  cr_profile_avatar_selector.registerTests();
  mocha.grep(cr_profile_avatar_selector.TestNames.Focus).run();
});

/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsToggleTest() {}

CrElementsToggleTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_toggle/cr_toggle.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_toggle_test.js',
  ]),
};

TEST_F('CrElementsToggleTest', 'All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsCheckboxTest() {}

CrElementsCheckboxTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_checkbox_test.js',
  ]),
};

// crbug.com/997943.
TEST_F('CrElementsCheckboxTest', 'DISABLED_All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsInputTest() {}

CrElementsInputTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_input/cr_input.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_input_test.js',
  ]),
};

// https://crbug.com/997943: Flaky on Mac
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsInputTest', 'MAYBE_All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsIconButtonFocusTest() {}

CrElementsIconButtonFocusTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'cr_icon_button_focus_tests.js',
  ]),
};

TEST_F('CrElementsIconButtonFocusTest', 'All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsExpandButtonTest() {}

CrElementsExpandButtonTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/util.js',
    '../test_util.js',
    'cr_expand_button_focus_tests.js',
  ]),
};

TEST_F('CrElementsExpandButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsBrowserTest}
 */
function CrElementsTabsTest() {}

CrElementsTabsTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_tabs/cr_tabs.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/util.js',
    '../test_util.js',
    'cr_tabs_test.js',
  ]),
};

TEST_F('CrElementsTabsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var IronListFocusTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://resources/polymer/v1_0/iron-list/iron-list.html';
  }

  /** @override */
  get extraLibraries() {
    return [
      ...PolymerTest.prototype.extraLibraries,
      '../test_util.js',
      'iron_list_focus_test.js',
    ];
  }
};

TEST_F('IronListFocusTest', 'All', function() {
  mocha.run();
});
