// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 components using Mojo. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for shared Polymer 3 components using Mojo. */
// eslint-disable-next-line no-var
var CrComponentsMojoBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }
};

var CrComponentsCustomizeThemesTest =
    class extends CrComponentsMojoBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=cr_components/customize_themes_test.js';
  }
};

TEST_F('CrComponentsCustomizeThemesTest', 'All', function() {
  mocha.run();
});
