// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests to ensure that chrome.timeticks.nowInMicroseconds()
 * is close to C++ base::TimeTicks::Now().
 */

GEN('#include "chrome/test/data/webui/chrome_timeticks_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

/* eslint-disable no-var */

/** Test fixture for chrome.timeTicks WebUI testing. */
var ChromeTimeTicksBrowserTest = class extends testing.Test {
  /** @override */
  get typedefCppFixture() {
    return 'ChromeTimeTicksBrowserTest';
  }

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
    return 'chrome://test/test_loader.html?module=chrome_timeticks_test.js&host=webui-test';
  }
};

TEST_F('ChromeTimeTicksBrowserTest', 'All', function() {
  mocha.run();
});