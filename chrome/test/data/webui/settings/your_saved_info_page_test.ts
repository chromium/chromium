// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SettingsYourSavedInfoPageElement} from 'chrome://settings/settings.js';
import {AutofillManagerImpl, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PasswordManagerImpl, PasswordManagerPage, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createAddressEntry, createCreditCardEntry, createIbanEntry, createPayOverTimeIssuerEntry, TestAutofillManager, TestPaymentsManager} from './autofill_fake_data.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

suite('YourSavedInfoPage', function() {
  let yourSavedInfoPage: SettingsYourSavedInfoPageElement;
  let autofillManager: TestAutofillManager;
  let passwordManager: TestPasswordManagerProxy;
  let paymentsManager: TestPaymentsManager;

  setup(async function() {
    Router.resetInstanceForTesting(new Router(routes));

    // Override for testing.
    autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [createAddressEntry()];
    AutofillManagerImpl.setInstance(autofillManager);
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    paymentsManager = new TestPaymentsManager();
    PaymentsManagerImpl.setInstance(paymentsManager);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    yourSavedInfoPage =
        document.createElement('settings-your-saved-info-page');
    document.body.appendChild(yourSavedInfoPage);
    await flushTasks();
  });

  test('TitleExists', function() {
    const yourSavedInfoPageTitleElement =
        yourSavedInfoPage.shadowRoot!.querySelector('#yourSavedInfoPageTitle');
    assertTrue(!!yourSavedInfoPageTitleElement);
  });

  test('AddressesAndPaymentsCountersAreUpdated', async function() {
    await autofillManager.whenCalled('getAddressList');
    await paymentsManager.whenCalled('getCreditCardList');
    await paymentsManager.whenCalled('getIbanList');
    await paymentsManager.whenCalled('getPayOverTimeIssuerList');

    assertEquals(1, yourSavedInfoPage.addressesCount);
    assertEquals(0, yourSavedInfoPage.creditCardsCount);
    assertEquals(0, yourSavedInfoPage.ibansCount);
    assertEquals(0, yourSavedInfoPage.payOverTimeIssuersCount);

    const addressList = [createAddressEntry(), createAddressEntry()];
    const cardList = [createCreditCardEntry()];
    const ibanList = [createIbanEntry(), createIbanEntry(), createIbanEntry()];
    const payOverTimeIssuerList = [createPayOverTimeIssuerEntry()];

    autofillManager.lastCallback.setPersonalDataManagerListener!
        (addressList, cardList, ibanList, payOverTimeIssuerList);
    await flushTasks();

    assertEquals(2, yourSavedInfoPage.addressesCount);
    assertEquals(1, yourSavedInfoPage.creditCardsCount);
    assertEquals(3, yourSavedInfoPage.ibansCount);
    assertEquals(1, yourSavedInfoPage.payOverTimeIssuersCount);
  });

});

suite('RelatedServices', function() {
  let openUrl: string|URL|undefined;
  let yourSavedInfoPage: SettingsYourSavedInfoPageElement;
  let passwordManager: TestPasswordManagerProxy;

  // Save the original window.open and restore it after the test.
  const originalOpen = window.open;

  setup(function() {
    // Mock window.open to be able to check that it was called.
    window.open = (url) => {
      openUrl = url;
      return null;
    };

    Router.resetInstanceForTesting(new Router(routes));

    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    yourSavedInfoPage =
        document.createElement('settings-your-saved-info-page');
    document.body.appendChild(yourSavedInfoPage);
    flush();
  });

  teardown(function() {
    window.open = originalOpen;
  });

  test('CardRendersCorrectly', function() {
    const relatedServicesCard =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
            `settings-section[page-title="${
                loadTimeData.getString('yourSavedInfoRelatedServicesTitle')}"]`);
    assertTrue(!!relatedServicesCard);

    assertTrue(
        !!relatedServicesCard.querySelector('#passwordManagerButton'),
        'Password manager button not found');
    assertTrue(
        !!relatedServicesCard.querySelector('#walletButton'),
        'Wallet button not found');
    assertTrue(
        !!relatedServicesCard.querySelector('#profileButton'),
        'Profile button not found');
  });

  test('PasswordManagerRowOpensPasswordManager', async function() {
    const passwordManagerRow =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
            '#passwordManagerButton');
    assertTrue(!!passwordManagerRow);
    passwordManagerRow.click();

    const page = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.PASSWORDS, page);
    assertEquals(undefined, openUrl);
  });

  test('WalletRowOpensWallet', function() {
    const walletRow =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
            '#walletButton');
    assertTrue(!!walletRow);
    walletRow.click();
    assertEquals('https://wallet.google.com', openUrl);
  });

  test('ProfileRowOpensProfile', function() {
    const profileRow =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
            '#profileButton');
    assertTrue(!!profileRow);
    profileRow.click();
    assertEquals('https://myaccount.google.com', openUrl);
  });
});
