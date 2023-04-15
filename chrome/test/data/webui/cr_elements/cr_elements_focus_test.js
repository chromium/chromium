// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements which rely on focus. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "build/build_config.h"');
GEN('#include "content/public/test/browser_test.h"');

var CrElementsFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

var CrElementsActionMenuTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_action_menu_test.js';
  }
};

TEST_F('CrElementsActionMenuTest', 'All', function() {
  mocha.run();
});

var CrElementsCheckboxTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_checkbox_test.js';
  }
};

TEST_F('CrElementsCheckboxTest', 'All', function() {
  mocha.run();
});

var CrElementsInputTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_input_test.js';
  }
};

// https://crbug.com/997943: Flaky on Mac
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsInputTest', 'MAYBE_All', function() {
  mocha.run();
});

var CrElementsProfileAvatarSelectorTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_profile_avatar_selector_test.js';
  }
};

TEST_F('CrElementsProfileAvatarSelectorTest', 'All', function() {
  mocha.run();
});

var CrElementsTabsTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_tabs_test.js';
  }
};

TEST_F('CrElementsTabsTest', 'All', function() {
  mocha.run();
});

var CrElementsToggleTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_toggle_test.js';
  }
};

TEST_F('CrElementsToggleTest', 'All', function() {
  mocha.run();
});

var CrElementsToolbarSearchFieldTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_toolbar_search_field_test.js';
  }
};

TEST_F('CrElementsToolbarSearchFieldTest', 'All', function() {
  mocha.run();
});


var IronListFocusTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/iron_list_focus_test.js';
  }
};

TEST_F('IronListFocusTest', 'All', function() {
  mocha.run();
});


var CrElementsGridFocusTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_grid_focus_test.js';
  }
};

TEST_F('CrElementsGridFocusTest', 'All', function() {
  mocha.run();
});


var CrElementsMenuSelectorFocusTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_menu_selector_focus_test.js';
  }
};

TEST_F('CrElementsMenuSelectorFocusTest', 'All', function() {
  mocha.run();
});


var CrElementsToolbarFocusTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_toolbar_focus_test.js';
  }
};

TEST_F('CrElementsToolbarFocusTest', 'All', function() {
  mocha.run();
});

var CrElementsTextareaFocusTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_textarea_focus_test.js';
  }
};

TEST_F('CrElementsTextareaFocusTest', 'All', function() {
  mocha.run();
});

var CrFocusRowMixinTest = class extends CrElementsFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_focus_row_mixin_test.js';
  }
};

TEST_F('CrFocusRowMixinTest', 'FocusTest', function() {
  mocha.run();
});
