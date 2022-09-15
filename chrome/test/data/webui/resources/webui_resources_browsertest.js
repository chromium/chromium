// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the WebUI resources tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');

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

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
var WebUIResourcesListPropertyUpdateBehaviorTest =
    class extends WebUIResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=resources/list_property_update_behavior_tests.js';
  }
};

TEST_F('WebUIResourcesListPropertyUpdateBehaviorTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

var WebUIResourcesListPropertyUpdateMixinTest =
    class extends WebUIResourcesBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=resources/list_property_update_mixin_tests.js';
  }
};

TEST_F('WebUIResourcesListPropertyUpdateMixinTest', 'All', function() {
  mocha.run();
});
