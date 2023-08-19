// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, IronCollapseElement, IronSelectorElement, OsSettingsMenuElement, Router, routes, routesMojom} from 'chrome://os-settings/os_settings.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

/** @fileoverview Runs tests for the OS settings menu. */
suite('<os-settings-menu>', () => {
  let settingsMenu: OsSettingsMenuElement;

  setup(() => {
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(() => {
    settingsMenu.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('advancedOpenedBinding', () => {
    assertFalse(settingsMenu.advancedOpened);
    settingsMenu.advancedOpened = true;
    flush();

    const advancedCollapse =
        settingsMenu.shadowRoot!.querySelector<IronCollapseElement>(
            '#advancedCollapse');
    assertTrue(!!advancedCollapse);
    assertTrue(advancedCollapse.opened);

    settingsMenu.advancedOpened = false;
    flush();
    assertFalse(advancedCollapse.opened);
  });

  test('tapAdvanced', () => {
    assertFalse(settingsMenu.advancedOpened);

    const advancedToggle =
        settingsMenu.shadowRoot!.querySelector<HTMLButtonElement>(
            '#advancedButton');
    assertTrue(!!advancedToggle);

    advancedToggle.click();
    flush();

    const advancedCollapse =
        settingsMenu.shadowRoot!.querySelector<IronCollapseElement>(
            '#advancedCollapse');
    assertTrue(!!advancedCollapse);
    assertTrue(advancedCollapse.opened);

    advancedToggle.click();
    flush();
    assertFalse(advancedCollapse.opened);
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
    const submenu = settingsMenu.shadowRoot!.querySelector<IronSelectorElement>(
        '#advancedSubmenu');
    assertTrue(!!submenu);
    const path = new window.URL(submenu.selected as string).pathname;
    assertEquals('/osReset', path);
  });

  test('navigateToAnotherSection', () => {
    const submenu = settingsMenu.shadowRoot!.querySelector<IronSelectorElement>(
        '#advancedSubmenu');
    assertTrue(!!submenu);
    let path = new window.URL(submenu.selected as string).pathname;
    assertEquals('/osReset', path);

    Router.getInstance().navigateTo(routes.BLUETOOTH);
    flush();

    path = new window.URL(submenu.selected as string).pathname;
    assertEquals('/bluetooth', path);
  });

  test('navigateToBasic', () => {
    const submenu = settingsMenu.shadowRoot!.querySelector<IronSelectorElement>(
        '#advancedSubmenu');
    assertTrue(!!submenu);
    const path = new window.URL(submenu.selected as string).pathname;
    assertEquals('/osReset', path);

    Router.getInstance().navigateTo(routes.BASIC);
    flush();

    // BASIC has no sub page selected.
    assertEquals('', submenu.selected);
  });
});

suite('<os-settings-menu> menu item visibility', () => {
  let settingsMenu: OsSettingsMenuElement;

  const {Section} = routesMojom;
  type SectionName = keyof typeof Section;

  setup(() => {
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    settingsMenu.advancedOpened = true;
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(() => {
    settingsMenu.remove();
  });

  function queryMenuItemByHref(href: string): HTMLElement|null {
    return settingsMenu.shadowRoot!.querySelector<HTMLElement>(
        `a.item[href="${href}"]`);
  }

  test('About page menu item should always be visible', () => {
    const href = `/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`;
    const menuItem = queryMenuItemByHref(href);
    assertTrue(isVisible(menuItem));
  });

  interface MenuItemData {
    sectionName: SectionName;
    href: string;
  }

  const menuItemData: MenuItemData[] = [
    // Basic pages
    {
      sectionName: 'kNetwork',
      href: `/${routesMojom.NETWORK_SECTION_PATH}`,
    },
    {
      sectionName: 'kBluetooth',
      href: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
    },
    {
      sectionName: 'kMultiDevice',
      href: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
    },
    {
      sectionName: 'kPeople',
      href: `/${routesMojom.PEOPLE_SECTION_PATH}`,
    },
    {
      sectionName: 'kKerberos',
      href: `/${routesMojom.KERBEROS_SECTION_PATH}`,
    },
    {
      sectionName: 'kDevice',
      href: `/${routesMojom.DEVICE_SECTION_PATH}`,
    },
    {
      sectionName: 'kPersonalization',
      href: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
    },
    {
      sectionName: 'kSearchAndAssistant',
      href: `/${routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH}`,
    },
    {
      sectionName: 'kPrivacyAndSecurity',
      href: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
    },
    {
      sectionName: 'kApps',
      href: `/${routesMojom.APPS_SECTION_PATH}`,
    },
    {
      sectionName: 'kAccessibility',
      href: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
    },

    // Advanced pages
    {
      sectionName: 'kDateAndTime',
      href: `/${routesMojom.DATE_AND_TIME_SECTION_PATH}`,
    },
    {
      sectionName: 'kLanguagesAndInput',
      href: `/${routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH}`,
    },
    {
      sectionName: 'kFiles',
      href: `/${routesMojom.FILES_SECTION_PATH}`,
    },
    {
      sectionName: 'kPrinting',
      href: `/${routesMojom.PRINTING_SECTION_PATH}`,
    },
    {
      sectionName: 'kCrostini',
      href: `/${routesMojom.CROSTINI_SECTION_PATH}`,
    },
    {
      sectionName: 'kReset',
      href: `/${routesMojom.RESET_SECTION_PATH}`,
    },
  ];

  for (const {sectionName, href} of menuItemData) {
    test(
        `${sectionName} menu item is visible if corresponding page is available`,
        () => {
          // Make page available
          settingsMenu.pageAvailability = {
            ...settingsMenu.pageAvailability,
            [Section[sectionName]]: true,
          };
          flush();

          let menuItem = queryMenuItemByHref(href);
          assertTrue(
              isVisible(menuItem),
              `Menu item for ${sectionName} should be visible.`);

          // Make page unavailable
          settingsMenu.pageAvailability = {
            ...settingsMenu.pageAvailability,
            [Section[sectionName]]: false,
          };
          flush();

          menuItem = queryMenuItemByHref(href);
          assertFalse(
              isVisible(menuItem),
              `Menu item for ${sectionName} should not be visible.`);
        });
  }
});
