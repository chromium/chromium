// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for History which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

const HistoryFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/';
  }
};

// eslint-disable-next-line no-var
var HistoryToolbarFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_toolbar_focus_test.js';
  }
};

TEST_F('HistoryToolbarFocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryListFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_list_focus_test.js';
  }
};

// Flaky. See crbug.com/1040940.
TEST_F('HistoryListFocusTest', 'DISABLED_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistorySyncedDeviceManagerFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_synced_device_manager_focus_test.js';
  }
};

TEST_F('HistorySyncedDeviceManagerFocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryItemFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_item_focus_test.js';
  }
};

TEST_F('HistoryItemFocusTest', 'All', function() {
  mocha.run();
});
