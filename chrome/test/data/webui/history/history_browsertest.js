// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Material Design history page.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "base/command_line.h"');
GEN('#include "chrome/test/data/webui/history_ui_browsertest.h"');

function HistoryBrowserTest() {}

HistoryBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://history',

  /** @override */
  runAccessibilityChecks: false,

  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '../test_util.js',
    'test_util.js',
  ],

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    suiteSetup(function() {
      // Wait for the top-level app element to be upgraded.
      return waitForAppUpgrade()
          .then(function() {
            return history.ensureLazyLoaded();
          })
          .then(function() {
            $('history-app').queryState_.queryingDisabled = true;
          });
    });
  },
};

function HistoryBrowserServiceTest() {}

HistoryBrowserServiceTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'browser_service_test.js',
  ]),
};

TEST_F('HistoryBrowserServiceTest', 'All', function() {
  mocha.run();
});

function HistoryDrawerTest() {}

HistoryDrawerTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_drawer_test.js',
  ]),
};

TEST_F('HistoryDrawerTest', 'All', function() {
  mocha.run();
});

function HistoryItemTest() {}

HistoryItemTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_item_test.js',
  ]),
};

TEST_F('HistoryItemTest', 'All', function() {
  mocha.run();
});

function HistoryListTest() {}

HistoryListTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_list_test.js',
  ]),
};

// Times out on debug builders because the History page can take several seconds
// to load in a Debug build. See https://crbug.com/669227.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('HistoryListTest', 'MAYBE_All', function() {
  mocha.run();
});

function HistoryMetricsTest() {}

HistoryMetricsTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_metrics_test.js',
  ]),
};

TEST_F('HistoryMetricsTest', 'All', function() {
  mocha.run();
});

function HistoryOverflowMenuTest() {}

HistoryOverflowMenuTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_overflow_menu_test.js',
  ]),
};

TEST_F('HistoryOverflowMenuTest', 'All', function() {
  mocha.run();
});

function HistoryRoutingTest() {}

HistoryRoutingTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_routing_test.js',
  ]),
};

TEST_F('HistoryRoutingTest', 'All', function() {
  history.history_routing_test.registerTests();
  mocha.run();
});

function HistoryRoutingWithQueryParamTest() {}

HistoryRoutingWithQueryParamTest.prototype = {
  __proto__: HistoryRoutingTest.prototype,

  browsePreload: 'chrome://history/?q=query',

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // This message handler needs to be registered before the test since the
    // query can happen immediately after the element is upgraded. However,
    // since there may be a delay as well, the test might check the global var
    // too early as well. In this case the test will have overtaken the
    // callback.
    registerMessageCallback('queryHistory', this, function(info) {
      window.historyQueryInfo = info;
    });

    suiteSetup(function() {
      // Wait for the top-level app element to be upgraded.
      return waitForAppUpgrade().then(function() {
        history.ensureLazyLoaded();
      });
    });
  },
};

TEST_F('HistoryRoutingWithQueryParamTest', 'All', function() {
  history.history_routing_test_with_query_param.registerTests();
  mocha.run();
});

function HistorySyncedTabsTest() {}

HistorySyncedTabsTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_synced_tabs_test.js',
  ]),
};

TEST_F('HistorySyncedTabsTest', 'All', function() {
  mocha.run();
});

function HistorySupervisedUserTest() {}

HistorySupervisedUserTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  typedefCppFixture: 'HistoryUIBrowserTest',

  testGenPreamble: function() {
    GEN('  SetDeleteAllowed(false);');
  },

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_supervised_user_test.js',
  ]),
};

TEST_F('HistorySupervisedUserTest', 'All', function() {
  mocha.run();
});

function HistoryToolbarTest() {}

HistoryToolbarTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'history_toolbar_test.js',
  ]),
};

TEST_F('HistoryToolbarTest', 'All', function() {
  mocha.run();
});

function HistorySearchedLabelTest() {}

HistorySearchedLabelTest.prototype = {
  __proto__: HistoryBrowserTest.prototype,

  extraLibraries: HistoryBrowserTest.prototype.extraLibraries.concat([
    'searched_label_test.js',
  ]),
};

TEST_F('HistorySearchedLabelTest', 'All', function() {
  mocha.run();
});
