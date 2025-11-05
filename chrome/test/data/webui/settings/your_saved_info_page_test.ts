// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiEnterpriseFeaturePrefName, AutofillManagerImpl, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement, SettingsYourSavedInfoPageElement} from 'chrome://settings/settings.js';
import {loadTimeData, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage, resetRouterForTesting, Router} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

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
    // Override for testing.
    autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [createAddressEntry()];
    AutofillManagerImpl.setInstance(autofillManager);
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    paymentsManager = new TestPaymentsManager();
    PaymentsManagerImpl.setInstance(paymentsManager);

    await setupPage({
      enableYourSavedInfoSettingsPage: true,
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: true,
      enableLoyaltyCardsFilling: true,
      showAutofillAiControl: true,
    });
  });

  async function setupPage(overrides: {[key: string]: boolean}) {
    loadTimeData.overrideValues(overrides);
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    yourSavedInfoPage = document.createElement('settings-your-saved-info-page');
    setDefaultPrefs(settingsPrefs);
    yourSavedInfoPage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(yourSavedInfoPage);
    await flushTasks();
  }

  function getChipCount(chipLabel: string): number|undefined {
    const cards = yourSavedInfoPage.shadowRoot!.querySelectorAll(
        'category-reference-card');
    for (const card of cards) {
      const chips = card.shadowRoot!.querySelectorAll('cr-chip');
      for (const chip of chips) {
        const labelSpan = chip.querySelector('span:not(.counter)');
        if (labelSpan && labelSpan.textContent === chipLabel) {
          const counter = chip.querySelector<HTMLElement>('.counter')!;
          if (counter.hidden) {
            return undefined;
          }
          const match = counter.textContent.match(/\((\d+)\)/);
          return match ? +match[1]! : undefined;
        }
      }
    }
    return undefined;
  }

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

  // Do not use route constants (like `routes.PAYMENTS`) as expectedRoute
  // values. The `expectedRoute` is calculated and cached before `setup()` or
  // `suiteSetup()` when the `yourSavedInfo` feature flag is disabled, which
  // results in some path values being undefined. Instead, use the literal
  // string path, e.g., use `'/payments'` instead of `routes.PAYMENTS`.
  [{cardTitle: 'paymentsTitle', expectedRoute: '/payments'},
   {cardTitle: 'contactInfoTitle', expectedRoute: '/addresses'},
   {cardTitle: 'identityDocsCardTitle', expectedRoute: '/identityDocs'},
   {cardTitle: 'travelCardTitle', expectedRoute: '/travel'},
  ].forEach(({cardTitle, expectedRoute}) => {
    test(`${cardTitle} card navigates to the correct route`, function() {
      const card = yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
          `category-reference-card[card-title="${
              loadTimeData.getString(cardTitle)}"]`);
      assertTrue(!!card);

      card.shadowRoot!.querySelector('cr-link-row')!.click();
      assertEquals(expectedRoute, Router.getInstance().currentRoute.path);
    });
  });

  test('AddressesAndPaymentsCountersAreUpdated', async function() {
    await autofillManager.whenCalled('getAddressList');
    await paymentsManager.whenCalled('getCreditCardList');
    await paymentsManager.whenCalled('getIbanList');
    await paymentsManager.whenCalled('getPayOverTimeIssuerList');

    assertEquals(1, getChipCount(loadTimeData.getString('addresses')));
    assertEquals(
        undefined,
        getChipCount(loadTimeData.getString('creditAndDebitCardTitle')));
    assertEquals(undefined, getChipCount(loadTimeData.getString('ibanTitle')));
    assertEquals(
        undefined,
        getChipCount(
            loadTimeData.getString('autofillPayOverTimeSettingsLabel')));

    const addressList = [createAddressEntry(), createAddressEntry()];
    const cardList = [createCreditCardEntry()];
    const ibanList = [createIbanEntry(), createIbanEntry(), createIbanEntry()];
    const payOverTimeIssuerList = [createPayOverTimeIssuerEntry()];
    autofillManager.lastCallback.setPersonalDataManagerListener!
        (addressList, cardList, ibanList, payOverTimeIssuerList);
    await flushTasks();

    assertEquals(2, getChipCount(loadTimeData.getString('addresses')));
    assertEquals(
        1, getChipCount(loadTimeData.getString('creditAndDebitCardTitle')));
    assertEquals(3, getChipCount(loadTimeData.getString('ibanTitle')));
    assertEquals(
        1,
        getChipCount(
            loadTimeData.getString('autofillPayOverTimeSettingsLabel')));
  });
});

