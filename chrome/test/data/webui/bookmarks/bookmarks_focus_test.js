// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for Bookmarks which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);
GEN('#include "services/network/public/cpp/features.h"');

const BookmarksFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {enabled: ['network::features::kOutOfBlinkCors']};
  }
};

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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
