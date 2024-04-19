// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AccountManagerBrowserProxyImpl, SettingsAccountManagerSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrIconButtonElement, CrTooltipIconElement, ParentalControlsBrowserProxyImpl, Router, routes, settingMojom, setUserActionRecorderForTesting} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';

import {TestAccountManagerBrowserProxy, TestAccountManagerBrowserProxyForUnmanagedAccounts} from './test_account_manager_browser_proxy.js';
import {TestParentalControlsBrowserProxy} from './test_parental_controls_browser_proxy.js';

suite('<settings-account-manager-subpage>', () => {
  let browserProxy: TestAccountManagerBrowserProxy;
  let accountManager: SettingsAccountManagerSubpageElement;
  let accountList: DomRepeat;
  let userActionRecorder: FakeUserActionRecorder;

  suiteSetup(() => {
    loadTimeData.overrideValues({isDeviceAccountManaged: true});

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  setup(() => {
    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);

    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);
    const list =
        accountManager.shadowRoot!.querySelector<DomRepeat>('#account-list');
    assertTrue(!!list);
    accountList = list;

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(() => {
    accountManager.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('AccountListIsPopulatedAtStartup', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    // 1 device account + 3 secondary accounts were added in
    // |getAccounts()| mock above.
    assertEquals(3, accountList.items!.length);
  });

  test('AddAccount', () => {
    const button = accountManager.shadowRoot!.querySelector<HTMLButtonElement>(
        '#add-account-button');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertNull(accountManager.shadowRoot!.querySelector(
        '.secondary-accounts-disabled-tooltip'));
    button.click();
    assertEquals(1, browserProxy.getCallCount('addAccount'));
  });

  test('ReauthenticateAccount', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    accountManager.shadowRoot!
        .querySelectorAll<HTMLButtonElement>('.reauth-button')[0]!.click();
    assertEquals(1, browserProxy.getCallCount('reauthenticateAccount'));
    const accountEmail = await browserProxy.whenCalled('reauthenticateAccount');
    assertEquals('user2@example.com', accountEmail);
  });

  test('UnauthenticatedAccountLabel', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    assertEquals(
        loadTimeData.getString('accountManagerReauthenticationLabel'),
        accountManager.shadowRoot!.querySelectorAll('.reauth-button')[0]!
            .textContent!.trim());
  });

  test('UnmigratedAccountLabel', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    assertEquals(
        loadTimeData.getString('accountManagerMigrationLabel'),
        accountManager.shadowRoot!.querySelectorAll('.reauth-button')[1]!
            .textContent!.trim());
  });

  test('RemoveAccount', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    // Click on 'More Actions' for the second account (First one (index 0)
    // to have the hamburger menu).
    accountManager.shadowRoot!.querySelectorAll('cr-icon-button')[0]!.click();
    // Click on 'Remove account' (the first button in the menu).
    const actionMenu =
        accountManager.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    actionMenu.querySelectorAll('button')[0]!.click();

    if (loadTimeData.getBoolean('lacrosEnabled')) {
      const confirmationDialog =
          accountManager.shadowRoot!.querySelector('#removeConfirmationDialog');
      assertTrue(!!confirmationDialog);
      const button = confirmationDialog.querySelector<HTMLButtonElement>(
          '#removeConfirmationButton');
      assertTrue(!!button);
      button.click();
    }

    const account = await browserProxy.whenCalled('removeAccount');
    assertEquals('456', account.id);
    // Add account button should be in focus now.
    assertEquals(
        accountManager.shadowRoot!.querySelector('#add-account-button'),
        getDeepActiveElement());
  });

  test('Deep link to remove account button', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kRemoveAccount.toString());
    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER, params);

    const deepLinkElement =
        accountManager.shadowRoot!.querySelectorAll('cr-icon-button')[0];
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Kebab menu should be focused for settingId=301.');
  });

  test('AccountListIsUpdatedWhenAccountManagerUpdates', () => {
    assertEquals(1, browserProxy.getCallCount('getAccounts'));
    webUIListenerCallback('accounts-changed');
    assertEquals(2, browserProxy.getCallCount('getAccounts'));
  });

  test('ManagementStatusForManagedAccounts', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const managedBadge = accountManager.shadowRoot!.querySelector<HTMLElement>(
        '.device-account-icon .managed-badge');
    assertTrue(!!managedBadge);
    // Managed badge should be shown for managed accounts.
    assertFalse(managedBadge.hidden);
  });

  test('ArcAvailabilityIsShownForSecondaryAccounts', async () => {
    if (!loadTimeData.getBoolean('arcAccountRestrictionsEnabled')) {
      return;
    }

    await browserProxy.whenCalled('getAccounts');
    flush();

    accountList.items!.forEach((item, i) => {
      const notAvailableInArc =
          accountManager.shadowRoot!.querySelectorAll<HTMLElement>(
              '.arc-availability')[i];
      assertTrue(!!notAvailableInArc);
      assertEquals(item.isAvailableInArc, notAvailableInArc.hidden);
    });
  });

  test('ChangeArcAvailability', async () => {
    if (!loadTimeData.getBoolean('arcAccountRestrictionsEnabled')) {
      return;
    }

    await browserProxy.whenCalled('getAccounts');
    flush();

    const testAccount = accountList.items![0];
    const currentValue = testAccount.isAvailableInArc;
    // Click on 'More Actions' for the |testAccount| (First one (index 0)
    // to have the hamburger menu).
    accountManager.shadowRoot!.querySelectorAll('cr-icon-button')[0]!.click();
    // Click on the button to change ARC availability (the second button in
    // the menu).
    const actionMenu =
        accountManager.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    actionMenu.querySelectorAll('button')[1]!.click();

    const args = await browserProxy.whenCalled('changeArcAvailability');
    assertEquals(testAccount, args[0]);
    assertEquals(!currentValue, args[1]);
    // 'More actions' button should be in focus now.
    assertEquals(
        accountManager.shadowRoot!.querySelectorAll('cr-icon-button')[0],
        getDeepActiveElement());
  });
});

