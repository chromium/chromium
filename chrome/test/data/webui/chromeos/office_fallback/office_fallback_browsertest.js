// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://office-fallback. Tests the entire page
 * instead of individual components.
 */

GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

var OfficeFallbackAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://office-fallback/test_loader.html?module=chromeos/' +
        'office_fallback/office_fallback_app_test.js';
  }

  get typedefCppFixture() {
    return 'NonManagedUserWebUIBrowserTest';
  }
};

TEST_F('OfficeFallbackAppBrowserTest', 'All', () => mocha.run());