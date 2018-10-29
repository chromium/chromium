// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements which rely on focus. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_interactive_ui_test.js']);

function CrElementsFocusTest() {}

CrElementsFocusTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),
};

function CrElementsActionMenuTest() {}

CrElementsActionMenuTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.html',

  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../settings/test_util.js',
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
    '../settings/test_util.js',
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
    '../settings/test_util.js',
    'cr_checkbox_test.js',
  ]),
};

TEST_F('CrElementsCheckboxTest', 'All', function() {
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
    '../settings/test_util.js',
    'cr_input_test.js',
  ]),
};

// This test is flaky on ChromeOS. See https://crbug.com/895832.
GEN('#if defined(OS_CHROMEOS)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsInputTest', 'MAYBE_All', function() {
  mocha.run();
});
