// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer what's new tests on what's new UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer What's New elements. */
const WhatsNewBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overridden by subclasses');
  }

  /** @override */
  get webuiHost() {
    return 'whats-new';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kChromeWhatsNewUI']};
  }
};

var WhatsNewAppTest = class extends WhatsNewBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://whats-new/test_loader.html?module=whats_new/whats_new_app_test.js';
  }
};

TEST_F('WhatsNewAppTest', 'All', function() {
  mocha.run();
});
