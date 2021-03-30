// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeUsersPrivate} from './fake_users_private.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

let page = null;

/** {!Array<!chrome.usersPrivate.User>} */
const users = [
  {
    email: 'test@gmail.com',
    displayEmail: 'test@gmail.com',
    name: 'test',
    isOwner: false,
    isSupervised: false,
    isChild: false
  },
  {
    email: 'test1@gmail.com',
    displayEmail: 'test1@gmail.com',
    name: 'test1',
    isOwner: false,
    isSupervised: false,
    isChild: false
  },
  {
    email: 'test2@gmail.com',
    displayEmail: 'test2@gmail.com',
    name: 'test2',
    isOwner: false,
    isSupervised: false,
    isChild: false
  },
  {
    email: 'owner@gmail.com',
    displayEmail: 'owner@gmail.com',
    name: 'owner',
    isOwner: true,
    isSupervised: false,
    isChild: false
  }
];

function createUsersPage() {
  PolymerTest.clearBody();
  page = document.createElement('settings-users-page');
  page.set('prefs', {
    cros: {
      accounts: {
        allowGuest: {
          value: false
          }
        }
      }
  });

  document.body.appendChild(page);
  Polymer.dom.flush();
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
    Polymer.dom.flush();
    // Keep 'users_' array in 'settings-user-list' in sync with the users array
    // in our mocked usersPrivate API. With this, we refetch the users after
    // each removal which is consistent with how the page handles removals.
    userList.setUsers_(userList.usersPrivate_.users);
    Polymer.dom.flush();
  }
}

suite('UserPage', () => {
  setup(function() {
    createUsersPage();
  });

  teardown(function() {
    removeUsersPage();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Focus add user button after all managed users are removed', () => {
    const userList = page.$$('settings-user-list');
    const addUserButton = page.$$('#add-user-button a');

    // Setup and initialize fake users API.
    userList.usersPrivate_ = new settings.FakeUsersPrivate();
    userList.usersPrivate_.users = users;
    userList.setUsers_(users);
    Polymer.dom.flush();
    const removeUserIcons =
        userList.shadowRoot.querySelectorAll('cr-icon-button');

    // Ensure that the add user button gains focus once all managed users have
    // been removed.
    removeManagedUsers(removeUserIcons, userList);
    assertEquals(getDeepActiveElement(), addUserButton);
  });

  test('Deep link to Guest browsing', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    const settingId = page.isAccountManagementFlowsV2Enabled_ ? '1104' : '305';

    const params = new URLSearchParams;
    params.append('settingId', settingId);
    settings.Router.getInstance().navigateTo(settings.routes.ACCOUNTS, params);

    const deepLinkElement = page.$$('#allowGuestBrowsing').$$('cr-toggle');
    assertTrue(!!deepLinkElement);
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Guest browsing toggle should be focused for settingId=' + settingId);
  });

  test('Deep link to Show Usernames And Photos At Signin', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    const settingId = page.isAccountManagementFlowsV2Enabled_ ? '1105' : '306';

    const params = new URLSearchParams;
    params.append('settingId', settingId);
    settings.Router.getInstance().navigateTo(settings.routes.ACCOUNTS, params);

    const deepLinkElement = page.$$('#showUserNamesOnSignIn').$$('cr-toggle');
    assertTrue(!!deepLinkElement);
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Show Usernames And Photos toggle should be focused for settingId=' +
            settingId);
  });

  test('Deep link to Restrict Signin', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    const settingId = page.isAccountManagementFlowsV2Enabled_ ? '1106' : '307';

    const params = new URLSearchParams;
    params.append('settingId', settingId);
    settings.Router.getInstance().navigateTo(settings.routes.ACCOUNTS, params);

    const deepLinkElement = page.$$('#restrictSignIn').$$('cr-toggle');
    assertTrue(!!deepLinkElement);
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Restrict Signin toggle should be focused for settingId=' + settingId);
  });
});
