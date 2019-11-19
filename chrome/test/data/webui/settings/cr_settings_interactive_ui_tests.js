// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

/**
 * Test fixture for interactive Polymer Settings elements.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
function CrSettingsInteractiveUITest() {}

CrSettingsInteractiveUITest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  },

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};


/**
 * Test fixture for Sync Page.
 * @constructor
 * @extends {CrSettingsInteractiveUITest}
 */
function CrSettingsSyncPageTest() {}

CrSettingsSyncPageTest.prototype = {
  __proto__: CrSettingsInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/sync_page.html',

  /** @override */
  extraLibraries: CrSettingsInteractiveUITest.prototype.extraLibraries.concat([
    'people_page_sync_page_interactive_test.js',
  ]),
};

TEST_F('CrSettingsSyncPageTest', 'All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {CrSettingsInteractiveUITest}
 */
function CrSettingsAnimatedPagesTest() {}

CrSettingsAnimatedPagesTest.prototype = {
  __proto__: CrSettingsInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/settings_page/settings_animated_pages.html',

  extraLibraries: CrSettingsInteractiveUITest.prototype.extraLibraries.concat([
    'settings_animated_pages_test.js',
  ]),
};

TEST_F('CrSettingsAnimatedPagesTest', 'All', function() {
  mocha.run();
});
