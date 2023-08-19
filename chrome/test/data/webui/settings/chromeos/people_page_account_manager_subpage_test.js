// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {ParentalControlsBrowserProxyImpl, Router, routes, setUserActionRecorderForTesting, userActionRecorderMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {FakeUserActionRecorder} from './fake_user_action_recorder.js';

/** @implements {AccountManagerBrowserProxy} */
class TestAccountManagerBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAccounts',
      'addAccount',
      'reauthenticateAccount',
      'removeAccount',
      'changeArcAvailability',
    ]);
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');

    return Promise.resolve([
      {
        id: '123',
        accountType: 1,
        isDeviceAccount: true,
        isSignedIn: true,
        unmigrated: false,
        fullName: 'Device Account',
        email: 'admin@domain.com',
        pic: 'data:image/png;base64,abc123',
        isAvailableInArc: true,
        organization: 'Family Link',
      },
      {
        id: '456',
        accountType: 1,
        isDeviceAccount: false,
        isSignedIn: true,
        unmigrated: false,
        isManaged: true,
        fullName: 'Secondary Account 1',
        email: 'user1@example.com',
        pic: '',
        isAvailableInArc: true,
      },
      {
        id: '789',
        accountType: 1,
        isDeviceAccount: false,
        isSignedIn: false,
        unmigrated: false,
        fullName: 'Secondary Account 2',
        email: 'user2@example.com',
        pic: '',
        isAvailableInArc: true,
      },
      {
        id: '1010',
        accountType: 1,
        isDeviceAccount: false,
        isSignedIn: false,
        unmigrated: true,
        fullName: 'Secondary Account 3',
        email: 'user3@example.com',
        pic: '',
        isAvailableInArc: false,
      },
    ]);
  }

  /** @override */
  addAccount() {
    this.methodCalled('addAccount');
  }

  /** @override */
  reauthenticateAccount(account_email) {
    this.methodCalled('reauthenticateAccount', account_email);
  }

  /** @override */
  removeAccount(account) {
    this.methodCalled('removeAccount', account);
  }

  /** @override */
  changeArcAvailability(account, isAvailableInArc) {
    this.methodCalled('changeArcAvailability', [account, isAvailableInArc]);
  }
}

/** @implements {AccountManagerBrowserProxy} */
class TestAccountManagerBrowserProxyForUnmanagedAccounts extends
    TestAccountManagerBrowserProxy {
  constructor() {
    super([
      'getAccounts',
      'addAccount',
      'reauthenticateAccount',
      'removeAccount',
    ]);
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');

    return new Promise((resolve) => {
      resolve([
        {
          id: '123',
          accountType: 1,
          isDeviceAccount: true,
          isSignedIn: true,
          unmigrated: false,
          fullName: 'Device Account',
          email: 'admin@domain.com',
          pic: 'data:image/png;base64,abc123',
        },
      ]);
    });
  }
}

class TestParentalControlsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showAddSupervisionDialog',
      'launchFamilyLinkSettings',
    ]);
  }

  /** @override */
  launchFamilyLinkSettings() {
    this.methodCalled('launchFamilyLinkSettings');
  }

  /** @override */
  showAddSupervisionDialog() {
    this.methodCalled('showAddSupervisionDialog');
  }
}

