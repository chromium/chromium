// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Path to general chrome browser settings and associated utilities.
const BROWSER_SETTINGS_PATH = '../';

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

// Only run in release builds because we frequently see test timeouts in debug.
// We suspect this is because the settings page loads slowly in debug.
// https://crbug.com/1003483
GEN('#if defined(NDEBUG)');

GEN('#include "chromeos/constants/chromeos_features.h"');

// Test fixture for the top-level OS settings UI.
// eslint-disable-next-line no-var
var OSSettingsUIBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat(
        BROWSER_SETTINGS_PATH + '../test_util.js');
  }
};

TEST_F('OSSettingsUIBrowserTest', 'AllJsTests', () => {
  suite('os-settings-ui', () => {
    let ui;

    suiteSetup(() => {
      testing.Test.disableAnimationsAndTransitions();
      ui = assert(document.querySelector('os-settings-ui'));
      ui.$.drawerTemplate.restamp = true;
    });

    setup(() => {
      ui.$.drawerTemplate.if = false;
      Polymer.dom.flush();
    });

    test('top container shadow always shows for sub-pages', () => {
      const element = ui.$$('#cr-container-shadow-top');
      assertTrue(!!element, 'Shadow container element always exists');

      assertFalse(
          element.classList.contains('has-shadow'),
          'Main page should not show shadow ' + element.className);

      settings.navigateTo(settings.routes.POWER);
      Polymer.dom.flush();
      assertTrue(
          element.classList.contains('has-shadow'),
          'Sub-page should show shadow ' + element.className);
    });

    test('showing menu in toolbar is dependent on narrow mode', () => {
      const toolbar = assert(ui.$$('os-toolbar'));
      toolbar.narrow = true;
      assertTrue(toolbar.showMenu);

      toolbar.narrow = false;
      assertFalse(toolbar.showMenu);
    });

    test('app drawer', async () => {
      assertEquals(null, ui.$$('cr-drawer os-settings-menu'));
      const drawer = ui.$.drawer;
      assertFalse(drawer.open);

      drawer.openDrawer();
      Polymer.dom.flush();
      await test_util.eventToPromise('cr-drawer-opened', drawer);

      // Validate that dialog is open and menu is shown so it will animate.
      assertTrue(drawer.open);
      assertTrue(!!ui.$$('cr-drawer os-settings-menu'));

      drawer.cancel();
      await test_util.eventToPromise('close', drawer);
      // Drawer is closed, but menu is still stamped so its contents remain
      // visible as the drawer slides out.
      assertTrue(!!ui.$$('cr-drawer os-settings-menu'));
    });

    test('app drawer closes when exiting narrow mode', async () => {
      const drawer = ui.$.drawer;
      const toolbar = ui.$$('os-toolbar');

      // Mimic narrow mode and open the drawer.
      toolbar.narrow = true;
      drawer.openDrawer();
      Polymer.dom.flush();
      await test_util.eventToPromise('cr-drawer-opened', drawer);

      toolbar.narrow = false;
      Polymer.dom.flush();
      await test_util.eventToPromise('close', drawer);
      assertFalse(drawer.open);
    });

    test('advanced UIs stay in sync', () => {
      const main = ui.$$('os-settings-main');
      const floatingMenu = ui.$$('#left os-settings-menu');
      assertTrue(!!main);
      assertTrue(!!floatingMenu);

      assertFalse(!!ui.$$('cr-drawer os-settings-menu'));
      assertFalse(ui.advancedOpenedInMain_);
      assertFalse(ui.advancedOpenedInMenu_);
      assertFalse(floatingMenu.advancedOpened);
      assertFalse(main.advancedToggleExpanded);

      main.advancedToggleExpanded = true;
      Polymer.dom.flush();

      assertFalse(!!ui.$$('cr-drawer os-settings-menu'));
      assertTrue(ui.advancedOpenedInMain_);
      assertTrue(ui.advancedOpenedInMenu_);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(main.advancedToggleExpanded);

      ui.$.drawerTemplate.if = true;
      Polymer.dom.flush();

      const drawerMenu = ui.$$('cr-drawer os-settings-menu');
      assertTrue(!!drawerMenu);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(drawerMenu.advancedOpened);

      // Collapse 'Advanced' in the menu.
      drawerMenu.$.advancedButton.click();
      Polymer.dom.flush();

      // Collapsing it in the menu should not collapse it in the main area.
      assertFalse(drawerMenu.advancedOpened);
      assertFalse(floatingMenu.advancedOpened);
      assertFalse(ui.advancedOpenedInMenu_);
      assertTrue(main.advancedToggleExpanded);
      assertTrue(ui.advancedOpenedInMain_);

      // Expand both 'Advanced's again.
      drawerMenu.$.advancedButton.click();

      // Collapse 'Advanced' in the main area.
      main.advancedToggleExpanded = false;
      Polymer.dom.flush();

      // Collapsing it in the main area should not collapse it in the menu.
      assertFalse(ui.advancedOpenedInMain_);
      assertTrue(drawerMenu.advancedOpened);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(ui.advancedOpenedInMenu_);
    });

    test('URL initiated search propagates to search box', () => {
      toolbar = /** @type {!OsToolbarElement} */ (ui.$$('os-toolbar'));
      const searchField =
          /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());
      assertEquals('', searchField.getSearchInput().value);

      const query = 'foo';
      settings.navigateTo(
          settings.routes.BASIC, new URLSearchParams(`search=${query}`));
      assertEquals(query, searchField.getSearchInput().value);
    });

    test('search box initiated search propagates to URL', () => {
      toolbar = /** @type {!OsToolbarElement} */ (ui.$$('os-toolbar'));
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

    test('whitespace only search query is ignored', () => {
      toolbar = /** @type {!OsToolbarElement} */ (ui.$$('os-toolbar'));
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

GEN('#endif  // defined(NDEBUG)');
