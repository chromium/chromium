// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, OsSettingsMenuElement, OsSettingsRoutes, Route, Router, routes} from 'chrome://os-settings/os_settings.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

/** @fileoverview Runs tests for the OS settings menu. */

function setupRouter() {
  const basicRoute = new Route('/');
  const bluetoothRoute = basicRoute.createSection('/bluetooth', 'bluetooth');
  const advancedRoute = new Route('/advanced');
  const resetRoute = advancedRoute.createSection('/osReset', 'osReset');

  const testRoutes = {
    BASIC: basicRoute,
    ABOUT: new Route('/about'),
    ADVANCED: advancedRoute,
    BLUETOOTH: bluetoothRoute,
    OS_RESET: resetRoute,
  };

  Router.resetInstanceForTesting(new Router(testRoutes as OsSettingsRoutes));

  routes.OS_RESET = testRoutes.OS_RESET;
  routes.BLUETOOTH = testRoutes.BLUETOOTH;
  routes.ADVANCED = testRoutes.ADVANCED;
  routes.BASIC = testRoutes.BASIC;
}

suite('<os-settings-menu>', () => {
  let settingsMenu: OsSettingsMenuElement;

  setup(() => {
    setupRouter();
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(settingsMenu);
  });

  teardown(() => {
    settingsMenu.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('advancedOpenedBinding', () => {
    assertFalse(settingsMenu.advancedOpened);
    settingsMenu.advancedOpened = true;
    flush();
    assertTrue(settingsMenu.$.advancedSubmenu.opened);

    settingsMenu.advancedOpened = false;
    flush();
    assertFalse(settingsMenu.$.advancedSubmenu.opened);
  });

  test('tapAdvanced', () => {
    assertFalse(settingsMenu.advancedOpened);

    const advancedToggle =
        settingsMenu.shadowRoot!.querySelector<HTMLButtonElement>(
            '#advancedButton');
    assertTrue(!!advancedToggle);

    advancedToggle.click();
    flush();
    assertTrue(settingsMenu.$.advancedSubmenu.opened);

    advancedToggle.click();
    flush();
    assertFalse(settingsMenu.$.advancedSubmenu.opened);
  });

  test('upAndDownIcons', () => {
    // There should be different icons for a top level menu being open
    // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
    const ironIconElement =
        settingsMenu.shadowRoot!.querySelector<IronIconElement>(
            '#advancedButton iron-icon');
    assertTrue(!!ironIconElement);

    settingsMenu.advancedOpened = true;
    flush();
    const openIcon = ironIconElement.icon;
    assertTrue(!!openIcon);

    settingsMenu.advancedOpened = false;
    flush();
    assertNotEquals(openIcon, ironIconElement.icon);
  });

  test('Advanced menu expands on navigating to an advanced setting', () => {
    assertFalse(settingsMenu.advancedOpened);
    Router.getInstance().navigateTo(routes.OS_RESET);
    assertFalse(settingsMenu.advancedOpened);

    // If there are search params and the current route is a descendant of
    // the Advanced route, then ensure that the advanced menu expands.
    const params = new URLSearchParams('search=test');
    Router.getInstance().navigateTo(routes.OS_RESET, params);
    flush();
    assertTrue(settingsMenu.advancedOpened);
  });
});

suite('<os-settings-menu> reset', () => {
  let settingsMenu: OsSettingsMenuElement;

  setup(() => {
    setupRouter();
    Router.getInstance().navigateTo(routes.OS_RESET);
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(() => {
    settingsMenu.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('openResetSection', () => {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected as string).pathname;
    assertEquals('/osReset', path);
  });

  test('navigateToAnotherSection', () => {
    const selector = settingsMenu.$.subMenu;
    let path = new window.URL(selector.selected as string).pathname;
    assertEquals('/osReset', path);

    Router.getInstance().navigateTo(routes.BLUETOOTH);
    flush();

    path = new window.URL(selector.selected as string).pathname;
    assertEquals('/bluetooth', path);
  });

  test('navigateToBasic', () => {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected as string).pathname;
    assertEquals('/osReset', path);

    Router.getInstance().navigateTo(routes.BASIC);
    flush();

    // BASIC has no sub page selected.
    assertEquals('', selector.selected);
  });
});

suite('<os-settings-menu> page availability', () => {
  let settingsMenu: OsSettingsMenuElement;

  setup(() => {
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(() => {
    settingsMenu.remove();
  });

  function queryMenuItemByPageName(pageName: string): HTMLElement|null {
    return settingsMenu.shadowRoot!.querySelector<HTMLElement>(
        `a.item[data-page-name='${pageName}']`);
  }

  const pages = [
    // Basic pages
    'internet',
    'bluetooth',
    'multidevice',
    'kerberos',
    'osPeople',
    'device',
    'personalization',
    'osSearch',
    'osPrivacy',
    'apps',
    'osAccessibility',
    // Advanced section pages
    'dateTime',
    'osLanguages',
    'files',
    'osPrinting',
    'crostini',
    'osReset',
  ];
  for (const pageName of pages) {
    test(`${pageName} menu item is controlled by pageAvailability`, () => {
      // Make page available
      settingsMenu.pageAvailability = {
        ...settingsMenu.pageAvailability,
        [pageName]: true,
      };
      flush();

      let menuItem = queryMenuItemByPageName(pageName);
      assertTrue(!!menuItem, `Menu item for ${pageName} should be stamped.`);

      // Make page unavailable
      settingsMenu.pageAvailability = {
        ...settingsMenu.pageAvailability,
        [pageName]: false,
      };
      flush();

      menuItem = queryMenuItemByPageName(pageName);
      assertNull(menuItem, `Menu item for ${pageName} should not be stamped.`);
    });
  }
});
