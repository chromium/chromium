// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_account_manager', function() {
  /** @implements {settings.AccountManagerBrowserProxy} */
  class TestAccountManagerBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'getAccounts',
        'addAccount',
        'reauthenticateAccount',
        'removeAccount',
        'showWelcomeDialogIfRequired',
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
          organization: 'Family Link',
        },
        {
          id: '456',
          accountType: 1,
          isDeviceAccount: false,
          isSignedIn: true,
          unmigrated: false,
          fullName: 'Secondary Account 1',
          email: 'user1@example.com',
          pic: '',
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
        }
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
    showWelcomeDialogIfRequired() {
      this.methodCalled('showWelcomeDialogIfRequired');
    }
  }

  /** @implements {settings.AccountManagerBrowserProxy} */
  class TestAccountManagerBrowserProxyForUnmanagedAccounts extends
      TestAccountManagerBrowserProxy {
    constructor() {
      super([
        'getAccounts',
        'addAccount',
        'reauthenticateAccount',
        'removeAccount',
        'showWelcomeDialogIfRequired',
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

  suite('AccountManagerTests', function() {
    let browserProxy = null;
    let accountManager = null;
    let accountList = null;

    suiteSetup(function() {});

    setup(function() {
      browserProxy = new TestAccountManagerBrowserProxy();
      settings.AccountManagerBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      accountManager = document.createElement('settings-account-manager');
      document.body.appendChild(accountManager);
      accountList = accountManager.$$('#account-list');
      assertTrue(!!accountList);

      settings.navigateTo(settings.routes.ACCOUNT_MANAGER);
    });

    teardown(function() {
      accountManager.remove();
    });

    test('AccountListIsPopulatedAtStartup', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // 4 accounts were added in |getAccounts()| mock above.
        assertEquals(4, accountList.items.length);
      });
    });

    test('AddAccount', function() {
      assertFalse(accountManager.$$('#add-account-button').disabled);
      assertTrue(accountManager.$$('#settings-box-user-message').hidden);
      accountManager.$$('#add-account-button').click();
      assertEquals(1, browserProxy.getCallCount('addAccount'));
    });

    test('ReauthenticateAccount', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        accountManager.root.querySelectorAll('.reauth-button')[0].click();
        assertEquals(1, browserProxy.getCallCount('reauthenticateAccount'));
        return browserProxy.whenCalled('reauthenticateAccount')
            .then((account_email) => {
              assertEquals('user2@example.com', account_email);
            });
      });
    });

    test('UnauthenticatedAccountLabel', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        assertEquals(
            loadTimeData.getString('accountManagerReauthenticationLabel'),
            accountManager.root.querySelectorAll('.reauth-button')[0]
                .textContent.trim());
      });
    });

    test('UnmigratedAccountLabel', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        assertEquals(
            loadTimeData.getString('accountManagerMigrationLabel'),
            accountManager.root.querySelectorAll('.reauth-button')[1]
                .textContent.trim());
      });
    });

    test('RemoveAccount', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // Click on 'More Actions' for the second account (First one (index 0)
        // to have the hamburger menu).
        accountManager.root.querySelectorAll('cr-icon-button')[0].click();
        // Click on 'Remove account'
        accountManager.$$('cr-action-menu').querySelector('button').click();

        return browserProxy.whenCalled('removeAccount').then((account) => {
          assertEquals('456', account.id);
        });
      });
    });

    test('AccountListIsUpdatedWhenAccountManagerUpdates', function() {
      assertEquals(1, browserProxy.getCallCount('getAccounts'));
      cr.webUIListenerCallback('accounts-changed');
      assertEquals(2, browserProxy.getCallCount('getAccounts'));
    });

    test('WelcomeScreenIsShownIfRequired', function() {
      // We have navigated to |settings.routes.ACCOUNT_MANAGER| in |setup|. A
      // welcome screen should be shown if required.
      assertGT(browserProxy.getCallCount('showWelcomeDialogIfRequired'), 0);
    });

    test('ManagementStatusForManagedAccounts', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();

        const managementLabel =
            accountManager.root.querySelectorAll('.management-status')[0]
                .innerHTML.trim();
        assertEquals('Managed by Family Link', managementLabel);
      });
    });
  });

  suite('AccountManagerUnmanagedAccountTests', function() {
    let browserProxy = null;
    let accountManager = null;
    let accountList = null;

    setup(function() {
      browserProxy = new TestAccountManagerBrowserProxyForUnmanagedAccounts();
      settings.AccountManagerBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      accountManager = document.createElement('settings-account-manager');
      document.body.appendChild(accountManager);
      accountList = accountManager.$$('#account-list');
      assertTrue(!!accountList);

      settings.navigateTo(settings.routes.ACCOUNT_MANAGER);
    });

    teardown(function() {
      accountManager.remove();
    });

    test('ManagementStatusForUnmanagedAccounts', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();

        const managementLabel =
            accountManager.root.querySelectorAll('.management-status')[0]
                .innerHTML.trim();
        assertEquals('Primary account', managementLabel);
      });
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
      settings.AccountManagerBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      accountManager = document.createElement('settings-account-manager');
      document.body.appendChild(accountManager);
      accountList = accountManager.$$('#account-list');
      assertTrue(!!accountList);

      settings.navigateTo(settings.routes.ACCOUNT_MANAGER);
    });

    teardown(function() {
      accountManager.remove();
    });

    test('AddAccountCanBeDisabledByPolicy', function() {
      assertTrue(accountManager.$$('#add-account-button').disabled);
      assertFalse(accountManager.$$('#settings-box-user-message').hidden);
    });

    test('UserMessageSetForAccountType', function() {
      assertEquals(
          loadTimeData.getString('accountManagerSecondaryAccountsDisabledText'),
          accountManager.$$('#user-message-text').textContent.trim());
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
      settings.AccountManagerBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      accountManager = document.createElement('settings-account-manager');
      document.body.appendChild(accountManager);
      accountList = accountManager.$$('#account-list');
      assertTrue(!!accountList);

      settings.navigateTo(settings.routes.ACCOUNT_MANAGER);
    });

    teardown(function() {
      accountManager.remove();
    });

    test('UserMessageSetForAccountType', function() {
      assertEquals(
          loadTimeData.getString(
              'accountManagerSecondaryAccountsDisabledChildText'),
          accountManager.$$('#user-message-text').textContent.trim());
    });
  });
});