suite('AccountManagerTests', function() {
  let browserProxy = null;
  let accountManager = null;
  let accountList = null;

  /** @type {?userActionRecorderMojom.UserActionRecorderInterface} */
  let userActionRecorder = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({isDeviceAccountManaged: true});
  });

  setup(function() {
    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();

    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);
    accountList = accountManager.shadowRoot.querySelector('#account-list');
    assertTrue(!!accountList);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(function() {
    accountManager.remove();
    Router.getInstance().resetRouteForTesting();
    setUserActionRecorderForTesting(null);
  });

  test('AccountListIsPopulatedAtStartup', async function() {
    await browserProxy.whenCalled('getAccounts');
    flush();
    // 1 device account + 3 secondary accounts were added in
    // |getAccounts()| mock above.
    assertEquals(3, accountList.items.length);
  });

  test('AddAccount', function() {
    assertFalse(accountManager.shadowRoot.querySelector('#add-account-button')
                    .disabled);
    assertTrue(
        accountManager.shadowRoot.querySelector(
            '.secondary-accounts-disabled-tooltip') === null);
    accountManager.shadowRoot.querySelector('#add-account-button').click();
    assertEquals(1, browserProxy.getCallCount('addAccount'));
  });

  test('ReauthenticateAccount', async function() {
    await browserProxy.whenCalled('getAccounts');
    flush();
    accountManager.root.querySelectorAll('.reauth-button')[0].click();
    assertEquals(1, browserProxy.getCallCount('reauthenticateAccount'));
    const accountEmail = await browserProxy.whenCalled('reauthenticateAccount');
    assertEquals('user2@example.com', accountEmail);
  });

  test('UnauthenticatedAccountLabel', async function() {
    await browserProxy.whenCalled('getAccounts');
    flush();
    assertEquals(
        loadTimeData.getString('accountManagerReauthenticationLabel'),
        accountManager.root.querySelectorAll('.reauth-button')[0]
            .textContent.trim());
  });

  test('UnmigratedAccountLabel', async function() {
    await browserProxy.whenCalled('getAccounts');
    flush();
    assertEquals(
        loadTimeData.getString('accountManagerMigrationLabel'),
        accountManager.root.querySelectorAll('.reauth-button')[1]
            .textContent.trim());
  });

  test('RemoveAccount', async function() {
    await browserProxy.whenCalled('getAccounts');
    flush();
    // Click on 'More Actions' for the second account (First one (index 0)
    // to have the hamburger menu).
    accountManager.root.querySelectorAll('cr-icon-button')[0].click();
    // Click on 'Remove account' (the first button in the menu).
    accountManager.shadowRoot.querySelector('cr-action-menu')
        .querySelectorAll('button')[0]
        .click();

    if (loadTimeData.getBoolean('lacrosEnabled')) {
      const confirmationDialog =
          accountManager.shadowRoot.querySelector('#removeConfirmationDialog');
      assertTrue(!!confirmationDialog);
      confirmationDialog.querySelector('#removeConfirmationButton').click();
    }

    const account = await browserProxy.whenCalled('removeAccount');
    assertEquals('456', account.id);
    // Add account button should be in focus now.
    assertEquals(
        accountManager.shadowRoot.querySelector('#add-account-button'),
        accountManager.root.activeElement);
  });

  test('Deep link to remove account button', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '301');
    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER, params);

    const deepLinkElement =
        accountManager.root.querySelectorAll('cr-icon-button')[0];
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Kebab menu should be focused for settingId=301.');
  });

  test('AccountListIsUpdatedWhenAccountManagerUpdates', function() {
    assertEquals(1, browserProxy.getCallCount('getAccounts'));
    webUIListenerCallback('accounts-changed');
    assertEquals(2, browserProxy.getCallCount('getAccounts'));
  });

  test('ManagementStatusForManagedAccounts', async function() {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const managedBadge = accountManager.root.querySelector(
        '.device-account-icon .managed-badge');
    // Managed badge should be shown for managed accounts.
    assertFalse(managedBadge.hidden);
  });

  test('ArcAvailabilityIsShownForSecondaryAccounts', async function() {
    if (!loadTimeData.getBoolean('arcAccountRestrictionsEnabled')) {
      return;
    }

    await browserProxy.whenCalled('getAccounts');
    flush();

    accountList.items.forEach((item, i) => {
      const notAvailableInArc =
          accountManager.root.querySelectorAll('.arc-availability')[i];
      assertEquals(item.isAvailableInArc, notAvailableInArc.hidden);
    });
  });

  test('ChangeArcAvailability', async function() {
    if (!loadTimeData.getBoolean('arcAccountRestrictionsEnabled')) {
      return;
    }

    await browserProxy.whenCalled('getAccounts');
    flush();

    const testAccount = accountList.items[0];
    const currentValue = testAccount.isAvailableInArc;
    // Click on 'More Actions' for the |testAccount| (First one (index 0)
    // to have the hamburger menu).
    accountManager.root.querySelectorAll('cr-icon-button')[0].click();
    // Click on the button to change ARC availability (the second button in
    // the menu).
    accountManager.shadowRoot.querySelector('cr-action-menu')
        .querySelectorAll('button')[1]
        .click();

    const args = await browserProxy.whenCalled('changeArcAvailability');
    assertEquals(args[0], testAccount);
    assertEquals(args[1], !currentValue);
    // 'More actions' button should be in focus now.
    assertEquals(
        accountManager.root.querySelectorAll('cr-icon-button')[0],
        accountManager.root.activeElement);
  });
});

