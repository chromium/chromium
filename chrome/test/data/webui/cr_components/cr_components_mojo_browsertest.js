// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 components using Mojo. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "components/history_clusters/core/features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for shared Polymer 3 components using Mojo. */
var CrComponentsMojoBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
};

var CrComponentsCustomizeThemesTest =
    class extends CrComponentsMojoBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=cr_components/customize_themes_test.js';
  }
};

TEST_F('CrComponentsCustomizeThemesTest', 'All', function() {
  mocha.run();
});

var CrComponentsHelpBubbleMixinTest =
    class extends CrComponentsMojoBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=cr_components/help_bubble_mixin_test.js';
  }
};

TEST_F('CrComponentsHelpBubbleMixinTest', 'All', function() {
  mocha.run();
});

var CrComponentsHelpBubbleTest = class extends CrComponentsMojoBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=cr_components/help_bubble_test.js';
  }
};

TEST_F('CrComponentsHelpBubbleTest', 'All', function() {
  mocha.run();
});

var CrComponentsHistoryClustersTest =
    class extends CrComponentsMojoBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=cr_components/history_clusters_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'history_clusters::internal::kJourneysImages',
      ],
    };
  }
};

TEST_F('CrComponentsHistoryClustersTest', 'All', function() {
  mocha.run();
});

var CrComponentsMostVisitedTest = class extends CrComponentsMojoBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=cr_components/most_visited_test.js';
  }
};

TEST_F('CrComponentsMostVisitedTest', 'General', function() {
  runMochaSuite('General');
});

TEST_F('CrComponentsMostVisitedTest', 'Layouts', function() {
  runMochaSuite('Layouts');
});

TEST_F('CrComponentsMostVisitedTest', 'LoggingAndUpdates', function() {
  runMochaSuite('LoggingAndUpdates');
});

// crbug.com/1226996
GEN('#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)');
GEN('#define MAYBE_Modification DISABLED_Modification');
GEN('#else');
GEN('#define MAYBE_Modification Modification');
GEN('#endif');
TEST_F('CrComponentsMostVisitedTest', 'MAYBE_Modification', function() {
  runMochaSuite('Modification');
});

TEST_F('CrComponentsMostVisitedTest', 'DragAndDrop', function() {
  runMochaSuite('DragAndDrop');
});

TEST_F('CrComponentsMostVisitedTest', 'Theming', function() {
  runMochaSuite('Theming');
});
