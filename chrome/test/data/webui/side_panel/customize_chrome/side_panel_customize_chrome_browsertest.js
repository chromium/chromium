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

var SidePanelCustomizeChromeButtonLabelTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/button_label_test.js';
  }
};

var SidePanelCustomizeChromeCardsTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/cards_test.js';
  }
};

var SidePanelCustomizeChromeShortcutsTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/shortcuts_test.js';
  }
};

var SidePanelCustomizeChromeAppTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/app_test.js';
  }
};

var SidePanelCustomizeChromeAppearanceTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/appearance_test.js';
  }
};

var SidePanelCustomizeChromeCategoriesTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/categories_test.js';
  }
};

var SidePanelCustomizeChromeCheckMarkWrapperTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/check_mark_wrapper_test.js';
  }
};

var SidePanelCustomizeChromeColorTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/color_test.js';
  }
};

var SidePanelCustomizeChromeColorsTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/colors_test.js';
  }
};

var SidePanelCustomizeChromeHoverButtonTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/hover_button_test.js';
  }
};

var SidePanelCustomizeChromeThemesTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/themes_test.js';
  }
};

var SidePanelCustomizeChromeThemeSnapshotTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/theme_snapshot_test.js';
  }
};

var SidePanelCustomizeChromeChromeColorsTest =
    class extends SidePanelCustomizeChromeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://customize-chrome-side-panel.top-chrome/test_loader.html' +
        '?module=side_panel_customize_chrome/chrome_colors_test.js';
  }
};

TEST_F('SidePanelCustomizeChromeButtonLabelTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeCardsTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeShortcutsTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeAppTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeAppearanceTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeCategoriesTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeCheckMarkWrapperTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeColorTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeColorsTest', 'All', function() {
  mocha.run();
});


TEST_F('SidePanelCustomizeChromeHoverButtonTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeThemesTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeThemeSnapshotTest', 'All', function() {
  mocha.run();
});

TEST_F('SidePanelCustomizeChromeChromeColorsTest', 'All', function() {
  mocha.run();
});
