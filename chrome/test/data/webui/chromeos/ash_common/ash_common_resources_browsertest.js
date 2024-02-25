// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the WebUI resources tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var AshCommonResourcesBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overridden by subclasses');
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

var AshCommonResourcesListPropertyUpdateBehaviorTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=chromeos/ash_common/list_property_update_behavior_test.js';
  }
};

TEST_F('AshCommonResourcesListPropertyUpdateBehaviorTest', 'All', function() {
  mocha.run();
});

var AshCommonResourcesI18nBehaviorTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=chromeos/ash_common/i18n_behavior_test.js';
  }
};

TEST_F('AshCommonResourcesI18nBehaviorTest', 'All', function() {
  mocha.run();
});

var AshCommonResourcesContainerShadowBehaviorTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=chromeos/ash_common/cr_container_shadow_behavior_test.js';
  }
};

TEST_F('AshCommonResourcesContainerShadowBehaviorTest', 'All', function() {
  mocha.run();
});


var AshCommonResourcesPolicyIndicatorBehaviorTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=chromeos/ash_common/cr_policy_indicator_behavior_test.js';
  }
};

TEST_F('AshCommonResourcesPolicyIndicatorBehaviorTest', 'All', function() {
  mocha.run();
});

var AshCommonResourcesScrollableBehaviorTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=chromeos/ash_common/cr_scrollable_behavior_test.js';
  }
};

TEST_F('AshCommonResourcesScrollableBehaviorTest', 'All', function() {
  mocha.run();
});

var AshCommonResourcesTypescriptUtilsStrictQueryTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=chromeos/ash_common/' +
        'typescript_utils/strict_query_test.js'
  }
};

TEST_F('AshCommonResourcesTypescriptUtilsStrictQueryTest', 'All', function() {
  mocha.run();
});

var AshCommonResourcesShortcutInputKeyTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=' +
        'chromeos/ash_common/shortcut_input_key_test.js';
  }
};

TEST_F('AshCommonResourcesShortcutInputKeyTest', 'All', function() {
  mocha.run();
});

var AshCommonResourcesShortcutInputTest =
    class extends AshCommonResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=' +
        'chromeos/ash_common/shortcut_input_test.js';
  }
};

TEST_F('AshCommonResourcesShortcutInputTest', 'All', function() {
  mocha.run();
});
