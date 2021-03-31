// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

class TabSearchBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }
}

// eslint-disable-next-line no-var
var TabSearchAppTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_app_test.js';
  }
};

TEST_F('TabSearchAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BiMapTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/bimap_test.js';
  }
};

TEST_F('BiMapTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var FuzzySearchTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/fuzzy_search_test.js';
  }
};

TEST_F('FuzzySearchTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var InfiniteListTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/infinite_list_test.js';
  }
};

TEST_F('InfiniteListTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var TabSearchItemTest = class extends TabSearchBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_item_test.js';
  }
};

TEST_F('TabSearchItemTest', 'All', function() {
  mocha.run();
});
