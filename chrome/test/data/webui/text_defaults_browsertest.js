// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for testing async methods of cr.js.
 */
var TextDefaultsTest = class extends testing.Test {
  /**
   * @override
   */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=text_defaults_test.js';
  }

  /** @override */
  get isAsync() {
    return true;
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


TEST_F('TextDefaultsTest', 'All', function() {
  mocha.run();
});
