// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Material Design history page.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "base/command_line.h"');
GEN('#include "build/build_config.h"');
GEN('#include "components/history_clusters/core/features.h"');
GEN('#include "chrome/test/data/webui/history_ui_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

const HistoryBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/';
  }
};

var HistoryDrawerTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_drawer_test.js';
  }
};

TEST_F('HistoryDrawerTest', 'All', function() {
  mocha.run();
});

var HistoryItemTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_item_test.js';
  }
};

TEST_F('HistoryItemTest', 'All', function() {
  mocha.run();
});

var HistoryLinkClickTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/link_click_test.js';
  }
};

TEST_F('HistoryLinkClickTest', 'All', function() {
  mocha.run();
});

var HistoryListTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_list_test.js';
  }

  /** @override */
  get suiteName() {
    return history_list_test.suiteName;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

TEST_F('HistoryListTest', 'DeletingSingleItem', function() {
  this.runMochaTest(history_list_test.TestNames.DeletingSingleItem);
});

TEST_F('HistoryListTest', 'CancellingSelectionOfMultipleItems', function() {
  this.runMochaTest(
      history_list_test.TestNames.CancellingSelectionOfMultipleItems);
});

TEST_F(
    'HistoryListTest', 'SelectionOfMultipleItemsUsingShiftClick', function() {
      this.runMochaTest(
          history_list_test.TestNames.SelectionOfMultipleItemsUsingShiftClick);
    });

TEST_F('HistoryListTest', 'DisablingCtrlAOnSyncedTabsPage', function() {
  this.runMochaTest(history_list_test.TestNames.DisablingCtrlAOnSyncedTabsPage);
});

TEST_F('HistoryListTest', 'SettingFirstAndLastItems', function() {
  this.runMochaTest(history_list_test.TestNames.SettingFirstAndLastItems);
});

TEST_F('HistoryListTest', 'UpdatingHistoryResults', function() {
  this.runMochaTest(history_list_test.TestNames.UpdatingHistoryResults);
});

TEST_F('HistoryListTest', 'DeletingMultipleItemsFromView', function() {
  this.runMochaTest(history_list_test.TestNames.DeletingMultipleItemsFromView);
});

TEST_F(
    'HistoryListTest', 'SearchResultsDisplayWithCorrectItemTitle', function() {
      this.runMochaTest(
          history_list_test.TestNames.SearchResultsDisplayWithCorrectItemTitle);
    });

TEST_F(
    'HistoryListTest', 'CorrectDisplayMessageWhenNoHistoryAvailable',
    function() {
      this.runMochaTest(history_list_test.TestNames
                            .CorrectDisplayMessageWhenNoHistoryAvailable);
    });

TEST_F(
    'HistoryListTest', 'MoreFromThisSiteSendsAndSetsCorrectData', function() {
      this.runMochaTest(
          history_list_test.TestNames.MoreFromThisSiteSendsAndSetsCorrectData);
    });

TEST_F('HistoryListTest', 'ChangingSearchDeselectsItems', function() {
  this.runMochaTest(history_list_test.TestNames.ChangingSearchDeselectsItems);
});

TEST_F('HistoryListTest', 'DeleteItemsEndToEnd', function() {
  this.runMochaTest(history_list_test.TestNames.DeleteItemsEndToEnd);
});

TEST_F('HistoryListTest', 'DeleteViaMenuButton', function() {
  this.runMochaTest(history_list_test.TestNames.DeleteViaMenuButton);
});

TEST_F('HistoryListTest', 'DeleteDisabledWhilePending', function() {
  this.runMochaTest(history_list_test.TestNames.DeleteDisabledWhilePending);
});

TEST_F('HistoryListTest', 'DeletingItemsUsingShortcuts', function() {
  this.runMochaTest(history_list_test.TestNames.DeletingItemsUsingShortcuts);
});

TEST_F('HistoryListTest', 'DeleteDialogClosedOnBackNavigation', function() {
  this.runMochaTest(
      history_list_test.TestNames.DeleteDialogClosedOnBackNavigation);
});

TEST_F('HistoryListTest', 'ClickingFileUrlSendsMessageToChrome', function() {
  this.runMochaTest(
      history_list_test.TestNames.ClickingFileUrlSendsMessageToChrome);
});

TEST_F(
    'HistoryListTest', 'DeleteHistoryResultsInQueryHistoryEvent', function() {
      this.runMochaTest(
          history_list_test.TestNames.DeleteHistoryResultsInQueryHistoryEvent);
    });

var HistoryMetricsTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_metrics_test.js';
  }

  /** @override */
  get featureList() {
    return {
      disabled: [
        'history_clusters::kRenameJourneys',
      ],
    };
  }
};

TEST_F('HistoryMetricsTest', 'All', function() {
  mocha.run();
});

var HistoryOverflowMenuTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_overflow_menu_test.js';
  }
};

TEST_F('HistoryOverflowMenuTest', 'All', function() {
  mocha.run();
});

var HistoryRoutingTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_routing_test.js';
  }

  /** @override */
  get featureList() {
    return {
      disabled: [
        'history_clusters::kRenameJourneys',
      ],
    };
  }
};

TEST_F('HistoryRoutingTest', 'All', function() {
  mocha.run();
});

var HistoryRoutingWithQueryParamTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_routing_with_query_param_test.js';
  }
};

TEST_F('HistoryRoutingWithQueryParamTest', 'All', function() {
  mocha.run();
});

var HistorySyncedTabsTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_synced_tabs_test.js';
  }
};

TEST_F('HistorySyncedTabsTest', 'All', function() {
  mocha.run();
});

var HistorySupervisedUserTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_supervised_user_test.js';
  }

  get typedefCppFixture() {
    return 'HistoryUIBrowserTest';
  }

  /** @override */
  testGenPreamble() {
    GEN('  SetDeleteAllowed(false);');
  }
};

GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_AllSupervised DISABLED_All');
GEN('#else');
GEN('#define MAYBE_AllSupervised All');
GEN('#endif');

TEST_F('HistorySupervisedUserTest', 'MAYBE_AllSupervised', function() {
  mocha.run();
});

var HistoryToolbarTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_toolbar_test.js';
  }
};

TEST_F('HistoryToolbarTest', 'All', function() {
  mocha.run();
});

var HistorySearchedLabelTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/searched_label_test.js';
  }
};

TEST_F('HistorySearchedLabelTest', 'All', function() {
  mocha.run();
});
