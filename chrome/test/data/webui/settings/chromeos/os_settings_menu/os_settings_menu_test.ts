// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, OsSettingsMenuElement, Router, routes, routesMojom} from 'chrome://os-settings/os_settings.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

/** @fileoverview Runs tests for the OS settings menu. */
suite('<os-settings-menu>', () => {
  let settingsMenu: OsSettingsMenuElement;

  setup(() => {
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

  const {Section} = routesMojom;
  type PageName = keyof typeof Section;

  setup(() => {
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(() => {
    settingsMenu.remove();
  });

  function queryMenuItem(pageName: PageName): HTMLElement|null {
    return settingsMenu.shadowRoot!.querySelector<HTMLElement>(
        `a.item[data-section="${Section[pageName]}"]`);
  }

  const pageNames: PageName[] = [
    // Basic pages
    'kNetwork',
    'kBluetooth',
    'kMultiDevice',
    'kKerberos',
    'kPeople',
    'kDevice',
    'kPersonalization',
    'kSearchAndAssistant',
    'kPrivacyAndSecurity',
    'kApps',
    'kAccessibility',
    // Advanced section pages
    'kDateAndTime',
    'kLanguagesAndInput',
    'kFiles',
    'kPrinting',
    'kCrostini',
    'kReset',
  ];
  for (const pageName of pageNames) {
    test(`${pageName} menu item is controlled by pageAvailability`, () => {
      // Make page available
      settingsMenu.pageAvailability = {
        ...settingsMenu.pageAvailability,
        [Section[pageName]]: true,
      };
      flush();

      let menuItem = queryMenuItem(pageName);
      assertTrue(!!menuItem, `Menu item for ${pageName} should be stamped.`);

      // Make page unavailable
      settingsMenu.pageAvailability = {
        ...settingsMenu.pageAvailability,
        [Section[pageName]]: false,
      };
      flush();

      menuItem = queryMenuItem(pageName);
      assertNull(menuItem, `Menu item for ${pageName} should not be stamped.`);
    });
  }
});
