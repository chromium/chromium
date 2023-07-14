// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "components/search/ntp_features.h"');
GEN('#include "content/public/test/browser_test.h"');

class NewTabPageBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var NewTabPageModulesHistoryClustersModuleTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/history_clusters/module_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ntp_features::kNtpHistoryClustersModule',
      ],
    };
  }
};

TEST_F('NewTabPageModulesHistoryClustersModuleTest', 'Core', function() {
  runMochaSuite('NewTabPageModulesHistoryClustersModuleTest core');
});

TEST_F('NewTabPageModulesHistoryClustersModuleTest', 'Layouts', function() {
  runMochaSuite('NewTabPageModulesHistoryClustersModuleTest layouts');
});

TEST_F(
    'NewTabPageModulesHistoryClustersModuleTest',
    'UnloadMetricImageDisplayStateNone', function() {
      runMochaSuite(
          'NewTabPageModulesHistoryClustersModuleTest unload metric no images');
    });

TEST_F(
    'NewTabPageModulesHistoryClustersModuleTest',
    'UnloadMetricImageDisplayStateAll', function() {
      runMochaSuite(
          'NewTabPageModulesHistoryClustersModuleTest unload metric all images');
    });

TEST_F(
    'NewTabPageModulesHistoryClustersModuleTest', 'CartTileRendering',
    function() {
      runMochaSuite(
          'NewTabPageModulesHistoryClustersModuleTest cart tile rendering');
    });

var NewTabPageModulesHistoryClustersModuleTileTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/history_clusters/tile_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ntp_features::kNtpHistoryClustersModule',
      ],
    };
  }
};

TEST_F('NewTabPageModulesHistoryClustersModuleTileTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesHistoryClustersModuleSuggestTileTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/history_clusters/suggest_tile_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ntp_features::kNtpHistoryClustersModule',
      ],
    };
  }
};

TEST_F(
    'NewTabPageModulesHistoryClustersModuleSuggestTileTest', 'All', function() {
      mocha.run();
    });

var NewTabPageModulesHistoryClustersModuleCartTileTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/history_clusters/cart/cart_tile_test.js';
  }
};

TEST_F('NewTabPageModulesHistoryClustersModuleCartTileTest', 'All', function() {
  mocha.run();
});

var NewTabPageModulesHistoryClustersV2ModuleTest =
    class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/modules/v2/history_clusters/module_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ntp_features::kNtpHistoryClustersModule',
        'ntp_features::kNtpModulesRedesigned',
      ],
    };
  }
};

TEST_F('NewTabPageModulesHistoryClustersV2ModuleTest', 'Core', function() {
  runMochaSuite('NewTabPageModulesHistoryClustersV2ModuleTest core');
});

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
