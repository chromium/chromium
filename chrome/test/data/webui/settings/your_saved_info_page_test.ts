// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsYourSavedInfoPageElement} from 'chrome://settings/settings.js';
import {AutofillManagerImpl, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createAddressEntry, createCreditCardEntry, createIbanEntry, createPayOverTimeIssuerEntry, TestAutofillManager, TestPaymentsManager} from './autofill_fake_data.js';

suite('YourSavedInfoPage', function() {
  let yourSavedInfoPage: SettingsYourSavedInfoPageElement;
  let autofillManager: TestAutofillManager;
  let paymentsManager: TestPaymentsManager;

  setup(async function() {
    autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [createAddressEntry()];
    AutofillManagerImpl.setInstance(autofillManager);
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