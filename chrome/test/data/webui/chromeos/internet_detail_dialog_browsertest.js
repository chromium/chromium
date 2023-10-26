// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Internet Detail Dialog.
 */
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var InternetDetailDialogBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://internet-detail-dialog/test_loader.html?module=chromeos/internet_detail_dialog_test.js';
  }
};

TEST_F('InternetDetailDialogBrowserTest', 'All', function() {
  mocha.run();
});
