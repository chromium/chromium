// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "build/chromeos_buildflags.h"');

/** Test fixture for shared Polymer 3 elements. */
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

var CrElementsA11yAnnouncerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_a11y_announcer_test.js';
  }
};

TEST_F('CrElementsA11yAnnouncerTest', 'All', function() {
  mocha.run();
});

var CrElementsButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_button_test.js';
  }
};

TEST_F('CrElementsButtonTest', 'All', function() {
  mocha.run();
});

var CrElementsContainerShadowMixinTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_container_shadow_mixin_test.js';
  }
};

TEST_F('CrElementsContainerShadowMixinTest', 'All', function() {
  mocha.run();
});

var CrElementsDialogTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_dialog_test.js';
  }
};

TEST_F('CrElementsDialogTest', 'All', function() {
  mocha.run();
});

var CrElementsDrawerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_drawer_test.js';
  }
};

// https://crbug.com/1008122 - Flaky on Mac.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_Drawer DISABLED_Drawer');
GEN('#else');
GEN('#define MAYBE_Drawer Drawer');
GEN('#endif');

TEST_F('CrElementsDrawerTest', 'MAYBE_Drawer', function() {
  mocha.run();
});

var CrElementsExpandButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_expand_button_test.js';
  }
};

TEST_F('CrElementsExpandButtonTest', 'All', function() {
  mocha.run();
});

var CrElementsFindShortcutMixinTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/find_shortcut_mixin_test.js';
  }
};

TEST_F('CrElementsFindShortcutMixinTest', 'All', function() {
  mocha.run();
});

var CrElementsFingerprintProgressArcTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_fingerprint_progress_arc_test.js';
  }

  /** @override */
  get commandLineSwitches() {
    return [{switchName: 'enable-pixel-output-in-tests'}];
  }
};

// https://crbug.com/1044390 - maybe flaky on Mac?
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_Fingerprint DISABLED_Fingerprint');
GEN('#else');
GEN('#define MAYBE_Fingerprint Fingerprint');
GEN('#endif');

TEST_F('CrElementsFingerprintProgressArcTest', 'MAYBE_Fingerprint', function() {
  mocha.run();
});

var CrElementsI18nMixinTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/i18n_mixin_test.js';
  }
};

TEST_F('CrElementsI18nMixinTest', 'All', function() {
  mocha.run();
});

var CrElementsIconButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_icon_button_test.js';
  }
};

TEST_F('CrElementsIconButtonTest', 'All', function() {
  mocha.run();
});

var CrElementsLazyRenderTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_lazy_render_test.js';
  }
};

TEST_F('CrElementsLazyRenderTest', 'All', function() {
  mocha.run();
});

var CrElementsLinkRowTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_link_row_test.js';
  }
};

TEST_F('CrElementsLinkRowTest', 'All', function() {
  mocha.run();
});

var CrElementsListPropertyUpdateMixinTest =
    class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/list_property_update_mixin_test.js';
  }
};

TEST_F('CrElementsListPropertyUpdateMixinTest', 'All', function() {
  mocha.run();
});

var CrElementsRadioButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_radio_button_test.js';
  }
};

TEST_F('CrElementsRadioButtonTest', 'All', function() {
  mocha.run();
});

var CrElementsCardRadioButtonTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_card_radio_button_test.js';
  }
};

TEST_F('CrElementsCardRadioButtonTest', 'All', function() {
  mocha.run();
});


var CrElementsRadioGroupTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_radio_group_test.js';
  }
};

TEST_F('CrElementsRadioGroupTest', 'All', function() {
  mocha.run();
});

var CrElementsScrollableMixinTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_scrollable_mixin_test.js';
  }
};

TEST_F('CrElementsScrollableMixinTest', 'All', function() {
  mocha.run();
});

var CrElementsSearchFieldTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_search_field_test.js';
  }
};

TEST_F('CrElementsSearchFieldTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
var CrElementsSearchableDropDownTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_searchable_drop_down_test.js';
  }
};

TEST_F('CrElementsSearchableDropDownTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

var CrElementsSliderTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_slider_test.js';
  }
};

TEST_F('CrElementsSliderTest', 'All', function() {
  mocha.run();
});

var CrElementsSplitterTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_splitter_test.js';
  }
};

TEST_F('CrElementsSplitterTest', 'All', function() {
  mocha.run();
});

var CrElementsToastTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_toast_test.js';
  }
};

TEST_F('CrElementsToastTest', 'All', function() {
  mocha.run();
});

var CrElementsToastManagerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_toast_manager_test.js';
  }
};

TEST_F('CrElementsToastManagerTest', 'All', function() {
  mocha.run();
});

var CrElementsViewManagerTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_view_manager_test.js';
  }
};

TEST_F('CrElementsViewManagerTest', 'All', function() {
  mocha.run();
});

var CrElementsPolicyIndicatorTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_policy_indicator_test.js';
  }
};

TEST_F('CrElementsPolicyIndicatorTest', 'All', function() {
  mocha.run();
});

var CrElementsPolicyPrefIndicatorTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    // Preload a settings URL, so that the test can access settingsPrivate.
    return 'chrome://settings/test_loader.html?module=cr_elements/cr_policy_pref_indicator_test.js';
  }
};

TEST_F('CrElementsPolicyPrefIndicatorTest', 'All', function() {
  mocha.run();
});

var CrElementsPolicyIndicatorMixinTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_policy_indicator_mixin_test.js';
  }
};

TEST_F('CrElementsPolicyIndicatorMixinTest', 'All', function() {
  mocha.run();
});

var CrElementsLottieTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_lottie_test.js';
  }

  /** @override */
  get commandLineSwitches() {
    return [{switchName: 'enable-pixel-output-in-tests'}];
  }
};

TEST_F('CrElementsLottieTest', 'All', function() {
  mocha.run();
});

var CrElementsAutoImgTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_auto_img_test.js';
  }
};

TEST_F('CrElementsAutoImgTest', 'All', function() {
  mocha.run();
});

var CrElementsToolbarTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_toolbar_test.js';
  }
};

TEST_F('CrElementsToolbarTest', 'All', function() {
  mocha.run();
});

var CrElementsTreeTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_tree_test.js';
  }
};

TEST_F('CrElementsTreeTest', 'All', function() {
  mocha.run();
});

var CrElementsWebUiListenerMixinTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/web_ui_listener_mixin_test.js';
  }
};

TEST_F('CrElementsWebUiListenerMixinTest', 'All', function() {
  mocha.run();
});


var CrElementsUrlListItemTest = class extends CrElementsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_elements/cr_url_list_item_test.js';
  }
};

TEST_F('CrElementsUrlListItemTest', 'All', function() {
  mocha.run();
});
