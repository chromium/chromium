// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Runs tests for the left menu of CrOS Settings, assuming the
 * kOsSettingsRevampWayfinding feature flag is enabled.
 */

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, OsSettingsMenuElement, routesMojom} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

const {Section} = routesMojom;
type SectionName = keyof typeof Section;

interface MenuItemData {
  sectionName: SectionName;
  path: string;
}

suite('<os-settings-menu> menu item visibility', () => {
  let settingsMenu: OsSettingsMenuElement;

  function createMenu() {
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    settingsMenu.advancedOpened = true;
    document.body.appendChild(settingsMenu);
    flush();
  }

  setup(() => {
    createMenu();
  });

  teardown(() => {
    settingsMenu.remove();
  });

  function queryMenuItemByPath(path: string): HTMLElement|null {
    return settingsMenu.shadowRoot!.querySelector<HTMLElement>(
        `os-settings-menu-item[path="${path}"]`);
  }

  test('Advanced toggle and collapsible menu are not visible', () => {
    const advancedButton =
        settingsMenu.shadowRoot!.querySelector('#advancedButton');
    assertFalse(isVisible(advancedButton));

    const advancedCollapse =
        settingsMenu.shadowRoot!.querySelector('#advancedCollapse');
    assertFalse(isVisible(advancedCollapse));

    const advancedSubmenu =
        settingsMenu.shadowRoot!.querySelector('#advancedSubmenu');
    assertFalse(isVisible(advancedSubmenu));
  });

  test('About page menu item should always be visible', () => {
    const path = `/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`;
    const menuItem = queryMenuItemByPath(path);
    assertTrue(isVisible(menuItem));
  });

  const menuItemData: MenuItemData[] = [
    // Basic pages
    {
      sectionName: 'kNetwork',
      path: `/${routesMojom.NETWORK_SECTION_PATH}`,
    },
    {
      sectionName: 'kBluetooth',
      path: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
    },
    {
      sectionName: 'kMultiDevice',
      path: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
    },
    {
      sectionName: 'kPeople',
      path: `/${routesMojom.PEOPLE_SECTION_PATH}`,
    },
    {
      sectionName: 'kKerberos',
      path: `/${routesMojom.KERBEROS_SECTION_PATH}`,
    },
    {
      sectionName: 'kDevice',
      path: `/${routesMojom.DEVICE_SECTION_PATH}`,
    },
    {
      sectionName: 'kPersonalization',
      path: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
    },
    {
      sectionName: 'kPrivacyAndSecurity',
      path: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
    },
    {
      sectionName: 'kApps',
      path: `/${routesMojom.APPS_SECTION_PATH}`,
    },
    {
      sectionName: 'kAccessibility',
      path: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
    },
    {
      sectionName: 'kSystemPreferences',
      path: `/${routesMojom.SYSTEM_PREFERENCES_SECTION_PATH}`,
    },
  ];

  for (const {sectionName, path} of menuItemData) {
    test(
        `${sectionName} menu item is visible if page is available`, () => {
          // Make page available
          settingsMenu.pageAvailability = {
            ...settingsMenu.pageAvailability,
            [Section[sectionName]]: true,
          };
          flush();

          const menuItem = queryMenuItemByPath(path);
          assertTrue(isVisible(menuItem));
        });

    test(
        `${sectionName} menu item is not visible if page is unavailable`,
        () => {
          // Make page unavailable
          settingsMenu.pageAvailability = {
            ...settingsMenu.pageAvailability,
            [Section[sectionName]]: false,
          };
          flush();

          const menuItem = queryMenuItemByPath(path);
          assertFalse(isVisible(menuItem));
        });
  }
});
