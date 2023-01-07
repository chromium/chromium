// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the Commander WebUI interface. */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "content/public/test/browser_test.h"');

var CommanderWebUIBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://commander/test_loader.html?module=commander/commander_app_test.js';
  }
};

TEST_F('CommanderWebUIBrowserTest', 'All', function() {
  mocha.run();
});
