// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI customize chrome. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "components/search/ntp_features.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');

class SidePanelCustomizeChromeBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ntp_features::kCustomizeChromeSidePanel',
      ],
    };
  };
}

var ShortcutsTest = class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/shortcuts_test.js';
  }
};

TEST_F('ShortcutsTest', 'All', function() {
  mocha.run();
});
