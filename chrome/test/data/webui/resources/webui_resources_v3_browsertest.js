// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the WebUI resources tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

// eslint-disable-next-line no-var
var WebUIResourcesV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
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

// eslint-disable-next-line no-var
var WebUIResourcesListPropertyUpdateBehaviorV3Test =
    class extends WebUIResourcesV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=resources/list_property_update_behavior_tests.m.js';
  }
};

TEST_F('WebUIResourcesListPropertyUpdateBehaviorV3Test', 'All', function() {
  mocha.run();
});
