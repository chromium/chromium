// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://print-management.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function PrintManagementBrowserTest() {}

PrintManagementBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://print-management/test_loader.html?module=chromeos/' +
      'print_management/print_management_test.js',
};

TEST_F('PrintManagementBrowserTest', 'All', function() {
  mocha.run();
});
