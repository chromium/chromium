// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "content/public/test/browser_test.h"');

class NewTabPageBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }
}

// eslint-disable-next-line no-var
var NewTabPageAppTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/app_test.js';
  }
};

// TODO(https://crbug.com/1253309): Flaky on Linux debug builds.
GEN('#if defined(OS_LINUX) && !defined(NDEBUG)');
GEN('#define MAYBE_NewTabPageAppTestAll DISABLED_All');
GEN('#else');
GEN('#define MAYBE_NewTabPageAppTestAll All');
GEN('#endif');

TEST_F('NewTabPageAppTest', 'MAYBE_NewTabPageAppTestAll', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_dialog_test.js';
  }
};

TEST_F('NewTabPageCustomizeDialogTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageUtilsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/utils_test.js';
  }
};

TEST_F('NewTabPageUtilsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageMetricsUtilsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/metrics_utils_test.js';
  }
};

TEST_F('NewTabPageMetricsUtilsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeShortcutsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_shortcuts_test.js';
  }
};

TEST_F('NewTabPageCustomizeShortcutsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeModulesTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_modules_test.js';
  }
};

TEST_F('NewTabPageCustomizeModulesTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeBackgroundsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_backgrounds_test.js';
  }
};

TEST_F('NewTabPageCustomizeBackgroundsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageVoiceSearchOverlayTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/voice_search_overlay_test.js';
  }
};

TEST_F('NewTabPageVoiceSearchOverlayTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageRealboxTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/realbox/realbox_test.js';
  }
};

TEST_F('NewTabPageRealboxTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageLogoTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/logo_test.js';
  }
};

TEST_F('NewTabPageLogoTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageDoodleShareDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/doodle_share_dialog_test.js';
  }
};

TEST_F('NewTabPageDoodleShareDialogTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageBackgroundManagerTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/background_manager_test.js';
  }
};

TEST_F('NewTabPageBackgroundManagerTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesModuleWrapperTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/module_wrapper_test.js';
  }
};

TEST_F('NewTabPageModulesModuleWrapperTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesModulesTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/modules_test.js';
  }
};

TEST_F('NewTabPageModulesModulesTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesModuleDescriptorTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/module_descriptor_test.js';
  }
};

TEST_F('NewTabPageModulesModuleDescriptorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesModuleRegistryTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/module_registry_test.js';
  }
};

TEST_F('NewTabPageModulesModuleRegistryTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesModuleHeaderTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/module_header_test.js';
  }
};

TEST_F('NewTabPageModulesModuleHeaderTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesInfoDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/info_dialog_test.js';
  }
};

TEST_F('NewTabPageModulesInfoDialogTest', 'All', function() {
  mocha.run();
});

// The dummy module is not available in official builds.
GEN('#if !defined(OFFICIAL_BUILD)');

// eslint-disable-next-line no-var
var NewTabPageModulesDummyModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/dummy/module_test.js';
  }
};

TEST_F('NewTabPageModulesDummyModuleTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OFFICIAL_BUILD)');

// eslint-disable-next-line no-var
var NewTabPageMiddleSlotPromoTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/middle_slot_promo_test.js';
  }
};

TEST_F('NewTabPageMiddleSlotPromoTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesDriveModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/drive/module_test.js';
  }
};

TEST_F('NewTabPageModulesDriveModuleTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesDriveV2ModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/drive_v2/module_test.js';
  }
};

TEST_F('NewTabPageModulesDriveV2ModuleTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesTaskModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/task_module/module_test.js';
  }
};

TEST_F('NewTabPageModulesTaskModuleTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesRecipesV2ModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/recipes_v2/module_test.js';
  }
};

TEST_F('NewTabPageModulesRecipesV2ModuleTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesChromeCartModuleTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/cart/module_test.js';
  }
};

TEST_F('NewTabPageModulesChromeCartModuleTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageModulesChromeCartV2ModuleTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/cart_v2/module_test.js';
  }
};

GEN('#if !defined(OFFICIAL_BUILD)');

// eslint-disable-next-line no-var
var NewTabPageModulesPhotosModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/photos/module_test.js';
  }
};

TEST_F('NewTabPageModulesPhotosModuleTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OFFICIAL_BUILD)');

// https://crbug.com/1227564: Flaky on Chrome OS.
GEN('#if defined(OS_CHROMEOS)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('NewTabPageModulesChromeCartV2ModuleTest', 'MAYBE_All', function() {
  mocha.run();
});
