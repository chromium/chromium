// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "build/chromeos_buildflags.h"');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrElementsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

// eslint-disable-next-line no-var
var CrElementsButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_button_tests.js';
  }
};

TEST_F('CrElementsButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsContainerShadowBehaviorV3Test =
    class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_container_shadow_behavior_test.js';
  }
};

TEST_F('CrElementsContainerShadowBehaviorV3Test', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var CrElementsDialogV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_dialog_test.js';
  }
};

TEST_F('CrElementsDialogV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsDrawerV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_drawer_tests.js';
  }
};

// https://crbug.com/1008122 - Flaky on Mac 10.10.
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_Drawer DISABLED_Drawer');
GEN('#else');
GEN('#define MAYBE_Drawer Drawer');
GEN('#endif');

TEST_F('CrElementsDrawerV3Test', 'MAYBE_Drawer', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsExpandButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_expand_button_tests.js';
  }
};

TEST_F('CrElementsExpandButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsFindShortcutBehaviorV3Test =
    class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/find_shortcut_behavior_test.js';
  }
};

TEST_F('CrElementsFindShortcutBehaviorV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsFingerprintProgressArcV3Test =
    class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_fingerprint_progress_arc_tests.js';
  }

  /** @override */
  get commandLineSwitches() {
    return [{switchName: 'enable-pixel-output-in-tests'}];
  }
};

// https://crbug.com/1044390 - maybe flaky on Mac?
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_Fingerprint DISABLED_Fingerprint');
GEN('#else');
GEN('#define MAYBE_Fingerprint Fingerprint');
GEN('#endif');

TEST_F(
    'CrElementsFingerprintProgressArcV3Test', 'MAYBE_Fingerprint', function() {
      mocha.run();
    });

// eslint-disable-next-line no-var
var CrElementsIconButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_icon_button_tests.js';
  }
};

TEST_F('CrElementsIconButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLazyRenderV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_lazy_render_tests.js';
  }
};

TEST_F('CrElementsLazyRenderV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLinkRowV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_link_row_tests.js';
  }
};

TEST_F('CrElementsLinkRowV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsRadioButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_radio_button_test.js';
  }
};

TEST_F('CrElementsRadioButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsCardRadioButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_card_radio_button_test.js';
  }
};

TEST_F('CrElementsCardRadioButtonV3Test', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var CrElementsRadioGroupV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_radio_group_test.js';
  }
};

TEST_F('CrElementsRadioGroupV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsScrollableBehaviorV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_scrollable_behavior_tests.js';
  }
};

TEST_F('CrElementsScrollableBehaviorV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsSearchFieldV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_search_field_tests.js';
  }
};

TEST_F('CrElementsSearchFieldV3Test', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
// eslint-disable-next-line no-var
var CrElementsSearchableDropDownV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_searchable_drop_down_tests.js';
  }
};

TEST_F('CrElementsSearchableDropDownV3Test', 'All', function() {
  mocha.run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrElementsSliderV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_slider_test.js';
  }
};

TEST_F('CrElementsSliderV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsSplitterV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_splitter_test.js';
  }
};

TEST_F('CrElementsSplitterV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToastV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toast_test.js';
  }
};

TEST_F('CrElementsToastV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToastManagerV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toast_manager_test.js';
  }
};

TEST_F('CrElementsToastManagerV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsViewManagerV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_view_manager_test.js';
  }
};

TEST_F('CrElementsViewManagerV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsPolicyIndicatorV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_policy_indicator_tests.js';
  }
};

TEST_F('CrElementsPolicyIndicatorV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsPolicyPrefIndicatorV3Test =
    class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    // Preload a settings URL, so that the test can access settingsPrivate.
    return 'chrome://settings/test_loader.html?module=cr_elements/cr_policy_pref_indicator_tests.js';
  }
};

TEST_F('CrElementsPolicyPrefIndicatorV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsPolicyIndicatorBehaviorV3Test =
    class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_policy_indicator_behavior_tests.js';
  }
};

TEST_F('CrElementsPolicyIndicatorBehaviorV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLottieV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_lottie_tests.js';
  }

  /** @override */
  get commandLineSwitches() {
    return [{switchName: 'enable-pixel-output-in-tests'}];
  }
};

TEST_F('CrElementsLottieV3Test', 'All', function() {
  mocha.run();
});
