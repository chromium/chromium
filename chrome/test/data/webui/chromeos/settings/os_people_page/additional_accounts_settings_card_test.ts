// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {AdditionalAccountsSettingsCardElement, CrTooltipIconElement, Router, routes, settingMojom, setUserActionRecorderForTesting} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';

import {TestAccountManagerBrowserProxy} from './test_account_manager_browser_proxy.js';

suite('<additonal-accounts-settings-card>', () => {
  let browserProxy: TestAccountManagerBrowserProxy;
  let additionalAccountSettingsCard: AdditionalAccountsSettingsCardElement;
  let accountList: DomRepeat;
  let userActionRecorder: FakeUserActionRecorder;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isDeviceAccountManaged: true,
    });

    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);
  });

  setup(async () => {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
    const accounts = await browserProxy.getAccounts();

    additionalAccountSettingsCard =
        document.createElement('additional-accounts-settings-card');
    additionalAccountSettingsCard.accounts = accounts;
    document.body.appendChild(additionalAccountSettingsCard);
    const list =
        additionalAccountSettingsCard.shadowRoot!.querySelector<DomRepeat>(
            '#secondaryAccountsList');
    assertTrue(!!list);
    accountList = list;

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    flush();
  });

  teardown(() => {
    additionalAccountSettingsCard.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('accountList is populated at startup', () => {
    // 1 device account + 3 secondary accounts were added in
    // |getAccounts()| mock above.
    assertEquals(3, accountList.items!.length);
  });

  test(
      'addAccount function gets called when clicking on the addAccount button',
      () => {
        const button =
            additionalAccountSettingsCard.shadowRoot!
                .querySelector<HTMLButtonElement>('#addAccountButton');

        assert(button);
        assertFalse(button.disabled);
        assertNull(additionalAccountSettingsCard.shadowRoot!.querySelector(
            '.secondary-accounts-disabled-tooltip'));
        button.click();
        assertEquals(1, browserProxy.getCallCount('addAccount'));
      });

  test(
      'reauthenticateAccount gets called with unsigned in account',
      async () => {
        additionalAccountSettingsCard.shadowRoot!
            .querySelectorAll<HTMLButtonElement>('.reauth-button')[0]!.click();
        assertEquals(1, browserProxy.getCallCount('reauthenticateAccount'));
        const accountEmail =
            await browserProxy.whenCalled('reauthenticateAccount');
        assertEquals('user2@example.com', accountEmail);
      });

  test(
      'unauthenticated account label is shown for the unauthenticated account',
      () => {
        assertEquals(
            loadTimeData.getString('accountManagerReauthenticationLabel'),
            additionalAccountSettingsCard.shadowRoot!
                .querySelectorAll('.reauth-button')[0]!.textContent!.trim());
      });

  test('unmigrated account label is shown for the unmigrated account', () => {
    assertEquals(
        loadTimeData.getString('accountManagerMigrationLabel'),
        additionalAccountSettingsCard.shadowRoot!
            .querySelectorAll('.reauth-button')[1]!.textContent!.trim());
  });

  test('remove account', async () => {
    // Click on 'More Actions' for the second account (First one (index 0)
    // to have the hamburger menu).
    additionalAccountSettingsCard.shadowRoot!
        .querySelectorAll('cr-icon-button')[0]!.click();
    // Click on 'Remove account' (the first button in the menu).
    const actionMenu = additionalAccountSettingsCard.shadowRoot!.querySelector(
        'cr-action-menu');
    assertTrue(!!actionMenu);
    actionMenu.querySelectorAll('button')[0]!.click();

    if (loadTimeData.getBoolean('lacrosEnabled')) {
      const confirmationDialog =
          additionalAccountSettingsCard.shadowRoot!.querySelector(
              '#removeConfirmationDialog');
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
        additionalAccountSettingsCard.shadowRoot!.querySelector(
            '#addAccountButton'),
        getDeepActiveElement());
  });

  test('Deep link to remove account button', async () => {
    const params = new URLSearchParams();
    const removeAccountSettingId =
        settingMojom.Setting.kRemoveAccount.toString();
    params.append('settingId', removeAccountSettingId);
    Router.getInstance().navigateTo(routes.OS_PEOPLE, params);

    flush();

    const deepLinkElement =
        additionalAccountSettingsCard.shadowRoot!.querySelectorAll(
            'cr-icon-button')[0];
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Kebab menu should be focused for settingId${removeAccountSettingId}.`);
  });

  if (loadTimeData.getBoolean('arcAccountRestrictionsEnabled')) {
    test('arc availability is shown for secondary accounts', () => {
      accountList.items!.forEach((item, i) => {
        const notAvailableInArc =
            additionalAccountSettingsCard.shadowRoot!
                .querySelectorAll<HTMLElement>('.arc-availability')[i];
        assertTrue(!!notAvailableInArc);
        assertEquals(item.isAvailableInArc, notAvailableInArc.hidden);
      });
    });

    test('change arc availability', async () => {
      const testAccount = accountList.items![0];
      const currentValue = testAccount.isAvailableInArc;
      // Click on 'More Actions' for the |testAccount| (First one (index 0)
      // to have the hamburger menu).
      additionalAccountSettingsCard.shadowRoot!
          .querySelectorAll('cr-icon-button')[0]!.click();
      // Click on the button to change ARC availability (the second button in
      // the menu).
      const actionMenu =
          additionalAccountSettingsCard.shadowRoot!.querySelector(
              'cr-action-menu');
      assertTrue(!!actionMenu);
      actionMenu.querySelectorAll('button')[1]!.click();

      const args = await browserProxy.whenCalled('changeArcAvailability');
      assertEquals(testAccount, args[0]);
      assertEquals(!currentValue, args[1]);
      // 'More actions' button should be in focus now.
      assertEquals(
          additionalAccountSettingsCard.shadowRoot!.querySelectorAll(
              'cr-icon-button')[0],
          getDeepActiveElement());
    });
  }
});

suite('AccountManagerAccountAdditionDisabledTests', () => {
  let browserProxy: TestAccountManagerBrowserProxy;
  let additionalAccountSettingsCard: AdditionalAccountsSettingsCardElement;

  suiteSetup(() => {
    loadTimeData.overrideValues(
        {secondaryGoogleAccountSigninAllowed: false, isChild: false});

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  setup(() => {
    additionalAccountSettingsCard =
        document.createElement('additional-accounts-settings-card');
    document.body.appendChild(additionalAccountSettingsCard);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    flush();
  });

  teardown(() => {
    additionalAccountSettingsCard.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('add account can be disabled by policy', () => {
    const button = additionalAccountSettingsCard.shadowRoot!
                       .querySelector<HTMLButtonElement>('#addAccountButton');
    assertTrue(!!button);
    assertTrue(button.disabled);
    assertTrue(!!additionalAccountSettingsCard.shadowRoot!.querySelector(
        '.secondary-accounts-disabled-tooltip'));
  });

  test('user message is set for account type', () => {
    const tooltip = additionalAccountSettingsCard.shadowRoot!
                        .querySelector<CrTooltipIconElement>(
                            '.secondary-accounts-disabled-tooltip');
    assertTrue(!!tooltip);
    assertEquals(
        loadTimeData.getString('accountManagerSecondaryAccountsDisabledText'),
        tooltip.tooltipText);
  });
});

suite('SecondaryAccountAllowedInArcPolicyTests', () => {
  let browserProxy: TestAccountManagerBrowserProxy;
  let additionalAccountSettingsCard: AdditionalAccountsSettingsCardElement;
  let accountList: DomRepeat;
  let userActionRecorder: FakeUserActionRecorder;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isDeviceAccountManaged: true,
      arcManagedAccountRestrictionEnabled: true,
    });

    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);
  });

  setup(async () => {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
    const accounts = await browserProxy.getAccounts();

    additionalAccountSettingsCard =
        document.createElement('additional-accounts-settings-card');
    additionalAccountSettingsCard.accounts = accounts;
    document.body.appendChild(additionalAccountSettingsCard);
    const list =
        additionalAccountSettingsCard.shadowRoot!.querySelector<DomRepeat>(
            '#secondaryAccountsList');
    assertTrue(!!list);
    accountList = list;

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    flush();
  });

  teardown(() => {
    additionalAccountSettingsCard.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('arc availability is shown for secondary accounts', () => {
    accountList.items!.forEach((item, i) => {
      const notAvailableInArc =
          additionalAccountSettingsCard.shadowRoot!
              .querySelectorAll<HTMLElement>('.arc-availability')[i];
      assertTrue(!!notAvailableInArc);
      assertEquals(item.isAvailableInArc, notAvailableInArc.hidden);
    });
  });
});
