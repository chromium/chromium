// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends testing.Test
 */
function SettingsIdleLoadBrowserTest() {}

SettingsIdleLoadBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/test_loader.html?module=settings/idle_load_tests.js',

  /** @override */
  isAsync: true,
};

TEST_F('SettingsIdleLoadBrowserTest', 'All', function() {
  // Run all registered tests.
  mocha.run();
});
