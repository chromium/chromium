// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrDrawerElement, CrToolbarElement, CrToolbarSearchFieldElement, SettingsUiElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for the Settings layout. */
suite('SettingsUIToolbarAndDrawer', function() {
  let ui: SettingsUiElement;
  let toolbar: CrToolbarElement;
  let drawer: CrDrawerElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    ui = document.createElement('settings-ui');
    document.body.appendChild(ui);
    return CrSettingsPrefs.initialized.then(() => {
      flush();
      toolbar = ui.$.toolbar;
      drawer = ui.$.drawer;
    });
  });

  test('showing menu in toolbar is dependent on narrow mode', async function() {
    assertTrue(!!toolbar);
    toolbar.narrow = true;
    await toolbar.updateComplete;
    assertTrue(toolbar.showMenu);

    toolbar.narrow = false;
    await toolbar.updateComplete;
    assertFalse(toolbar.showMenu);
  });

  test('app drawer', async () => {
    assertEquals(null, ui.shadowRoot!.querySelector('cr-drawer settings-menu'));
    assertFalse(!!drawer.open);

    const drawerOpened = eventToPromise('cr-drawer-opened', drawer);
    drawer.openDrawer();
    await drawerOpened;

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(drawer.open);
    assertTrue(!!ui.shadowRoot!.querySelector('cr-drawer settings-menu'));

    const drawerClosed = eventToPromise('close', drawer);
    drawer.cancel();
    await drawerClosed;

    // Drawer is closed, but menu is still stamped so
    // its contents remain visible as the drawer slides
    // out.
    assertTrue(!!ui.shadowRoot!.querySelector('cr-drawer settings-menu'));
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

suite('SettingsUISearch', function() {
  let ui: SettingsUiElement;
  let toolbar: CrToolbarElement;
  let searchField: CrToolbarSearchFieldElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    ui = document.createElement('settings-ui');
    document.body.appendChild(ui);
    return CrSettingsPrefs.initialized.then(() => {
      flush();
      toolbar = ui.$.toolbar;
      searchField = toolbar.getSearchField();
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
        routes.BASIC, /* dynamicParams */ undefined,
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
    await toolbar.updateComplete;
    ui.$.leftMenu.focusFirstItem();
    assertEquals(ui.$.leftMenu, ui.shadowRoot!.activeElement);

    // Switch to narrow mode and test that focus moves to menu button.
    toolbar.narrow = true;
    await eventToPromise('focusin', toolbar);
    assertTrue(toolbar.isMenuFocused());

    // Switch back to non-narrow mode and test that focus moves to left menu.
    toolbar.narrow = false;
    await toolbar.updateComplete;
    assertEquals(ui.$.leftMenu, ui.shadowRoot!.activeElement);
  });
});
