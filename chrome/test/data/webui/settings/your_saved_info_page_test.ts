// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AutofillManagerImpl, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsYourSavedInfoPageElement} from 'chrome://settings/settings.js';
import {loadTimeData, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

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
    yourSavedInfoPage = document.createElement('settings-your-saved-info-page');
    document.body.appendChild(yourSavedInfoPage);
    await flushTasks();
  });

  test('TitleExists', function() {
    const yourSavedInfoPageTitleElement =
        yourSavedInfoPage.shadowRoot!.querySelector('#yourSavedInfoPageTitle');
    assertTrue(!!yourSavedInfoPageTitleElement);
  });

  test('CardsRenderCorrectly', function() {
    const cards = yourSavedInfoPage.shadowRoot!.querySelectorAll(
        'category-reference-card');
    const expectedCardTitles = [
      loadTimeData.getString('localPasswordManager'),
      loadTimeData.getString('paymentsTitle'),
    ];

    assertEquals(expectedCardTitles.length, cards.length);
    for (let i = 0; i < expectedCardTitles.length; i++) {
      assertEquals(expectedCardTitles[i], cards[i]!.cardTitle);
    }
  });

  test('passwordsCardOpensPasswordManager', async function() {
    const passwordsCard =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(`
        category-reference-card[card-title="${
            loadTimeData.getString('localPasswordManager')}"]`);
    assertTrue(!!passwordsCard);

    passwordsCard.shadowRoot!.querySelector<HTMLElement>(
                                 'cr-link-row')!.click();

    const page = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.PASSWORDS, page);
  });

  test('paymentsCardNavigatesToPayments', function() {
    const paymentsCard =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(`
        category-reference-card[card-title="${
            loadTimeData.getString('paymentsTitle')}"]`);
    assertTrue(!!paymentsCard);

    paymentsCard.shadowRoot!.querySelector('cr-link-row')!.click();
    assertEquals(routes.PAYMENTS, Router.getInstance().currentRoute);
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
  let yourSavedInfoPage: SettingsYourSavedInfoPageElement;
  let openWindowProxy: TestOpenWindowProxy;
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    Router.resetInstanceForTesting(new Router(routes));

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    yourSavedInfoPage = document.createElement('settings-your-saved-info-page');
    document.body.appendChild(yourSavedInfoPage);
  });

  async function testRowOpensUrl(selector: string, urlStringId: string) {
    const row =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(selector);
    assertTrue(!!row);
    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString(urlStringId), url);
  }

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
        !!relatedServicesCard.querySelector('#googleWalletButton'),
        'Wallet button not found');
    assertTrue(
        !!relatedServicesCard.querySelector('#googleAccountButton'),
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
  });

  test('WalletRowOpensWallet', function() {
    return testRowOpensUrl('#googleWalletButton', 'googleWalletUrl');
  });

  test('ProfileRowOpensProfile', function() {
    return testRowOpensUrl('#googleAccountButton', 'googleAccountUrl');
  });
});
