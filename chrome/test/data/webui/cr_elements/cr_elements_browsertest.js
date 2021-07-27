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
var CrElementsBrowserTest = class extends PolymerTest {
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
var CrElementsA11yAnnouncerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_a11y_announcer_test.js';
  }
};

TEST_F('CrElementsA11yAnnouncerTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_button_tests.js';
  }
};

TEST_F('CrElementsButtonTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsContainerShadowBehaviorTest =
    class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_container_shadow_behavior_test.js';
  }
};

TEST_F('CrElementsContainerShadowBehaviorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsContainerShadowMixinTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_container_shadow_mixin_test.js';
  }
};

TEST_F('CrElementsContainerShadowMixinTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsDialogTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_dialog_test.js';
  }
};

TEST_F('CrElementsDialogTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsDrawerTest = class extends CrElementsBrowserTest {
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

TEST_F('CrElementsDrawerTest', 'MAYBE_Drawer', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsExpandButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_expand_button_tests.js';
  }
};

TEST_F('CrElementsExpandButtonTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsFindShortcutBehaviorTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/find_shortcut_behavior_test.js';
  }
};

TEST_F('CrElementsFindShortcutBehaviorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsFingerprintProgressArcTest = class extends CrElementsBrowserTest {
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

TEST_F('CrElementsFingerprintProgressArcTest', 'MAYBE_Fingerprint', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsIconButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_icon_button_tests.js';
  }
};

TEST_F('CrElementsIconButtonTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLazyRenderTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_lazy_render_tests.js';
  }
};

TEST_F('CrElementsLazyRenderTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLinkRowTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_link_row_tests.js';
  }
};

TEST_F('CrElementsLinkRowTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsRadioButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_radio_button_test.js';
  }
};

TEST_F('CrElementsRadioButtonTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsCardRadioButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_card_radio_button_test.js';
  }
};

TEST_F('CrElementsCardRadioButtonTest', 'All', function() {
  mocha.run();
});


// eslint-disable-next-line no-var
var CrElementsRadioGroupTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_radio_group_test.js';
  }
};

TEST_F('CrElementsRadioGroupTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsScrollableBehaviorTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_scrollable_behavior_tests.js';
  }
};

TEST_F('CrElementsScrollableBehaviorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsSearchFieldTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_search_field_tests.js';
  }
};

TEST_F('CrElementsSearchFieldTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
// eslint-disable-next-line no-var
var CrElementsSearchableDropDownTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_searchable_drop_down_tests.js';
  }
};

TEST_F('CrElementsSearchableDropDownTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrElementsSliderTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_slider_test.js';
  }
};

TEST_F('CrElementsSliderTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsSplitterTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_splitter_test.js';
  }
};

TEST_F('CrElementsSplitterTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToastTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toast_test.js';
  }
};

TEST_F('CrElementsToastTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToastManagerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toast_manager_test.js';
  }
};

TEST_F('CrElementsToastManagerTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsViewManagerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_view_manager_test.js';
  }
};

TEST_F('CrElementsViewManagerTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsPolicyIndicatorTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_policy_indicator_tests.js';
  }
};

TEST_F('CrElementsPolicyIndicatorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsPolicyPrefIndicatorTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    // Preload a settings URL, so that the test can access settingsPrivate.
    return 'chrome://settings/test_loader.html?module=cr_elements/cr_policy_pref_indicator_tests.js';
  }
};

TEST_F('CrElementsPolicyPrefIndicatorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsPolicyIndicatorBehaviorTest =
    class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_policy_indicator_behavior_tests.js';
  }
};

TEST_F('CrElementsPolicyIndicatorBehaviorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsLottieTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_lottie_tests.js';
  }

  /** @override */
  get commandLineSwitches() {
    return [{switchName: 'enable-pixel-output-in-tests'}];
  }
};

TEST_F('CrElementsLottieTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsAutoImgTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_auto_img_test.js';
  }
};

TEST_F('CrElementsAutoImgTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToolbarTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_elements/cr_toolbar_test.js';
  }
};

TEST_F('CrElementsToolbarTest', 'All', function() {
  mocha.run();
});
