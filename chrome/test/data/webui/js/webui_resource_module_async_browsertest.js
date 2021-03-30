// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JS tests for various chrome://resources JS modules.
 */

GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for testing shared JS module resources. */
var WebUIResourceModuleAsyncTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return DUMMY_URL;
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

var CrModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/cr_test.js';
  }
};

TEST_F('CrModuleTest', 'AddSingletonGetter', function() {
  mocha.fgrep('CrModuleAddSingletonGetterTest').run();
});

TEST_F('CrModuleTest', 'SendWithPromise', function() {
  mocha.fgrep('CrModuleSendWithPromiseTest').run();
});

TEST_F('CrModuleTest', 'WebUIListeners', function() {
  mocha.fgrep('CrModuleWebUIListenersTest').run();
});

var PromiseResolverModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/promise_resolver_test.js';
  }
};

TEST_F('PromiseResolverModuleTest', 'All', function() {
  mocha.run();
});

var ParseHtmlSubsetModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/parse_html_subset_test.js';
  }
};

TEST_F('ParseHtmlSubsetModuleTest', 'All', function() {
  mocha.run();
});

var ParseHtmlSubsetTrustedTypesTest =
    class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/parse_html_subset_trusted_types_test.js';
  }
};

TEST_F('ParseHtmlSubsetTrustedTypesTest', 'All', function() {
  mocha.run();
});

var UtilModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/util_test.js';
  }
};

TEST_F('UtilModuleTest', 'All', function() {
  mocha.run();
});

var LoadTimeDataModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/load_time_data_test.m.js';
  }
};

TEST_F('LoadTimeDataModuleTest', 'All', function() {
  mocha.run();
});

var I18nBehaviorModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/i18n_behavior_test.js';
  }
};

TEST_F('I18nBehaviorModuleTest', 'All', function() {
  mocha.run();
});

var ColorUtilsModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/color_utils_test.js';
  }
};

TEST_F('ColorUtilsModuleTest', 'All', function() {
  mocha.run();
});

var CustomElementModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=js/custom_element_test.js';
  }
};

TEST_F('CustomElementModuleTest', 'All', function() {
  mocha.run();
});
