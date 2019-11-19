// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Material Help page tests. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsHelpPageBrowserTest() {}

SettingsHelpPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://help/',

  /** @override */
  setUp: function() {
    // Intentionally bypassing SettingsPageBrowserTest#setUp.
    PolymerTest.prototype.setUp.call(this);
  },
};

TEST_F('SettingsHelpPageBrowserTest', 'Load', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  const self = this;

  // Register mocha tests.
  suite('Help page', function() {
    test('about section', function() {
      return self.getPage('about').then(function(page) {
        expectTrue(!!self.getSection(page, 'about'));
      });
    });
  });

  // Run all registered tests.
  mocha.run();
});
