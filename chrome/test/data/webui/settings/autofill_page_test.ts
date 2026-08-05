// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiEnterpriseFeaturePrefName, AutofillManagerImpl, EntityDataManagerProxyImpl, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsAutofillPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage, resetRouterForTesting, Router, YourSavedInfoDataCategory, YourSavedInfoDataChip, YourSavedInfoRelatedService} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';


import {createAddressEntry, createCreditCardEntry, createIbanEntry, createPayOverTimeIssuerEntry, TestAutofillManager, TestPaymentsManager} from './autofill_fake_data.js';
import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

function setDefaultPrefs(objectToSetup: SettingsPrefsElement) {
  objectToSetup.set(
      `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
      ModelExecutionEnterprisePolicyValue.ALLOW);
  objectToSetup.set(
      'prefs.optimization_guide.model_execution.autofill_prediction_improvements_enterprise_policy_allowed.value',
      ModelExecutionEnterprisePolicyValue.ALLOW);
}

suite('AutofillPage', function() {
  let autofillPage: SettingsAutofillPageElement;
  let autofillManager: TestAutofillManager;
  let passwordManager: TestPasswordManagerProxy;
  let paymentsManager: TestPaymentsManager;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
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
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    await setupPage({
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: true,
      shoppingIntegrationEnabled: true,
      showSuggestionsFromGeminiSettings: true,
    });
  });

  async function setupPage(overrides: {[key: string]: boolean}) {
    loadTimeData.overrideValues(overrides);
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    autofillPage = document.createElement('settings-autofill-page');
    setDefaultPrefs(settingsPrefs);
    autofillPage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(autofillPage);
    await flushTasks();
  }

  function getChipCount(chipLabel: string): number|undefined {
    const cards = autofillPage.shadowRoot!.querySelectorAll(
        'category-reference-card');
    for (const card of cards) {
      const chips = card.shadowRoot!.querySelectorAll('cr-button');
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
    const autofillPageTitleElement =
        autofillPage.shadowRoot!.querySelector('#autofillPageTitle');
    assertTrue(!!autofillPageTitleElement);
  });

  test('ShoppingCategoryHiddenWhenFlagDisabled', async function() {
    await setupPage({
      shoppingIntegrationEnabled: false,
    });

    const shoppingCard =
        autofillPage.shadowRoot!.querySelector<HTMLElement>(
            '#shoppingManagerButton');
    assertFalse(!!shoppingCard);
  });

  test('CardsRenderCorrectly', function() {
    const cards = autofillPage.shadowRoot!.querySelectorAll(
        'category-reference-card');
    const expectedCardTitles = [
      loadTimeData.getString('localPasswordManager'),
      loadTimeData.getString('paymentsTitle'),
      loadTimeData.getString('contactInfoTitle'),
      loadTimeData.getString('identityDocsCardTitle'),
      loadTimeData.getString('travelCardTitle'),
      loadTimeData.getString('shoppingCardTitle'),
    ];

    assertEquals(expectedCardTitles.length, cards.length);
    for (let i = 0; i < expectedCardTitles.length; i++) {
      assertEquals(expectedCardTitles[i], cards[i]!.cardTitle);
    }
  });

  test('passwordsCardOpensPasswordManager', async function() {
    const passwordsCard =
        autofillPage.shadowRoot!.querySelector<HTMLElement>(`
        category-reference-card[card-title="${
            loadTimeData.getString('localPasswordManager')}"]`);
    assertTrue(!!passwordsCard);

    passwordsCard.shadowRoot!.querySelector<HTMLElement>(
                                 'cr-link-row')!.click();

    const page = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.PASSWORDS, page);
    const [category] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoCategoryClick');
    assertEquals(YourSavedInfoDataCategory.PASSWORD_MANAGER, category);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'Settings.YourSavedInfo.CategoryClick.PASSWORD_MANAGER', action);
  });

  // Do not use route constants (like `routes.PAYMENTS`) as expectedRoute
  // values. The `expectedRoute` is calculated and cached before `setup()` or
  // `suiteSetup()` when the `yourSavedInfo` feature flag is disabled, which
  // results in some path values being undefined. Instead, use the literal
  // string path, e.g., use `'/payments'` instead of `routes.PAYMENTS`.
  [{
    cardTitle: 'paymentsTitle',
    expectedRoute: '/payments',
    expectedCategory: YourSavedInfoDataCategory.PAYMENTS,
  },
   {
     cardTitle: 'contactInfoTitle',
     expectedRoute: '/contactInfo',
     expectedCategory: YourSavedInfoDataCategory.CONTACT_INFO,
   },
   {
     cardTitle: 'identityDocsCardTitle',
     expectedRoute: '/identityDocs',
     expectedCategory: YourSavedInfoDataCategory.IDENTITY_DOCS,
   },
   {
     cardTitle: 'travelCardTitle',
     expectedRoute: '/travel',
     expectedCategory: YourSavedInfoDataCategory.TRAVEL,
   },
   {
     cardTitle: 'shoppingCardTitle',
     expectedRoute: '/shopping',
     expectedCategory: YourSavedInfoDataCategory.SHOPPING,
   },
  ].forEach(({cardTitle, expectedRoute, expectedCategory}) => {
    test(`${cardTitle} card navigates to the correct route`, async function() {
      const card = autofillPage.shadowRoot!.querySelector<HTMLElement>(
          `category-reference-card[card-title="${
              loadTimeData.getString(cardTitle)}"]`);
      assertTrue(!!card);

      card.shadowRoot!.querySelector('cr-link-row')!.click();
      assertEquals(expectedRoute, Router.getInstance().currentRoute.path);
      const [category] = await metricsBrowserProxy.whenCalled(
          'recordYourSavedInfoCategoryClick');
      assertEquals(expectedCategory, category);
      const action = await metricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          `Settings.YourSavedInfo.CategoryClick.${
              YourSavedInfoDataCategory[expectedCategory]}`,
          action);
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

  test('ClickOnChipNavigatesToLeafPage', async function() {
    const card = autofillPage.shadowRoot!.querySelector<HTMLElement>(
        `category-reference-card[card-title="${
            loadTimeData.getString('contactInfoTitle')}"]`);
    assertTrue(!!card);
    const chips: HTMLElement[] =
        Array.from(card.shadowRoot!.querySelectorAll('cr-button'));
    const chip: HTMLElement = chips.find(chip => {
      const labelSpan = chip.querySelector('span:not(.counter)');
      return labelSpan &&
          labelSpan.textContent === loadTimeData.getString('addresses');
    })!;

    chip.click();
    assertEquals('/contactInfo', Router.getInstance().currentRoute.path);
    const [metricChip] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoDataChipClick');
    assertEquals(YourSavedInfoDataChip.ADDRESSES, metricChip);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals('Settings.YourSavedInfo.ChipClick.ADDRESSES', action);
  });

  test('ClickOnShoppingChipNavigatesToLeafPage', async function() {
    const card = autofillPage.shadowRoot!.querySelector<HTMLElement>(
        `category-reference-card[card-title="${
            loadTimeData.getString('shoppingCardTitle')}"]`);
    assertTrue(!!card);
    const chips: HTMLElement[] =
        Array.from(card.shadowRoot!.querySelectorAll('cr-button'));
    const chip: HTMLElement = chips.find(chip => {
      const labelSpan = chip.querySelector('span:not(.counter)');
      return labelSpan &&
          labelSpan.textContent ===
          loadTimeData.getString('yourSavedInfoOrdersChip');
    })!;

    chip.click();
    assertEquals('/shopping', Router.getInstance().currentRoute.path);
    const [metricChip] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoDataChipClick');
    assertEquals(YourSavedInfoDataChip.ORDERS, metricChip);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals('Settings.YourSavedInfo.ChipClick.ORDERS', action);
  });

  test('SuggestionsFromGeminiHiddenWhenFlagDisabled', async function() {
    await setupPage({
      showSuggestionsFromGeminiSettings: false,
    });

    const geminiCard = autofillPage.shadowRoot!.querySelector<HTMLElement>(
        '#suggestionsFromGeminiCard');
    assertFalse(!!geminiCard);
  });

  test('SuggestionsFromGeminiCardNavigates', async function() {
    const geminiCard = autofillPage.shadowRoot!.querySelector<HTMLElement>(
        '#suggestionsFromGeminiCard');
    assertTrue(!!geminiCard);

    const button = autofillPage.shadowRoot!.querySelector<HTMLElement>(
        '#suggestionsFromGeminiLinkRow');
    assertTrue(!!button);

    const icon = autofillPage.shadowRoot!.querySelector<CrIconElement>(
        '#suggestionsFromGeminiSubLabel cr-icon');
    assertTrue(!!icon);
    // <if expr="_google_chrome">
    assertEquals(
        loadTimeData.getBoolean('glicAssetsV2Enabled') ?
            'settings-internal:sparkv2' :
            'settings-internal:spark',
        icon.icon);
    // </if>
    // <if expr="not _google_chrome">
    assertEquals('settings20:lightbulb', icon.icon);
    // </if>

    button.click();
    assertEquals('/enhancedAutofill', Router.getInstance().currentRoute.path);

    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'PersonalContext.Settings.EntryPoint.AutofillAndPasswordsSettings',
        action);
  });
});

