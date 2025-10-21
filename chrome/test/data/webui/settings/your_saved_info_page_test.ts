// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiEnterpriseFeaturePrefName, AutofillManagerImpl, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement, SettingsYourSavedInfoPageElement} from 'chrome://settings/settings.js';
import {loadTimeData, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {createAddressEntry, createCreditCardEntry, createIbanEntry, createPayOverTimeIssuerEntry, TestAutofillManager, TestPaymentsManager} from './autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

function setDefaultPrefs(objectToSetup: SettingsPrefsElement) {
  objectToSetup.set(
      `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
      ModelExecutionEnterprisePolicyValue.ALLOW);
  objectToSetup.set(
      'prefs.optimization_guide.model_execution.autofill_prediction_improvements_enterprise_policy_allowed.value',
      ModelExecutionEnterprisePolicyValue.ALLOW);
}

suite('YourSavedInfoPage', function() {
  let yourSavedInfoPage: SettingsYourSavedInfoPageElement;
  let autofillManager: TestAutofillManager;
  let passwordManager: TestPasswordManagerProxy;
  let paymentsManager: TestPaymentsManager;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

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

    loadTimeData.overrideValues({
      showIbansSettings: false,
      shouldShowPayOverTimeSettings: false,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    yourSavedInfoPage = document.createElement('settings-your-saved-info-page');

    setDefaultPrefs(settingsPrefs);
    yourSavedInfoPage.prefs = settingsPrefs.prefs!;

    document.body.appendChild(yourSavedInfoPage);
    await flushTasks();
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
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
      loadTimeData.getString('contactInfoTitle'),
      loadTimeData.getString('identityDocsCardTitle'),
      loadTimeData.getString('travelCardTitle'),
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

  [
    {cardTitle: 'paymentsTitle', expectedRoute: routes.PAYMENTS},
    {cardTitle: 'contactInfoTitle', expectedRoute: routes.ADDRESSES},
    // TODO(crbug.com/438666322): Update routing once the Identity docs subpage is created.
    {cardTitle: 'identityDocsCardTitle', expectedRoute: routes.BASIC},
    // TODO(crbug.com/438666322): Update routing once the Travel subpage is created.
    {cardTitle: 'travelCardTitle', expectedRoute: routes.BASIC},
  ].forEach(({cardTitle, expectedRoute}) => {
    test(`${cardTitle} card navigates to the correct route`, function() {
      const card = yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
          `category-reference-card[card-title="${
              loadTimeData.getString(cardTitle)}"]`);
      assertTrue(!!card);

      card.shadowRoot!.querySelector('cr-link-row')!.click();
      assertEquals(expectedRoute, Router.getInstance().currentRoute);
    });
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

  test('PaymentsChipsVisibility', async function() {
    const paymentsCard =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(`
        category-reference-card[card-title="${
            loadTimeData.getString('paymentsTitle')}"]`);
    assertTrue(!!paymentsCard);

    const getChipLabels = () => {
      const chips = paymentsCard.shadowRoot!.querySelectorAll('cr-chip');
      return Array.from(chips).map(
          chip => chip.querySelector('span')!.textContent.trim());
    };

    let expectedLabels = [
      loadTimeData.getString('creditAndDebitCardTitle'),
      loadTimeData.getString('loyaltyCardsTitle'),
    ];
    assertDeepEquals(expectedLabels, getChipLabels());

    // Enable IBANs
    yourSavedInfoPage.set('enableIbans_', true);
    await flushTasks();
    expectedLabels = [
      loadTimeData.getString('creditAndDebitCardTitle'),
      loadTimeData.getString('ibanTitle'),
      loadTimeData.getString('loyaltyCardsTitle'),
    ];
    assertDeepEquals(expectedLabels, getChipLabels());

    // Enable Pay over time, disable IBANs
    yourSavedInfoPage.set('enableIbans_', false);
    yourSavedInfoPage.set('enablePayOverTime_', true);
    await flushTasks();
    expectedLabels = [
      loadTimeData.getString('creditAndDebitCardTitle'),
      loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
      loadTimeData.getString('loyaltyCardsTitle'),
    ];
    assertDeepEquals(expectedLabels, getChipLabels());

    // Enable both
    yourSavedInfoPage.set('enableIbans_', true);
    await flushTasks();
    expectedLabels = [
      loadTimeData.getString('creditAndDebitCardTitle'),
      loadTimeData.getString('ibanTitle'),
      loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
      loadTimeData.getString('loyaltyCardsTitle'),
    ];
    assertDeepEquals(expectedLabels, getChipLabels());
  });
});

suite('RelatedServices', function() {
  let yourSavedInfoPage: SettingsYourSavedInfoPageElement;
  let openWindowProxy: TestOpenWindowProxy;
  let passwordManager: TestPasswordManagerProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    Router.resetInstanceForTesting(new Router(routes));

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    yourSavedInfoPage =
        document.createElement('settings-your-saved-info-page');
    setDefaultPrefs(settingsPrefs);
    yourSavedInfoPage.prefs = settingsPrefs.prefs!;
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
