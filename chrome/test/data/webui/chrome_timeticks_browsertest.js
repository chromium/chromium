// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests to ensure that chrome.timeticks.nowInMicroseconds()
 * returns a BitInt.
 */

GEN('#include "content/public/test/browser_test.h"');

/* eslint-disable no-var */

/** Test fixture for chrome.timeTicks WebUI testing. */
var ChromeTimeTicksBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }

  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=chrome_timeticks_test.js';
  }
};

TEST_F('ChromeTimeTicksBrowserTest', 'All', function() {
  mocha.run();
});
