// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the Commander WebUI interface. */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var CommanderWebUIBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://commander/test_loader.html?module=commander/commander_app_test.js';
  }
  /** @override */
  get extraLibraries() {
    return [
      // Even though PolymerTest includes this, we need to override it to
      // avoid double-importing cr.m.js
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

TEST_F('CommanderWebUIBrowserTest', 'All', function() {
  mocha.run();
});
