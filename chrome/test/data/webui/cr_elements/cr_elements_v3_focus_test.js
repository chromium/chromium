// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements which rely on focus. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var CrElementsV3FocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
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
    return 'chrome://test?module=cr_elements/cr_action_menu_test.js';
  }

  /** @override */
  get extraLibraries() {
    return [
      // TODO(dpapad): Figure out why this test fails if test_loader.html is
      // used instead.
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

TEST_F('CrElementsActionMenuV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsCheckboxV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_checkbox_test.js';
  }
};

TEST_F('CrElementsCheckboxV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsInputV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_input_test.js';
  }
};

// https://crbug.com/997943: Flaky on Mac
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsInputV3Test', 'MAYBE_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsProfileAvatarSelectorV3Test =
    class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_profile_avatar_selector_tests.js';
  }
};

TEST_F('CrElementsProfileAvatarSelectorV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsTabsV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_tabs_test.js';
  }
};

TEST_F('CrElementsTabsV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToggleV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toggle_test.js';
  }
};

TEST_F('CrElementsToggleV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToolbarSearchFieldV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toolbar_search_field_tests.js';
  }
};

TEST_F('CrElementsToolbarSearchFieldV3Test', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var IronListFocusV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/iron_list_focus_test.js';
  }
};

TEST_F('IronListFocusV3Test', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var CrElementsGridFocusTest = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_grid_focus_test.js';
  }
};

TEST_F('CrElementsGridFocusTest', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var CrElementsMenuSelectorFocusTest = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_menu_selector_focus_test.js';
  }
};

TEST_F('CrElementsMenuSelectorFocusTest', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var CrElementsToolbarFocusV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toolbar_focus_tests.js';
  }
};

TEST_F('CrElementsToolbarFocusV3Test', 'All', function() {
  mocha.run();
});
