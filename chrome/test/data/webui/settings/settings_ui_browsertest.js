// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings layout. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsUIBrowserTest() {}

SettingsUIBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  extraLibraries: SettingsPageBrowserTest.prototype.extraLibraries.concat([
    'test_util.js',
  ]),
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434
// and http://crbug.com/711256.

GEN('#if !defined(NDEBUG) || defined(OS_MACOSX) || defined(OS_WINDOWS)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('SettingsUIBrowserTest', 'MAYBE_All', function() {
  suite('settings-ui', function() {
    let toolbar;
    let ui;

    suiteSetup(function() {
      testing.Test.disableAnimationsAndTransitions();
      ui = assert(document.querySelector('settings-ui'));
      ui.$.drawerTemplate.restamp = true;
    });

    setup(function() {
      ui.$.drawerTemplate.if = false;
      Polymer.dom.flush();
    });

    test('basic', function() {
      toolbar = assert(ui.$$('cr-toolbar'));
      assertTrue(toolbar.showMenu);
    });

    test('app drawer', function() {
      assertEquals(null, ui.$$('settings-menu'));
      const drawer = ui.$.drawer;
      assertFalse(!!drawer.open);

      const whenDone = test_util.eventToPromise('cr-drawer-opened', drawer);
      drawer.openDrawer();
      Polymer.dom.flush();

      // Validate that dialog is open and menu is shown so it will animate.
      assertTrue(drawer.open);
      assertTrue(!!ui.$$('settings-menu'));

      return whenDone
          .then(function() {
            const whenClosed = test_util.eventToPromise('close', drawer);
            drawer.cancel();
            return whenClosed;
          })
          .then(() => {
            // Drawer is closed, but menu is still stamped so
            // its contents remain visible as the drawer slides
            // out.
            assertTrue(!!ui.$$('settings-menu'));
          });
    });

    test('advanced UIs stay in sync', function() {
      const main = ui.$$('settings-main');
      assertTrue(!!main);

      assertFalse(!!ui.$$('settings-menu'));
      assertFalse(ui.advancedOpened_);
      assertFalse(main.advancedToggleExpanded);

      main.advancedToggleExpanded = true;
      Polymer.dom.flush();

      assertFalse(!!ui.$$('settings-menu'));
      assertTrue(ui.advancedOpened_);
      assertTrue(main.advancedToggleExpanded);

      ui.$.drawerTemplate.if = true;
      Polymer.dom.flush();

      const menu = ui.$$('settings-menu');
      assertTrue(!!menu);
      assertTrue(menu.advancedOpened);

      menu.$.advancedButton.click();
      Polymer.dom.flush();

      // Check that all values are updated in unison.
      assertFalse(menu.advancedOpened);
      assertFalse(ui.advancedOpened_);
      assertFalse(main.advancedToggleExpanded);
    });

    test('URL initiated search propagates to search box', function() {
      toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
      const searchField =
          /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());
      assertEquals('', searchField.getSearchInput().value);

      const query = 'foo';
      settings.navigateTo(
          settings.routes.BASIC, new URLSearchParams(`search=${query}`));
      assertEquals(query, searchField.getSearchInput().value);
    });

    test('search box initiated search propagates to URL', function() {
      toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
      const searchField =
          /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());

      settings.navigateTo(
          settings.routes.BASIC, /* dynamicParams */ null,
          /* removeSearch */ true);
      assertEquals('', searchField.getSearchInput().value);
      assertFalse(settings.getQueryParameters().has('search'));

      let value = 'GOOG';
      searchField.setValue(value);
      assertEquals(value, settings.getQueryParameters().get('search'));

      // Test that search queries are properly URL encoded.
      value = '+++';
      searchField.setValue(value);
      assertEquals(value, settings.getQueryParameters().get('search'));
    });

    test('whitespace only search query is ignored', function() {
      toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
      const searchField =
          /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());
      searchField.setValue('    ');
      let urlParams = settings.getQueryParameters();
      assertFalse(urlParams.has('search'));

      searchField.setValue('   foo');
      urlParams = settings.getQueryParameters();
      assertEquals('foo', urlParams.get('search'));

      searchField.setValue('   foo ');
      urlParams = settings.getQueryParameters();
      assertEquals('foo ', urlParams.get('search'));

      searchField.setValue('   ');
      urlParams = settings.getQueryParameters();
      assertFalse(urlParams.has('search'));
    });
  });

  mocha.run();
});
