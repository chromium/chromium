// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsManageUsersSubpageElement, SettingsUserListElement} from 'chrome://os-settings/lazy_load.js';
import {CrIconButtonElement, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeUsersPrivate} from '../fake_users_private.js';

suite('<settings-manage-users-subpage>', () => {
  let page: SettingsManageUsersSubpageElement;

  const users: chrome.usersPrivate.User[] = [
    {
      email: 'test@gmail.com',
      displayEmail: 'test@gmail.com',
      name: 'test',
      isOwner: false,
      isChild: false,
    },
    {
      email: 'test1@gmail.com',
      displayEmail: 'test1@gmail.com',
      name: 'test1',
      isOwner: false,
      isChild: false,
    },
    {
      email: 'test2@gmail.com',
      displayEmail: 'test2@gmail.com',
      name: 'test2',
      isOwner: false,
      isChild: false,
    },
    {
      email: 'owner@gmail.com',
      displayEmail: 'owner@gmail.com',
      name: 'owner',
      isOwner: true,
      isChild: false,
    },
  ];

  function createUsersPage(): void {
    page = document.createElement('settings-manage-users-subpage');
    page.set('prefs', {
      cros: {
        accounts: {
          allowGuest: {
            value: false,
          },
        },
      },
    });

    document.body.appendChild(page);
    flush();
  }

  function removeManagedUsers(
      removeUserIcons: NodeListOf<CrIconButtonElement>,
      userList: SettingsUserListElement): void {
    for (const icon of removeUserIcons) {
      icon.click();
      flush();
      // Keep 'users_' array in 'settings-user-list' in sync with the users
      // array in our mocked usersPrivate API. With this, we refetch the users
      // after each removal which is consistent with how the page handles
      // removals.
      userList['setUsers_'](userList['usersPrivate_'].users);
      flush();
    }
  }

  setup(() => {
    createUsersPage();
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Focus add user button after all managed users are removed', () => {
    const userList = page.shadowRoot!.querySelector('settings-user-list');
    const addUserButton = page.shadowRoot!.querySelector('#add-user-button a');

    assertTrue(!!userList);
    // Setup and initialize fake users API.
    const fakeUsersPrivate = new FakeUsersPrivate();
    fakeUsersPrivate.setUsersForTesting(users);
    userList.set('usersPrivate_', fakeUsersPrivate);
    userList['setUsers_'](fakeUsersPrivate.users);
    flush();
    const removeUserIcons =
        userList.shadowRoot!.querySelectorAll('cr-icon-button');

    // Ensure that the add user button gains focus once all managed users have
    // been removed.
    removeManagedUsers(removeUserIcons, userList);
    assertEquals(addUserButton, getDeepActiveElement());
  });

  test('Deep link to Guest browsing', async () => {
    const params = new URLSearchParams();
    params.append(
        'settingId', settingMojom.Setting.kGuestBrowsingV2.toString());
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    const allowGuestBrowsing =
        page.shadowRoot!.querySelector('#allowGuestBrowsing');
    assertTrue(!!allowGuestBrowsing);
    const deepLinkElement =
        allowGuestBrowsing.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Guest browsing toggle should be focused for settingId=1104');
  });

  test('Deep link to Show Usernames And Photos At Signin', async () => {
    const params = new URLSearchParams();
    params.append(
        'settingId',
        settingMojom.Setting.kShowUsernamesAndPhotosAtSignInV2.toString());
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    const showUserNamesOnSignIn =
        page.shadowRoot!.querySelector('#showUserNamesOnSignIn');
    assertTrue(!!showUserNamesOnSignIn);
    const deepLinkElement =
        showUserNamesOnSignIn.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Show Usernames And Photos toggle should be focused for settingId=1105');
  });

  test('Deep link to Restrict Signin', async () => {
    const params = new URLSearchParams();
    params.append(
        'settingId', settingMojom.Setting.kRestrictSignInV2.toString());
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    const restrictSignIn = page.shadowRoot!.querySelector('#restrictSignIn');
    assertTrue(!!restrictSignIn);
    const deepLinkElement =
        restrictSignIn.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Restrict Signin toggle should be focused for settingId=1106');
  });
});
