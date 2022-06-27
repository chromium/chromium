// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the WebUI resources tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

/* eslint-disable no-var */

var WebUIResourcesBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overridden by subclasses');
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

var WebUIResourcesListPropertyUpdateBehaviorTest =
    class extends WebUIResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=resources/list_property_update_behavior_tests.js';
  }
};

TEST_F('WebUIResourcesListPropertyUpdateBehaviorTest', 'All', function() {
  mocha.run();
});

var WebUIResourcesListPropertyUpdateMixinTest =
    class extends WebUIResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=resources/list_property_update_mixin_tests.js';
  }
};

TEST_F('WebUIResourcesListPropertyUpdateMixinTest', 'All', function() {
  mocha.run();
});
