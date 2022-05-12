// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/* eslint-disable no-var */

class TabSearchBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var TabSearchAppTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_app_test.js&host=webui-test';
  }

  get featureList() {
    return {enabled: ['features::kTabSearchUseMetricsReporter']};
  }
};

// This times out regularly on debug builds, see https://crbug.com/1311655
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('TabSearchAppTest', 'MAYBE_All', function() {
  mocha.run();
});

var BiMapTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/bimap_test.js&host=webui-test';
  }
};

TEST_F('BiMapTest', 'All', function() {
  mocha.run();
});

var FuzzySearchTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/fuzzy_search_test.js&host=webui-test';
  }
};

TEST_F('FuzzySearchTest', 'All', function() {
  mocha.run();
});

var InfiniteListTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/infinite_list_test.js&host=webui-test';
  }
};

TEST_F('InfiniteListTest', 'All', function() {
  mocha.run();
});

var TabSearchItemTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_item_test.js&host=webui-test';
  }
};

TEST_F('TabSearchItemTest', 'All', function() {
  mocha.run();
});
