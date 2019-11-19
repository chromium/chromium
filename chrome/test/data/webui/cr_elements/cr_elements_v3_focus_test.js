// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements which rely on focus. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

// eslint-disable-next-line no-var
var CrElementsV3FocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

// eslint-disable-next-line no-var
var CrElementsActionMenuV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_action_menu_test.m.js';
  }
};

TEST_F('CrElementsActionMenuV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsCheckboxV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_checkbox_test.m.js';
  }
};

TEST_F('CrElementsCheckboxV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsExpandButtonV3FocusTest = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_expand_button_focus_tests.m.js';
  }
};

TEST_F('CrElementsExpandButtonV3FocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsIconButtonV3FocusTest = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_icon_button_focus_tests.m.js';
  }
};

TEST_F('CrElementsIconButtonV3FocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsInputV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_input_test.m.js';
  }
};

// https://crbug.com/997943: Flaky on Mac
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsInputV3Test', 'MAYBE_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsTabsV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_tabs_test.m.js';
  }
};

TEST_F('CrElementsTabsV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToggleV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_toggle_test.m.js';
  }
};

TEST_F('CrElementsToggleV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var IronListFocusV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/iron_list_focus_test.m.js';
  }
};

TEST_F('IronListFocusV3Test', 'All', function() {
  mocha.run();
});
