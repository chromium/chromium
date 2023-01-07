// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interactive tests for shared Polymer 3 components using Mojo.
 */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

class CrComponentsMojoInteractiveTest extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var CrComponentsMostVisitedFocusTest =
    class extends CrComponentsMojoInteractiveTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=cr_components/most_visited_focus_test.js';
  }
};

TEST_F('CrComponentsMostVisitedFocusTest', 'All', function() {
  mocha.run();
});