suite('AccountManagerUnmanagedAccountTests', function() {
  let browserProxy = null;
  let accountManager = null;
  let accountList = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({isDeviceAccountManaged: false});
  });

  setup(function() {
    browserProxy = new TestAccountManagerBrowserProxyForUnmanagedAccounts();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();

    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);
    accountList = accountManager.shadowRoot.querySelector('#account-list');
    assertTrue(!!accountList);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
  });

  teardown(function() {
    accountManager.remove();
  });

  test('ManagementStatusForUnmanagedAccounts', async function() {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const managedBadge = accountManager.root.querySelector(
        '.device-account-icon .managed-badge');
    // Managed badge should not be shown for unmanaged accounts.
    assertEquals(null, managedBadge);
  });
});

suite('AccountManagerAccountAdditionDisabledTests', function() {
  let browserProxy = null;
  let accountManager = null;
  let accountList = null;

  suiteSetup(function() {
    loadTimeData.overrideValues(
        {secondaryGoogleAccountSigninAllowed: false, isChild: false});
  });

  setup(function() {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();

    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);
    accountList = accountManager.shadowRoot.querySelector('#account-list');
    assertTrue(!!accountList);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(function() {
    accountManager.remove();
  });

  test('AddAccountCanBeDisabledByPolicy', function() {
    assertTrue(accountManager.shadowRoot.querySelector('#add-account-button')
                   .disabled);
    assertFalse(
        accountManager.shadowRoot.querySelector(
            '.secondary-accounts-disabled-tooltip') === null);
  });

  test('UserMessageSetForAccountType', function() {
    assertEquals(
        loadTimeData.getString('accountManagerSecondaryAccountsDisabledText'),
        accountManager.shadowRoot
            .querySelector('.secondary-accounts-disabled-tooltip')
            .tooltipText);
  });
});

suite('AccountManagerAccountAdditionDisabledChildAccountTests', function() {
  let browserProxy = null;
  let accountManager = null;
  let accountList = null;

  suiteSetup(function() {
    loadTimeData.overrideValues(
        {secondaryGoogleAccountSigninAllowed: false, isChild: true});
  });

  setup(function() {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();

    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);
    accountList = accountManager.shadowRoot.querySelector('#account-list');
    assertTrue(!!accountList);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(function() {
    accountManager.remove();
  });

  test('UserMessageSetForAccountType', function() {
    assertEquals(
        loadTimeData.getString(
            'accountManagerSecondaryAccountsDisabledChildText'),
        accountManager.shadowRoot
            .querySelector('.secondary-accounts-disabled-tooltip')
            .tooltipText);
  });
});

suite('AccountManagerAccountChildAccountTests', function() {
  let parentalControlsBrowserProxy = null;
  let accountManager = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({isChild: true, isDeviceAccountManaged: true});
  });

  setup(function() {
    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.setInstanceForTesting(
        parentalControlsBrowserProxy);
    PolymerTest.clearBody();

    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(function() {
    accountManager.remove();
  });

  test('FamilyLinkIcon', function() {
    const icon = accountManager.shadowRoot.querySelector(
        '.managed-message cr-icon-button');
    assertTrue(!!icon, 'Could not find the managed icon');

    assertEquals('cr20:kite', icon.ironIcon);

    icon.click();
    assertEquals(
        parentalControlsBrowserProxy.getCallCount('launchFamilyLinkSettings'),
        1);
  });
});
