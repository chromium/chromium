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
    '../test_util.js',
  ]),
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434
// and http://crbug.com/711256.

// Disabling everywhere, see flaky failures at crbug.com/986985.
TEST_F('SettingsUIBrowserTest', 'DISABLED_All', function() {
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

    test('showing menu in toolbar is dependent on narrow mode', function() {
      toolbar = assert(ui.$$('cr-toolbar'));
      toolbar.narrow = true;
      assertTrue(toolbar.showMenu);

      toolbar.narrow = false;
      assertFalse(toolbar.showMenu);
    });

    test('app drawer', function() {
      assertEquals(null, ui.$$('cr-drawer settings-menu'));
      const drawer = ui.$.drawer;
      assertFalse(!!drawer.open);

      const whenDone = test_util.eventToPromise('cr-drawer-opened', drawer);
      drawer.openDrawer();
      Polymer.dom.flush();

      // Validate that dialog is open and menu is shown so it will animate.
      assertTrue(drawer.open);
      assertTrue(!!ui.$$('cr-drawer settings-menu'));

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
            assertTrue(!!ui.$$('cr-drawer settings-menu'));
          });
    });

    test('app drawer closes when exiting narrow mode', async () => {
      const drawer = ui.$.drawer;
      const toolbar = ui.$$('cr-toolbar');

      // Mimic narrow mode and open the drawer
      toolbar.narrow = true;
      drawer.openDrawer();
      Polymer.dom.flush();
      await test_util.eventToPromise('cr-drawer-opened', drawer);

      toolbar.narrow = false;
      Polymer.dom.flush();
      await test_util.eventToPromise('close', drawer);
      assertFalse(drawer.open);
    });

    test('advanced UIs stay in sync', function() {
      const main = ui.$$('settings-main');
      const floatingMenu = ui.$$('#left settings-menu');
      assertTrue(!!main);
      assertTrue(!!floatingMenu);

      assertFalse(!!ui.$$('cr-drawer settings-menu'));
      assertFalse(ui.advancedOpenedInMain_);
      assertFalse(ui.advancedOpenedInMenu_);
      assertFalse(floatingMenu.advancedOpened);
      assertFalse(main.advancedToggleExpanded);

      main.advancedToggleExpanded = true;
      Polymer.dom.flush();

      assertFalse(!!ui.$$('cr-drawer settings-menu'));
      assertTrue(ui.advancedOpenedInMain_);
      assertTrue(ui.advancedOpenedInMenu_);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(main.advancedToggleExpanded);

      ui.$.drawerTemplate.if = true;
      Polymer.dom.flush();

      const drawerMenu = ui.$$('cr-drawer settings-menu');
      assertTrue(!!drawerMenu);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(drawerMenu.advancedOpened);

      // Collapse 'Advanced' in the menu
      drawerMenu.$.advancedButton.click();
      Polymer.dom.flush();

      // Collapsing it in the menu should not collapse it in the main area
      assertFalse(drawerMenu.advancedOpened);
      assertFalse(floatingMenu.advancedOpened);
      assertFalse(ui.advancedOpenedInMenu_);
      assertTrue(main.advancedToggleExpanded);
      assertTrue(ui.advancedOpenedInMain_);

      // Expand both 'Advanced's again
      drawerMenu.$.advancedButton.click();

      // Collapse 'Advanced' in the main area
      main.advancedToggleExpanded = false;
      Polymer.dom.flush();

      // Collapsing it in the main area should not collapse it in the menu
      assertFalse(ui.advancedOpenedInMain_);
      assertTrue(drawerMenu.advancedOpened);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(ui.advancedOpenedInMenu_);
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
