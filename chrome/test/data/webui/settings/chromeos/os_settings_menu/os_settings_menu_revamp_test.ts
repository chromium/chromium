// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Runs tests for the left menu of CrOS Settings, assuming the
 * kOsSettingsRevampWayfinding feature flag is enabled.
 */

import 'chrome://os-settings/os_settings.js';

import {Account, AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createPageAvailabilityForTesting, OsSettingsMenuElement, OsSettingsMenuItemElement, routesMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

const {Section} = routesMojom;
type SectionName = keyof typeof Section;

interface MenuItemData {
  sectionName: SectionName;
  path: string;
}

suite('<os-settings-menu>', () => {
  let settingsMenu: OsSettingsMenuElement;
  let browserProxy: TestAccountManagerBrowserProxy;

  async function createMenu(): Promise<void> {
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    settingsMenu.advancedOpened = true;
    document.body.appendChild(settingsMenu);

    await browserProxy.whenCalled('getAccounts');
    await flushTasks();
  }

  function queryMenuItemByPath(path: string): OsSettingsMenuItemElement|null {
    return settingsMenu.shadowRoot!.querySelector<OsSettingsMenuItemElement>(
        `os-settings-menu-item[path="${path}"]`);
  }

  setup(() => {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  teardown(() => {
    settingsMenu.remove();
  });

  suite('Menu item visibility', () => {
    setup(async () => {
      await createMenu();
    });

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
      test(`${sectionName} menu item is visible if page is available`, () => {
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

  suite('Accounts menu item', () => {
    const fakeAccounts: Account[] = [
      {
        id: '123',
        accountType: 1,
        isDeviceAccount: true,
        isSignedIn: true,
        unmigrated: false,
        isManaged: false,
        fullName: 'Jon Snow',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'jon-snow-test@example.com',
        isAvailableInArc: true,
        organization: 'Stark',
      },
      {
        id: '456',
        accountType: 1,
        isDeviceAccount: false,
        isSignedIn: true,
        unmigrated: false,
        isManaged: false,
        fullName: 'Daenerys Targaryen',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'daenerys-targaryen-test@example.com',
        isAvailableInArc: true,
        organization: 'Targaryen',
      },
    ];

    suite('When there is only one account', () => {
      setup(() => {
        browserProxy.setAccountsForTesting(fakeAccounts.slice(0, 1));
      });

      test('Description should show account email', async () => {
        await createMenu();

        const accountsMenuItem =
            queryMenuItemByPath(`/${routesMojom.PEOPLE_SECTION_PATH}`);
        assertTrue(!!accountsMenuItem);
        assertEquals(fakeAccounts[0]!.email, accountsMenuItem.sublabel);
      });

      test('Description should update when an account is added', async () => {
        await createMenu();

        const accountsMenuItem =
            queryMenuItemByPath(`/${routesMojom.PEOPLE_SECTION_PATH}`);
        assertTrue(!!accountsMenuItem);
        assertEquals(fakeAccounts[0]!.email, accountsMenuItem.sublabel);

        // Update accounts to have 2 accounts
        browserProxy.setAccountsForTesting(fakeAccounts);
        webUIListenerCallback('accounts-changed');
        await flushTasks();

        assertEquals('2 accounts', accountsMenuItem.sublabel);
      });
    });

    suite('When there is more than one account', () => {
      setup(() => {
        browserProxy.setAccountsForTesting(fakeAccounts);
      });

      test('Description should show number of accounts', async () => {
        await createMenu();

        const accountsMenuItem =
            queryMenuItemByPath(`/${routesMojom.PEOPLE_SECTION_PATH}`);
        assertTrue(!!accountsMenuItem);
        assertEquals('2 accounts', accountsMenuItem.sublabel);
      });

      test('Description should update when an account is removed', async () => {
        await createMenu();

        const accountsMenuItem =
            queryMenuItemByPath(`/${routesMojom.PEOPLE_SECTION_PATH}`);
        assertTrue(!!accountsMenuItem);
        assertEquals('2 accounts', accountsMenuItem.sublabel);

        // Remove an account to leave only 1 account
        browserProxy.setAccountsForTesting(fakeAccounts.slice(0, 1));
        webUIListenerCallback('accounts-changed');
        await flushTasks();

        assertEquals(fakeAccounts[0]!.email, accountsMenuItem.sublabel);
      });
    });
  });
});
