// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

var TabStripInteractiveUITest = class extends PolymerInteractiveUITest {
  get isAsync() {
    return true;
  }

  get webuiHost() {
    return 'tab-strip.top-chrome';
  }
};

var TabStripTabListTest = class extends TabStripInteractiveUITest {
  get browsePreload() {
    return 'chrome://tab-strip.top-chrome/test_loader.html?module=tab_strip/tab_list_test.js';
  }
};

TEST_F('TabStripTabListTest', 'All', function() {
  mocha.run();
});
