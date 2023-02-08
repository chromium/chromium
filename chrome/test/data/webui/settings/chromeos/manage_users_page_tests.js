// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeUsersPrivate} from './fake_users_private.js';

let page = null;

/** {!Array<!chrome.usersPrivate.User>} */
const users = [
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

function createUsersPage() {
  PolymerTest.clearBody();
  page = document.createElement('settings-manage-users-page');
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

function removeUsersPage() {
  page.remove();
  page = null;
}

/**
 * @param {HTMLElement} removeUserIcons
 * @param {HTMLElement} userList
 */
function removeManagedUsers(removeUserIcons, userList) {
  for (const icon of removeUserIcons) {
    icon.click();
    flush();
    // Keep 'users_' array in 'settings-user-list' in sync with the users array
    // in our mocked usersPrivate API. With this, we refetch the users after
    // each removal which is consistent with how the page handles removals.
    userList.setUsers_(userList.usersPrivate_.users);
    flush();
  }
}

suite('ManageUsersPage', () => {
  setup(function() {
    createUsersPage();
  });

  teardown(function() {
    removeUsersPage();
    Router.getInstance().resetRouteForTesting();
  });

  test('Focus add user button after all managed users are removed', () => {
    const userList = page.shadowRoot.querySelector('settings-user-list');
    const addUserButton = page.shadowRoot.querySelector('#add-user-button a');

    // Setup and initialize fake users API.
    userList.usersPrivate_ = new FakeUsersPrivate();
    userList.usersPrivate_.users = users;
    userList.setUsers_(users);
    flush();
    const removeUserIcons =
        userList.shadowRoot.querySelectorAll('cr-icon-button');

    // Ensure that the add user button gains focus once all managed users have
    // been removed.
    removeManagedUsers(removeUserIcons, userList);
    assertEquals(getDeepActiveElement(), addUserButton);
  });

  test('Deep link to Guest browsing', async () => {
    const settingId = '1104';

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    const deepLinkElement = page.shadowRoot.querySelector('#allowGuestBrowsing')
                                .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Guest browsing toggle should be focused for settingId=' + settingId);
  });

  test('Deep link to Show Usernames And Photos At Signin', async () => {
    const settingId = '1105';

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    const deepLinkElement =
        page.shadowRoot.querySelector('#showUserNamesOnSignIn')
            .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Show Usernames And Photos toggle should be focused for settingId=' +
            settingId);
  });

  test('Deep link to Restrict Signin', async () => {
    const settingId = '1106';

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    const deepLinkElement = page.shadowRoot.querySelector('#restrictSignIn')
                                .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Restrict Signin toggle should be focused for settingId=' + settingId);
  });
});
