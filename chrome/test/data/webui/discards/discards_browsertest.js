// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

/**
 * TestFixture for Discards WebUI testing.
 */
var DiscardsTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://discards/test_loader.html?module=discards/discards_test.js';
  }
};


TEST_F('DiscardsTest', 'All', function() {
  mocha.run();
});
