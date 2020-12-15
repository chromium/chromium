// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';
// clang-format on

/** @fileoverview Suite of tests for the Settings layout. */
suite('SettingsUIToolbarAndDrawer', function() {
  /** @type {!SettingsUiElement} */
  let ui;

  /** @type {!CrToolbarElement} */
  let toolbar;

  /** @type {!CrDrawerElement} */
  let drawer;

  setup(function() {
    document.body.innerHTML = '';
    ui = /** @type {!SettingsUiElement} */ (
        document.createElement('settings-ui'));
    document.body.appendChild(ui);
    return CrSettingsPrefs.initialized.then(() => {
      flush();
      toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
      drawer = /** @type {!CrDrawerElement} */ (ui.$$('#drawer'));
    });
  });

  test('showing menu in toolbar is dependent on narrow mode', function() {
    assertTrue(!!toolbar);
    toolbar.narrow = true;
    assertTrue(toolbar.showMenu);

    toolbar.narrow = false;
    assertFalse(toolbar.showMenu);
  });

  test('app drawer', async () => {
    assertEquals(null, ui.$$('cr-drawer settings-menu'));
    assertFalse(!!drawer.open);

    const drawerOpened = eventToPromise('cr-drawer-opened', drawer);
    drawer.openDrawer();
    flush();

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(drawer.open);
    assertTrue(!!ui.$$('cr-drawer settings-menu'));

    await drawerOpened;
    const drawerClosed = eventToPromise('close', drawer);
    drawer.cancel();

    await drawerClosed;
    // Drawer is closed, but menu is still stamped so
    // its contents remain visible as the drawer slides
    // out.
    assertTrue(!!ui.$$('cr-drawer settings-menu'));
  });

  test('app drawer closes when exiting narrow mode', async () => {
    // Mimic narrow mode and open the drawer
    toolbar.narrow = true;
    drawer.openDrawer();
    flush();
    await eventToPromise('cr-drawer-opened', drawer);

    toolbar.narrow = false;
    flush();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
  });
});

suite('SettingsUIAdvanced', function() {
  /** @type {!SettingsUiElement} */
  let ui;

  setup(function() {
    document.body.innerHTML = '';
    ui = /** @type {!SettingsUiElement} */ (
        document.createElement('settings-ui'));
    document.body.appendChild(ui);
    return CrSettingsPrefs.initialized.then(() => flush());
  });

  test('advanced UIs stay in sync', function() {
    const main = ui.$$('settings-main');
    const floatingMenu =
        /** @type {!SettingsMenuElement} */ (ui.$$('#left settings-menu'));
    assertTrue(!!main);
    assertTrue(!!floatingMenu);

    assertFalse(!!ui.$$('cr-drawer settings-menu'));
    assertFalse(ui.getAdvancedOpenedInMainForTest());
    assertFalse(ui.getAdvancedOpenedInMenuForTest());
    assertFalse(floatingMenu.advancedOpened);
    assertFalse(main.advancedToggleExpanded);

    main.advancedToggleExpanded = true;
    flush();

    assertFalse(!!ui.$$('cr-drawer settings-menu'));
    assertTrue(ui.getAdvancedOpenedInMainForTest());
    assertTrue(ui.getAdvancedOpenedInMenuForTest());
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(main.advancedToggleExpanded);

    ui.$$('#drawerTemplate').if = true;
    flush();

    const drawerMenu = ui.$$('cr-drawer settings-menu');
    assertTrue(!!drawerMenu);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(drawerMenu.advancedOpened);

    // Collapse 'Advanced' in the menu
    drawerMenu.$$('#advancedButton').click();
    flush();

    // Collapsing it in the menu should not collapse it in the main area
    assertFalse(drawerMenu.advancedOpened);
    assertFalse(floatingMenu.advancedOpened);
    assertFalse(ui.getAdvancedOpenedInMenuForTest());
    assertTrue(main.advancedToggleExpanded);
    assertTrue(ui.getAdvancedOpenedInMainForTest());

    // Expand both 'Advanced's again
    drawerMenu.$$('#advancedButton').click();

    // Collapse 'Advanced' in the main area
    main.advancedToggleExpanded = false;
    flush();

    // Collapsing it in the main area should not collapse it in the menu
    assertFalse(ui.getAdvancedOpenedInMainForTest());
    assertTrue(drawerMenu.advancedOpened);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(ui.getAdvancedOpenedInMenuForTest());
  });
});

suite('SettingsUISearch', function() {
  /** @type {!SettingsUiElement} */
  let ui;

  /** @type {!CrToolbarElement} */
  let toolbar;

  /** @type {!CrToolbarSearchFieldElement} */
  let searchField;

  setup(function() {
    document.body.innerHTML = '';
    ui = /** @type {!SettingsUiElement} */ (
        document.createElement('settings-ui'));
    document.body.appendChild(ui);
    return CrSettingsPrefs.initialized.then(() => {
      flush();
      toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
      searchField =
          /** @type {!CrToolbarSearchFieldElement} */ (
              toolbar.getSearchField());
    });
  });

  test('URL initiated search propagates to search box', function() {
    assertEquals('', searchField.getSearchInput().value);

    const query = 'foo';
    Router.getInstance().navigateTo(
        routes.BASIC, new URLSearchParams(`search=${query}`));
    assertEquals(query, searchField.getSearchInput().value);
  });

  test('search box initiated search propagates to URL', function() {
    Router.getInstance().navigateTo(
        routes.BASIC, /* dynamicParams */ null,
        /* removeSearch */ true);
    assertEquals('', searchField.getSearchInput().value);
    assertFalse(Router.getInstance().getQueryParameters().has('search'));

    let value = 'GOOG';
    searchField.setValue(value);
    assertEquals(
        value, Router.getInstance().getQueryParameters().get('search'));

    // Test that search queries are properly URL encoded.
    value = '+++';
    searchField.setValue(value);
    assertEquals(
        value, Router.getInstance().getQueryParameters().get('search'));
  });

  test('whitespace only search query is ignored', function() {
    searchField.setValue('    ');
    let urlParams = Router.getInstance().getQueryParameters();
    assertFalse(urlParams.has('search'));

    searchField.setValue('   foo');
    urlParams = Router.getInstance().getQueryParameters();
    assertEquals('foo', urlParams.get('search'));

    searchField.setValue('   foo ');
    urlParams = Router.getInstance().getQueryParameters();
    assertEquals('foo ', urlParams.get('search'));

    searchField.setValue('   ');
    urlParams = Router.getInstance().getQueryParameters();
    assertFalse(urlParams.has('search'));
  });

  test('MaintainsFocusOnMenus', async () => {
    // Start in non-narrow mode with focus in the left menu.
    toolbar.narrow = false;
    ui.$$('#leftMenu').focusFirstItem();
    assertEquals(ui.$$('#leftMenu'), ui.shadowRoot.activeElement);

    // Switch to narrow mode and test that focus moves to menu button.
    toolbar.narrow = true;
    flush();
    await new Promise(resolve => requestAnimationFrame(resolve));
    assertTrue(ui.$.toolbar.isMenuFocused());

    // Switch back to non-narrow mode and test that focus moves to left menu.
    toolbar.narrow = false;
    flush();
    assertEquals(ui.$$('#leftMenu'), ui.shadowRoot.activeElement);
  });
});
