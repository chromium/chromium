// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');

class TabSearchBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var TabSearchAppTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_app_test.js';
  }

  get featureList() {
    return {enabled: ['features::kTabSearchUseMetricsReporter']};
  }
};

TEST_F('TabSearchAppTest', 'All', function() {
  mocha.run();
});

var TabSearchBiMapTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/bimap_test.js';
  }
};

TEST_F('TabSearchBiMapTest', 'All', function() {
  mocha.run();
});

var TabSearchFuzzySearchTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/fuzzy_search_test.js';
  }
};

TEST_F('TabSearchFuzzySearchTest', 'All', function() {
  mocha.run();
});

var TabSearchInfiniteListTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/infinite_list_test.js';
  }
};

TEST_F('TabSearchInfiniteListTest', 'All', function() {
  mocha.run();
});

var TabSearchItemTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_item_test.js';
  }
};

TEST_F('TabSearchItemTest', 'All', function() {
  mocha.run();
});

var TabSearchMediaTabsTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_media_tabs_test.js';
  }
};

TEST_F('TabSearchMediaTabsTest', 'All', function() {
  mocha.run();
});
