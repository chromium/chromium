// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests to ensure that chrome.timeticks.nowInMicroseconds()
 * returns a BitInt.
 */

GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for chrome.timeTicks WebUI testing. */
var ChromeTimeTicksBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=chrome_timeticks_test.js';
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

TEST_F('ChromeTimeTicksBrowserTest', 'All', function() {
  mocha.run();
});
