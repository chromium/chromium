// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrElementsV3BrowserTest = class extends PolymerTest {
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

  /** @override */
  get runAccessibilityChecks() {
    return true;
  }

  /** @override */
  setUp() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  }
};

// eslint-disable-next-line no-var
var CrElementsButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_button_tests.m.js';
  }
};

TEST_F('CrElementsButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsContainerShadowBehaviorV3Test =
    class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_container_shadow_behavior_test.m.js';
  }
};

TEST_F('CrElementsContainerShadowBehaviorV3Test', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var CrElementsDialogV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_dialog_test.m.js';
  }
};

TEST_F('CrElementsDialogV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsDrawerV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_drawer_tests.m.js';
  }
};

// https://crbug.com/1008122
GEN('#if defined(OS_MACOSX) && defined(NDEBUG)');
GEN('# define MAYBE_CrElementsDrawerV3Test_All \\');
GEN('     DISABLED_All');
GEN('#else');
GEN('# define MAYBE_CrElementsDrawerV3Test_All  \\');
GEN('     All');
GEN('#endif');
TEST_F(
    'CrElementsDrawerV3Test', 'MAYBE_CrElementsDrawerV3Test_All', function() {
      mocha.run();
    });

// eslint-disable-next-line no-var
var CrElementsExpandButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_expand_button_tests.m.js';
  }
};

TEST_F('CrElementsExpandButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsIconButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_icon_button_tests.m.js';
  }
};

TEST_F('CrElementsIconButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLazyRenderV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_lazy_render_tests.m.js';
  }
};

TEST_F('CrElementsLazyRenderV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLinkRowV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_link_row_tests.m.js';
  }
};

TEST_F('CrElementsLinkRowV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsRadioButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_radio_button_test.m.js';
  }
};

TEST_F('CrElementsRadioButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsRadioGroupV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_radio_group_test.m.js';
  }
};

TEST_F('CrElementsRadioGroupV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsSearchFieldV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_search_field_tests.m.js';
  }
};

TEST_F('CrElementsSearchFieldV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsSplitterV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_splitter_test.m.js';
  }
};

TEST_F('CrElementsSplitterV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToastV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_toast_test.m.js';
  }
};

TEST_F('CrElementsToastV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToolbarSearchFieldV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_toolbar_search_field_tests.m.js';
  }
};

TEST_F('CrElementsToolbarSearchFieldV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToastManagerV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_toast_manager_test.m.js';
  }
};

TEST_F('CrElementsToastManagerV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsViewManagerV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_view_manager_test.m.js';
  }
};

TEST_F('CrElementsViewManagerV3Test', 'All', function() {
  mocha.run();
});
