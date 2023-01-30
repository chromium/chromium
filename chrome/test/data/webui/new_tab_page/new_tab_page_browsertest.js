// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');

class NewTabPageBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var NewTabPageAppTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/app_test.js';
  }
};

TEST_F('NewTabPageAppTest', 'Misc', function() {
  runMochaSuite('NewTabPageAppTest misc');
});

TEST_F('NewTabPageAppTest', 'OgbThemingRemoveScrimFalse', function() {
  runMochaSuite('NewTabPageAppTest ogb theming removeScrim is false');
});

TEST_F('NewTabPageAppTest', 'OgbThemingRemoveScrimTrue', function() {
  runMochaSuite('NewTabPageAppTest ogb theming removeScrim is true');
});

TEST_F('NewTabPageAppTest', 'Theming', function() {
  runMochaSuite('NewTabPageAppTest theming');
});

TEST_F('NewTabPageAppTest', 'Promo', function() {
  runMochaSuite('NewTabPageAppTest promo');
});

TEST_F('NewTabPageAppTest', 'Clicks', function() {
  runMochaSuite('NewTabPageAppTest clicks');
});

TEST_F('NewTabPageAppTest', 'Modules', function() {
  runMochaSuite('NewTabPageAppTest modules');
});

TEST_F('NewTabPageAppTest', 'CounterfactualModules', function() {
  runMochaSuite('NewTabPageAppTest counterfactual modules');
});

TEST_F('NewTabPageAppTest', 'CustomizeUrl', function() {
  runMochaSuite('NewTabPageAppTest customize URL');
});

TEST_F('NewTabPageAppTest', 'CustomizeChromeSidePanel', function() {
  runMochaSuite('NewTabPageAppTest customize chrome side panel');
});

TEST_F('NewTabPageAppTest', 'LensUploadDialog', function() {
  runMochaSuite('NewTabPageAppTest Lens upload dialog');
});

var NewTabPageCustomizeDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_dialog_test.js';
  }
};

TEST_F('NewTabPageCustomizeDialogTest', 'All', function() {
  mocha.run();
});

var NewTabPageUtilsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/utils_test.js';
  }
};

TEST_F('NewTabPageUtilsTest', 'All', function() {
  mocha.run();
});

var NewTabPageMetricsUtilsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/metrics_utils_test.js';
  }
};

TEST_F('NewTabPageMetricsUtilsTest', 'All', function() {
  mocha.run();
});

var NewTabPageCustomizeShortcutsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_shortcuts_test.js';
  }
};

TEST_F('NewTabPageCustomizeShortcutsTest', 'All', function() {
  mocha.run();
});

var NewTabPageCustomizeModulesTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_modules_test.js';
  }
};

TEST_F('NewTabPageCustomizeModulesTest', 'All', function() {
  mocha.run();
});

var NewTabPageCustomizeBackgroundsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_backgrounds_test.js';
  }
};

TEST_F('NewTabPageCustomizeBackgroundsTest', 'All', function() {
  mocha.run();
});

var NewTabPageVoiceSearchOverlayTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/voice_search_overlay_test.js';
  }
};

TEST_F('NewTabPageVoiceSearchOverlayTest', 'All', function() {
  mocha.run();
});

var NewTabPageLensFormTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/lens_form_test.js';
  }
};

TEST_F('NewTabPageLensFormTest', 'All', function() {
  mocha.run();
});

var NewTabPageLensUploadDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/lens_upload_dialog_test.js';
  }
};

TEST_F('NewTabPageLensUploadDialogTest', 'All', function() {
  mocha.run();
});

var NewTabPageRealboxTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/realbox/realbox_test.js';
  }
};

TEST_F('NewTabPageRealboxTest', 'All', function() {
  mocha.run();
});

var NewTabPageRealboxLensTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/realbox/lens_test.js';
  }
};

TEST_F('NewTabPageRealboxLensTest', 'All', function() {
  mocha.run();
});

var NewTabPageLogoTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/logo_test.js';
  }
};

TEST_F('NewTabPageLogoTest', 'All', function() {
  mocha.run();
});

var NewTabPageDoodleShareDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/doodle_share_dialog_test.js';
  }
};

TEST_F('NewTabPageDoodleShareDialogTest', 'All', function() {
  mocha.run();
});

var NewTabPageBackgroundManagerTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/background_manager_test.js';
  }
};

TEST_F('NewTabPageBackgroundManagerTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesModuleWrapperTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/module_wrapper_test.js';
  }
};

TEST_F('NewTabPageModulesModuleWrapperTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesModulesTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/modules_test.js';
  }
};

TEST_F('NewTabPageModulesModulesTest', 'All', function() {
  mocha.run();
});

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

var NewTabPageModulesModuleRegistryTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/module_registry_test.js';
  }
};

TEST_F('NewTabPageModulesModuleRegistryTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesModuleHeaderTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/module_header_test.js';
  }
};

TEST_F('NewTabPageModulesModuleHeaderTest', 'All', function() {
  mocha.run();
});

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

var NewTabPageModulesDummyModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/dummy_v2/module_test.js';
  }
};

TEST_F('NewTabPageModulesDummyModuleTest', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OFFICIAL_BUILD)');

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

var NewTabPageModulesDriveV2ModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/drive_v2/module_test.js';
  }
};

TEST_F('NewTabPageModulesDriveV2ModuleTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesRecipesTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/recipes/module_test.js';
  }
};

TEST_F('NewTabPageModulesRecipesTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesRecipesV2ModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/recipes_v2/module_test.js';
  }
};

TEST_F('NewTabPageModulesRecipesV2ModuleTest', 'All', function() {
  mocha.run();
});

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

var NewTabPageModulesChromeCartV2ModuleTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/cart_v2/module_test.js';
  }
};

var NewTabPageModulesFeedModuleTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/feed/module_test.js';
  }
};

TEST_F('NewTabPageModulesFeedModuleTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesHistoryClustersModuleTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/history_clusters/module_test.js';
  }
};

TEST_F('NewTabPageModulesHistoryClustersModuleTest', 'All', function() {
  mocha.run();
});

// https://crbug.com/1227564: Flaky on Chrome OS.
GEN('#if BUILDFLAG(IS_CHROMEOS)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('NewTabPageModulesChromeCartV2ModuleTest', 'MAYBE_All', function() {
  mocha.run();
});
GEN('#undef MAYBE_All');

GEN('#if !defined(OFFICIAL_BUILD)');

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

var NewTabPageDiscountConsentDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/cart/discount_consent_dialog_test.js';
  }
};

TEST_F('NewTabPageDiscountConsentDialogTest', 'All', function() {
  mocha.run();
});

var NewTabPageDiscountConsentCartTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/cart/discount_consent_card_test.js';
  }
};

TEST_F('NewTabPageDiscountConsentCartTest', 'All', function() {
  mocha.run();
});
