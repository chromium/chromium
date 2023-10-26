// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Crostini Installer page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var CrostiniInstallerBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://crostini-installer/test_loader.html?module=chromeos/crostini_installer_app_test.js';
  }
};

TEST_F('CrostiniInstallerBrowserTest', 'All', function() {
  mocha.run();
});