suite('DataChipsVisibility', function() {
  let settingsPrefs: SettingsPrefsElement;
  let entityDataManager: TestEntityDataManagerProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    entityDataManager = new TestEntityDataManagerProxy();
    entityDataManager.setGetWritableEntityTypesResponse([
      {
        typeName: 0,
        typeNameAsString: 'Passport',
        addEntityTypeString: 'Add passport',
        editEntityTypeString: 'Edit passport',
        deleteEntityTypeString: 'Delete passport',
        supportsWalletStorage: false,
        passType: chrome.autofillPrivate.EntityPassType.PRIVATE_PASS,
      },
      {
        typeName: 1,
        typeNameAsString: 'Driver\'s license',
        addEntityTypeString: 'Add driver\'s license',
        editEntityTypeString: 'Edit driver\'s license',
        deleteEntityTypeString: 'Delete driver\'s license',
        supportsWalletStorage: false,
        passType: chrome.autofillPrivate.EntityPassType.PRIVATE_PASS,
      },
      {
        typeName: 2,
        typeNameAsString: 'Vehicle',
        addEntityTypeString: 'Add vehicle',
        editEntityTypeString: 'Edit vehicle',
        deleteEntityTypeString: 'Delete vehicle',
        supportsWalletStorage: false,
        passType: chrome.autofillPrivate.EntityPassType.PUBLIC_PASS,
      },
    ]);
    EntityDataManagerProxyImpl.setInstance(entityDataManager);
  });

  async function setupPage(overrides: {[key: string]: boolean}):
      Promise<SettingsAutofillPageElement> {
    loadTimeData.overrideValues(overrides);
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const autofillPage: SettingsAutofillPageElement =
        document.createElement('settings-autofill-page');
    setDefaultPrefs(settingsPrefs);
    autofillPage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(autofillPage);
    await flushTasks();
    return autofillPage;
  }

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  function getChipLabels(
      autofillPage: SettingsAutofillPageElement,
      cardSelector: string): string[] {
    const card =
        autofillPage.shadowRoot!.querySelector<HTMLElement>(cardSelector);
    assertTrue(!!card);
    const chips: HTMLElement[] =
        Array.from(card.shadowRoot!.querySelectorAll('cr-button'));
    return chips.map(chip => chip.querySelector('span')!.textContent);
  }

  test('AllChipsVisible', async function() {
    const autofillPage = await setupPage({
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: true,
      shoppingIntegrationEnabled: true,
    });
    await entityDataManager.whenCalled('getWritableEntityTypes');

    assertDeepEquals(
        [
          loadTimeData.getString('creditAndDebitCardTitle'),
          loadTimeData.getString('ibanTitle'),
          loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
          loadTimeData.getString('loyaltyCardsTitle'),
        ],
        getChipLabels(autofillPage, '#paymentManagerButton'));

    assertTrue(
        isChildVisible(autofillPage, '#identityManagerButton'),
        'Identity docs category should be visible');
    assertDeepEquals(
        [
          loadTimeData.getString('yourSavedInfoDriverLicenseChip'),
          loadTimeData.getString('yourSavedInfoPassportChip'),
        ],
        getChipLabels(autofillPage, '#identityManagerButton'));

    assertTrue(
        isChildVisible(autofillPage, '#travelManagerButton'),
        'Travel category should be visible');
    assertDeepEquals(
        [
          loadTimeData.getString('yourSavedInfoVehiclesChip'),
        ],
        getChipLabels(autofillPage, '#travelManagerButton'));

    assertTrue(
        isChildVisible(autofillPage, '#shoppingManagerButton'),
        'Shopping category should be visible');
    assertDeepEquals(
        [
          loadTimeData.getString('yourSavedInfoOrdersChip'),
          loadTimeData.getString('yourSavedInfoShipmentsChip'),
        ],
        getChipLabels(autofillPage, '#shoppingManagerButton'));
  });

  test('DisabledIbans', async function() {
    const autofillPage = await setupPage({
      showIbansSettings: false,
      shouldShowPayOverTimeSettings: true,
    });
    assertDeepEquals(
        [
          loadTimeData.getString('creditAndDebitCardTitle'),
          loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
          loadTimeData.getString('loyaltyCardsTitle'),
        ],
        getChipLabels(autofillPage, '#paymentManagerButton'));
  });

  test('DisabledIbansButAlreadyExisting', async function() {
    const autofillManager = new TestAutofillManager();
    AutofillManagerImpl.setInstance(autofillManager);
    const autofillPage = await setupPage({
      showIbansSettings: false,
      shouldShowPayOverTimeSettings: true,
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
        getChipLabels(autofillPage, '#paymentManagerButton'));
  });

  test('DisabledPayOverTime', async function() {
    // Disable Pay over time
    const autofillPage = await setupPage({
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: false,
    });
    assertDeepEquals(
        [
          loadTimeData.getString('creditAndDebitCardTitle'),
          loadTimeData.getString('ibanTitle'),
          loadTimeData.getString('loyaltyCardsTitle'),
        ],
        getChipLabels(autofillPage, '#paymentManagerButton'));
  });

  test('DisabledAutofillAi', async function() {
    const autofillPage = await setupPage({});
    assertTrue(
        isChildVisible(autofillPage, '#identityManagerButton'),
        'Identity docs category should be visible');
    assertTrue(
        isChildVisible(autofillPage, '#travelManagerButton'),
        'Travel category should be visible');
    assertTrue(
        isChildVisible(autofillPage, '#shoppingManagerButton'),
        'Shopping category should be visible');
  });

  test('DisabledAmbientAutofill', async function() {
    const autofillPage = await setupPage({
      shoppingIntegrationEnabled: false,
    });
    assertFalse(
        isChildVisible(autofillPage, '#shoppingManagerButton'),
        'Shopping category should not be visible');
  });

  test('UnsupportedAutofillAiDataTypeWithExistingItems', async function() {
    // National ID card type is not supported, but user has an existing item
    // already.
    const testEntityInstancesWithLabels:
        chrome.autofillPrivate.EntityInstanceWithLabels[] = [
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        type: {
          typeName: 3,
          typeNameAsString: 'National ID card',
          addEntityTypeString: 'Add ID',
          editEntityTypeString: 'Edit ID',
          deleteEntityTypeString: 'Delete ID',
          supportsWalletStorage: false,
          passType: chrome.autofillPrivate.EntityPassType.PRIVATE_PASS,
        },
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'ID card',
        storedInWallet: false,
      },
    ];
    entityDataManager.setLoadEntityInstancesResponse(
        testEntityInstancesWithLabels);
    const autofillPage = await setupPage({});
    await entityDataManager.whenCalled('loadEntityInstances');

    assertTrue(
        isChildVisible(autofillPage, '#identityManagerButton'),
        'Identity docs category should be visible');
    assertDeepEquals(
        [
          loadTimeData.getString('yourSavedInfoDriverLicenseChip'),
          loadTimeData.getString('yourSavedInfoNationalIdsChip'),
          loadTimeData.getString('yourSavedInfoPassportChip'),
        ],
        getChipLabels(autofillPage, '#identityManagerButton'),
        'Extra national ID cards chip should be visible');
    assertTrue(
        isChildVisible(autofillPage, '#travelManagerButton'),
        'Travel category should be visible');
    assertDeepEquals(
        [
          loadTimeData.getString('yourSavedInfoVehiclesChip'),
        ],
        getChipLabels(autofillPage, '#travelManagerButton'));
  });
});

