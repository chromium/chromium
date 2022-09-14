// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for Bookmarks which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

const BookmarksFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
};

var BookmarksFolderNodeFocusTest = class extends BookmarksFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/folder_node_focus_test.js';
  }
};

// http://crbug.com/1000950 : Flaky.
GEN('#define MAYBE_All DISABLED_All');
TEST_F('BookmarksFolderNodeFocusTest', 'MAYBE_All', function() {
  mocha.run();
});

var BookmarksListFocusTest = class extends BookmarksFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/list_focus_test.js';
  }
};

// http://crbug.com/1000950 : Flaky.
GEN('#define MAYBE_All DISABLED_All');
TEST_F('BookmarksListFocusTest', 'MAYBE_All', function() {
  mocha.run();
});

var BookmarksDialogFocusManagerTest = class extends BookmarksFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/dialog_focus_manager_test.js';
  }
};

// http://crbug.com/1000950 : Flaky.
GEN('#define MAYBE_All DISABLED_All');
TEST_F('BookmarksDialogFocusManagerTest', 'MAYBE_All', function() {
  mocha.run();
});
