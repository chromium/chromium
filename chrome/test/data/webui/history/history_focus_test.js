// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for History which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "build/build_config.h"');
GEN('#include "content/public/test/browser_test.h"');

const HistoryFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/';
  }
};

var HistoryToolbarFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_toolbar_focus_test.js';
  }
};

GEN('#if BUILDFLAG(IS_MAC)');
GEN('// Flaky, https://crbug.com/1200678');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('HistoryToolbarFocusTest', 'MAYBE_All', function() {
  mocha.run();
});
GEN('#undef MAYBE_All');

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

var HistorySyncedDeviceManagerFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_synced_device_manager_focus_test.js';
  }
};

TEST_F('HistorySyncedDeviceManagerFocusTest', 'All', function() {
  mocha.run();
});

var HistoryItemFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_item_focus_test.js';
  }
};

TEST_F('HistoryItemFocusTest', 'All', function() {
  mocha.run();
});