suite('DataChipsVisibility', function() {
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  async function setupPage(overrides: {[key: string]: boolean}):
      Promise<SettingsYourSavedInfoPageElement> {
    loadTimeData.overrideValues(overrides);
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const yourSavedInfoPage: SettingsYourSavedInfoPageElement =
        document.createElement('settings-your-saved-info-page');
    setDefaultPrefs(settingsPrefs);
    yourSavedInfoPage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(yourSavedInfoPage);
    await flushTasks();
    return yourSavedInfoPage;
  }

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  function getChipLabels(
      yourSavedInfoPage: SettingsYourSavedInfoPageElement,
      cardSelector: string): string[] {
    const card =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(cardSelector);
    assertTrue(!!card);
    const chips: HTMLElement[] =
        Array.from(card.shadowRoot!.querySelectorAll('cr-chip'));
    return chips.map(chip => chip.querySelector('span')!.textContent);
  }

  test('AllChipsVisible', async function() {
    const yourSavedInfoPage = await setupPage({
      enableYourSavedInfoSettingsPage: true,
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: true,
      enableLoyaltyCardsFilling: true,
      showAutofillAiControl: true,
    });

    assertDeepEquals(
        [
          loadTimeData.getString('creditAndDebitCardTitle'),
          loadTimeData.getString('ibanTitle'),
          loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
          loadTimeData.getString('loyaltyCardsTitle'),
        ],
        getChipLabels(yourSavedInfoPage, '#paymentManagerButton'));

    assertTrue(isChildVisible(yourSavedInfoPage, '#identityManagerButton'));
    assertTrue(isChildVisible(yourSavedInfoPage, '#travelManagerButton'));
  });

  test('DisabledIbans', async function() {
    const yourSavedInfoPage = await setupPage({
      showIbansSettings: false,
      shouldShowPayOverTimeSettings: true,
      enableLoyaltyCardsFilling: true,
    });
    assertDeepEquals(
        [
          loadTimeData.getString('creditAndDebitCardTitle'),
          loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
          loadTimeData.getString('loyaltyCardsTitle'),
        ],
        getChipLabels(yourSavedInfoPage, '#paymentManagerButton'));
  });

  test('DisabledIbansButAlreadyExisting', async function() {
    const autofillManager = new TestAutofillManager();
    AutofillManagerImpl.setInstance(autofillManager);
    const yourSavedInfoPage = await setupPage({
      showIbansSettings: false,
      shouldShowPayOverTimeSettings: true,
      enableLoyaltyCardsFilling: true,
    });
    autofillManager.lastCallback.setPersonalDataManagerListener!
        ([], [], [createIbanEntry()], []);
    await flushTasks();

    assertDeepEquals(
        [
          loadTimeData.getString('creditAndDebitCardTitle'),
          loadTimeData.getString('ibanTitle'),
          loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
          loadTimeData.getString('loyaltyCardsTitle'),
        ],
        getChipLabels(yourSavedInfoPage, '#paymentManagerButton'));
  });

  test('DisabledPayOverTime', async function() {
    // Disable Pay over time
    const yourSavedInfoPage = await setupPage({
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: false,
      enableLoyaltyCardsFilling: true,
    });
    assertDeepEquals(
        [
          loadTimeData.getString('creditAndDebitCardTitle'),
          loadTimeData.getString('ibanTitle'),
          loadTimeData.getString('loyaltyCardsTitle'),
        ],
        getChipLabels(yourSavedInfoPage, '#paymentManagerButton'));
  });

  test('DisabledAutofillAi', async function() {
    const yourSavedInfoPage = await setupPage({
      showAutofillAiControl: false,
    });
    const identityCard =
        yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
            '#identityManagerButton');
    assertTrue(!!identityCard);
    assertTrue(identityCard.hidden);
    const travelCard = yourSavedInfoPage.shadowRoot!.querySelector<HTMLElement>(
        '#travelManagerButton');
    assertTrue(!!travelCard);
    assertTrue(travelCard.hidden);
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
    resetRouterForTesting();

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

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
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
