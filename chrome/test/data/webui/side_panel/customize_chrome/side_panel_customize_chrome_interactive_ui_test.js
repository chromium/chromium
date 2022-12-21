// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI customize chrome. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "components/search/ntp_features.h"');

/* eslint-disable no-var */

class SidePanelCustomizeChromeInteractiveUiTest extends
    PolymerInteractiveUITest {
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
  }
}

var SidePanelCustomizeChromeColorsFocusTest =
    class extends SidePanelCustomizeChromeInteractiveUiTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/colors_focus_test.js';
  }
};

TEST_F('SidePanelCustomizeChromeColorsFocusTest', 'All', function() {
  mocha.run();
});