suite('AccountManagerUnmanagedAccountTests', () => {
  let browserProxy: TestAccountManagerBrowserProxyForUnmanagedAccounts;
  let accountManager: SettingsAccountManagerSubpageElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({isDeviceAccountManaged: false});

    browserProxy = new TestAccountManagerBrowserProxyForUnmanagedAccounts();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  setup(() => {
    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
  });

  teardown(() => {
    accountManager.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('ManagementStatusForUnmanagedAccounts', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const managedBadge = accountManager.shadowRoot!.querySelector(
        '.device-account-icon .managed-badge');
    // Managed badge should not be shown for unmanaged accounts.
    assertNull(managedBadge);
  });
});

suite('AccountManagerAccountAdditionDisabledTests', () => {
  let browserProxy: TestAccountManagerBrowserProxy;
  let accountManager: SettingsAccountManagerSubpageElement;

  suiteSetup(() => {
    loadTimeData.overrideValues(
        {secondaryGoogleAccountSigninAllowed: false, isChild: false});

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  setup(() => {
    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(() => {
    accountManager.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('AddAccountCanBeDisabledByPolicy', () => {
    const button = accountManager.shadowRoot!.querySelector<HTMLButtonElement>(
        '#add-account-button');
    assertTrue(!!button);
    assertTrue(button.disabled);
    assertTrue(!!accountManager.shadowRoot!.querySelector(
        '.secondary-accounts-disabled-tooltip'));
  });

  test('UserMessageSetForAccountType', () => {
    const tooltip =
        accountManager.shadowRoot!.querySelector<CrTooltipIconElement>(
            '.secondary-accounts-disabled-tooltip');
    assertTrue(!!tooltip);
    assertEquals(
        loadTimeData.getString('accountManagerSecondaryAccountsDisabledText'),
        tooltip.tooltipText);
  });
});

suite('AccountManagerAccountAdditionDisabledChildAccountTests', () => {
  let browserProxy: TestAccountManagerBrowserProxy;
  let accountManager: SettingsAccountManagerSubpageElement;

  suiteSetup(() => {
    loadTimeData.overrideValues(
        {secondaryGoogleAccountSigninAllowed: false, isChild: true});

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  setup(() => {
    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(() => {
    accountManager.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('UserMessageSetForAccountType', () => {
    const tooltip =
        accountManager.shadowRoot!.querySelector<CrTooltipIconElement>(
            '.secondary-accounts-disabled-tooltip');
    assertTrue(!!tooltip);
    assertEquals(
        loadTimeData.getString(
            'accountManagerSecondaryAccountsDisabledChildText'),
        tooltip.tooltipText);
  });
});

suite('AccountManagerAccountChildAccountTests', () => {
  let parentalControlsBrowserProxy: TestParentalControlsBrowserProxy;
  let accountManager: SettingsAccountManagerSubpageElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({isChild: true, isDeviceAccountManaged: true});

    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.setInstanceForTesting(
        parentalControlsBrowserProxy);
  });

  setup(() => {
    accountManager = document.createElement('settings-account-manager-subpage');
    document.body.appendChild(accountManager);

    Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    flush();
  });

  teardown(() => {
    accountManager.remove();
    parentalControlsBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('FamilyLinkIcon', () => {
    const icon = accountManager.shadowRoot!.querySelector<CrIconButtonElement>(
        '.managed-message cr-icon-button');
    assertTrue(!!icon, 'Could not find the managed icon');

    assertEquals('cr20:kite', icon.ironIcon);

    icon.click();
    assertEquals(
        1,
        parentalControlsBrowserProxy.getCallCount('launchFamilyLinkSettings'));
  });
});
