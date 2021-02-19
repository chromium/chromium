// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements which rely on focus. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

function CrElementsFocusTest() {}

CrElementsFocusTest.prototype = {
  __proto__: Polymer2DeprecatedInteractiveUITest.prototype,
};

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

TEST_F('CrElementsInputTest', 'All', function() {
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

// eslint-disable-next-line no-var
var IronListFocusTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://resources/polymer/v1_0/iron-list/iron-list.html';
  }

  /** @override */
  get extraLibraries() {
    return [
      ...Polymer2DeprecatedTest.prototype.extraLibraries,
      '../test_util.js',
      'iron_list_focus_test.js',
    ];
  }
};

TEST_F('IronListFocusTest', 'All', function() {
  mocha.run();
});
