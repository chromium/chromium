// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {Account} from 'chrome://os-settings/lazy_load.js';
import {AccountManagerSettingsCardElement, CrIconButtonElement, ParentalControlsBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestParentalControlsBrowserProxy} from './test_parental_controls_browser_proxy.js';

suite('<account-manager-settings-card>', () => {
  let accountManagerSettingsCard: AccountManagerSettingsCardElement;
  const managedAccount: Account = {
    id: '123',
    accountType: 1,
    isDeviceAccount: true,
    isSignedIn: true,
    unmigrated: false,
    isManaged: true,
    fullName: 'Primary Account',
    pic: 'data:image/png;base64,primaryAccountPicData',
    email: 'primary@gmail.com',
    isAvailableInArc: true,
    organization: 'Family Link',
  };

  suiteSetup(async () => {
    loadTimeData.overrideValues({isDeviceAccountManaged: true});
  });

  setup(() => {
    accountManagerSettingsCard =
        document.createElement('account-manager-settings-card');
    accountManagerSettingsCard.deviceAccount = managedAccount;
    document.body.appendChild(accountManagerSettingsCard);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    flush();
  });

  teardown(() => {
    accountManagerSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('managed badge is visible if device account is managed', () => {
    const managedBadge = accountManagerSettingsCard.shadowRoot!.querySelector(
        '.device-account-icon .managed-badge');

    assertTrue(isVisible(managedBadge));
  });

  test('account full name is correct and visible', () => {
    const accountFullNameEl =
        accountManagerSettingsCard.shadowRoot!.querySelector(
            '#deviceAccountFullName');

    assert(accountFullNameEl);
    assert(managedAccount);
    assertTrue(isVisible(accountFullNameEl));
    assertEquals(
        managedAccount.fullName, accountFullNameEl.textContent!.trim());
  });

  test('account email is correct and visible', () => {
    const accountEmailEl = accountManagerSettingsCard.shadowRoot!.querySelector(
        '#deviceAccountEmail');

    assert(accountEmailEl);
    assert(managedAccount);
    assertTrue(isVisible(accountEmailEl));
    assertEquals(managedAccount.email, accountEmailEl.textContent!.trim());
  });
});

suite('AccountManagerUnmanagedAccountTests', () => {
  let accountManagerSettingsCard: AccountManagerSettingsCardElement;
  const unmanagedAccount = {
    id: '123',
    accountType: 1,
    isDeviceAccount: true,
    isSignedIn: true,
    unmigrated: false,
    isManaged: false,
    fullName: 'Device Account',
    email: 'admin@domain.com',
    pic: 'data:image/png;base64,abc123',
    isAvailableInArc: false,
  };

  suiteSetup(() => {
    loadTimeData.overrideValues({isDeviceAccountManaged: false});
  });

  setup(() => {
    accountManagerSettingsCard =
        document.createElement('account-manager-settings-card');
    accountManagerSettingsCard.deviceAccount = unmanagedAccount;
    document.body.appendChild(accountManagerSettingsCard);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
  });

  teardown(() => {
    accountManagerSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('ManagementStatusForUnmanagedAccounts', () => {
    const managedBadge = accountManagerSettingsCard.shadowRoot!.querySelector(
        '.device-account-icon .managed-badge');
    // Managed badge should not be shown for unmanaged accounts.
    assertNull(managedBadge);
  });
});

suite('AccountManagerAccountChildAccountTests', () => {
  let parentalControlsBrowserProxy: TestParentalControlsBrowserProxy;
  let accountManagerSettingsCard: AccountManagerSettingsCardElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({isChild: true, isDeviceAccountManaged: true});

    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.setInstanceForTesting(
        parentalControlsBrowserProxy);
  });

  setup(() => {
    accountManagerSettingsCard =
        document.createElement('account-manager-settings-card');
    document.body.appendChild(accountManagerSettingsCard);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    flush();
  });

  teardown(() => {
    accountManagerSettingsCard.remove();
    parentalControlsBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('Family link icon is visible and launches family link settings', () => {
    const icon = accountManagerSettingsCard.shadowRoot!
                     .querySelector<CrIconButtonElement>(
                         '.managed-message cr-icon-button');
    assertTrue(!!icon, 'Could not find the managed icon');
    assertTrue(isVisible(icon));

    assertEquals('cr20:kite', icon.ironIcon);

    icon.click();
    assertEquals(
        1,
        parentalControlsBrowserProxy.getCallCount('launchFamilyLinkSettings'));
  });
});
