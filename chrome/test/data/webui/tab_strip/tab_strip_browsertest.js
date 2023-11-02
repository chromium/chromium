// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

var TabStripBrowserTest = class extends testing.Test {
  get isAsync() {
    return true;
  }

  get webuiHost() {
    return 'tab-strip.top-chrome';
  }
};

var TabStripTabListTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/tab_list_test.js';
  }
};

var TabStripTabTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/tab_test.js';
  }
};

TEST_F('TabStripTabTest', 'All', function() {
  mocha.run();
});

var TabStripAlertIndicatorsTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/alert_indicators_test.js';
  }
};

TEST_F('TabStripAlertIndicatorsTest', 'All', function() {
  mocha.run();
});

var TabStripAlertIndicatorTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/alert_indicator_test.js';
  }
};

TEST_F('TabStripAlertIndicatorTest', 'All', function() {
  mocha.run();
});

var TabStripTabSwiperTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/tab_swiper_test.js';
  }
};

TEST_F('TabStripTabSwiperTest', 'All', function() {
  mocha.run();
});

var TabStripTabGroupTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/tab_group_test.js';
  }
};

TEST_F('TabStripTabGroupTest', 'All', function() {
  mocha.run();
});

var TabStripDragManagerTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/drag_manager_test.js';
  }
};

TEST_F('TabStripDragManagerTest', 'All', function() {
  mocha.run();
});
