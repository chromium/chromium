// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JS tests for various chrome://resources JS modules.
 */

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
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

var CrModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=js/cr_test.js';
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
    return 'chrome://test?module=js/promise_resolver_test.js';
  }
};

TEST_F('PromiseResolverModuleTest', 'All', function() {
  mocha.run();
});

var ParseHtmlSubsetModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=js/parse_html_subset_test.js';
  }
};

TEST_F('ParseHtmlSubsetModuleTest', 'All', function() {
  mocha.run();
});

var UtilModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=js/util_test.js';
  }
};

TEST_F('UtilModuleTest', 'All', function() {
  mocha.run();
});

var LoadTimeDataModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=js/load_time_data_test.js';
  }
};

TEST_F('LoadTimeDataModuleTest', 'All', function() {
  mocha.run();
});

var I18nBehaviorModuleTest = class extends WebUIResourceModuleAsyncTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=js/i18n_behavior_test.js';
  }
};

TEST_F('I18nBehaviorModuleTest', 'All', function() {
  mocha.run();
});
