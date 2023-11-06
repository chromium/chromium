// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Crostini Upgrader page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var CrostiniUpgraderBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://crostini-upgrader/test_loader.html?module=chromeos/crostini_upgrader_app_test.js';
  }
};

TEST_F('CrostiniUpgraderBrowserTest', 'All', function() {
  mocha.run();
});
