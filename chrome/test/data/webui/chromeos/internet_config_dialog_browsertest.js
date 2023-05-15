// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Internet Config Dialog.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var InternetConfigDialogBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://internet-config-dialog/test_loader.html?module=chromeos/internet_config_dialog_test.js&host=test';
  }
};

TEST_F('InternetConfigDialogBrowserTest', 'All', function() {
  mocha.run();
});