suite('RelatedServices', function() {
  let autofillPage: SettingsAutofillPageElement;
  let openWindowProxy: TestOpenWindowProxy;
  let passwordManager: TestPasswordManagerProxy;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

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
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    autofillPage = document.createElement('settings-autofill-page');
    setDefaultPrefs(settingsPrefs);
    autofillPage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(autofillPage);
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  async function testRowOpensUrl(selector: string, urlStringId: string) {
    const row =
        autofillPage.shadowRoot!.querySelector<HTMLElement>(selector);
    assertTrue(!!row);
    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString(urlStringId), url);
  }

  test('CardRendersCorrectly', function() {
    const relatedServicesCard =
        autofillPage.shadowRoot!.querySelector<HTMLElement>(
            `settings-section[page-title="${
                loadTimeData.getString(
                    'yourSavedInfoRelatedServicesTitle')}"]`);
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
        autofillPage.shadowRoot!.querySelector<HTMLElement>(
            '#passwordManagerButton');
    assertTrue(!!passwordManagerRow);
    passwordManagerRow.click();
    const page = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.PASSWORDS, page);
    const [service] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoRelatedServiceClick');
    assertEquals(YourSavedInfoRelatedService.GOOGLE_PASSWORD_MANAGER, service);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'Settings.YourSavedInfo.RelatedServiceClick.GOOGLE_PASSWORD_MANAGER',
        action);
  });

  test('WalletRowOpensWallet', async function() {
    await testRowOpensUrl('#googleWalletButton', 'googleWalletUrl');
    const [service] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoRelatedServiceClick');
    assertEquals(YourSavedInfoRelatedService.GOOGLE_WALLET, service);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'Settings.YourSavedInfo.RelatedServiceClick.GOOGLE_WALLET', action);
  });

  test('ProfileRowOpensProfile', async function() {
    await testRowOpensUrl('#googleAccountButton', 'googleAccountUrl');
    const [service] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoRelatedServiceClick');
    assertEquals(YourSavedInfoRelatedService.GOOGLE_ACCOUNT, service);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'Settings.YourSavedInfo.RelatedServiceClick.GOOGLE_ACCOUNT', action);
  });
});
