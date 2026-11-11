// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiEnterpriseFeaturePrefName, AutofillManagerImpl, EntityDataManagerProxyImpl, EntityTypeName, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, SettingsAutofillPageV2Element} from 'chrome://settings/settings.js';
import {loadTimeData, MetricsBrowserProxyImpl, ModelExecutionEnterprisePolicyValue, OpenWindowProxyImpl, PasswordManagerImpl, PrefsBrowserProxy, PrefService, resetRouterForTesting, Router, YourSavedInfoDataCategory} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createAddressEntry, createCreditCardEntry, createIbanEntry, createPayOverTimeIssuerEntry, makeGuid, TestAutofillManager, TestPaymentsManager} from './autofill_fake_data.js';
import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

async function setupPrefs(): Promise<void> {
  const prefsBrowserProxy = new TestPrefsBrowserProxy([
    {
      key: 'signin.allowed_on_next_startup',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'autofill.profile_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: AiEnterpriseFeaturePrefName.AUTOFILL_AI,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW,
    },
    {
      key: 'autofill.autofill_ai.reauth_before_viewing_sensitive_data',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
  ]);
  PrefsBrowserProxy.setInstance(prefsBrowserProxy);
  PrefService.resetInstanceForTesting();
  await PrefService.getInstance().whenInitialized();
}

function createEntityInstanceWithLabels(typeName: EntityTypeName):
    chrome.autofillPrivate.EntityInstanceWithLabels {
  return {
    guid: makeGuid(),
    type: {
      typeName,
      typeNameAsString: `Entity ${typeName}`,
      addEntityTypeString: `Add ${typeName}`,
      editEntityTypeString: `Edit ${typeName}`,
      deleteEntityTypeString: `Delete ${typeName}`,
      supportsWalletStorage: false,
      passType: chrome.autofillPrivate.EntityPassType.PRIVATE_PASS,
    },
    entityInstanceLabel: `Label ${typeName}`,
    entityInstanceSubLabel: `SubLabel ${typeName}`,
    storedInWallet: false,
  };
}

suite('AutofillPageV2', function() {
  let autofillPage: SettingsAutofillPageV2Element;
  let autofillManager: TestAutofillManager;
  let entityDataManager: TestEntityDataManagerProxy;
  let paymentsManager: TestPaymentsManager;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    return setupPrefs();
  });

  function setupProxies() {
    autofillManager = new TestAutofillManager();
    AutofillManagerImpl.setInstance(autofillManager);
    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);
    PasswordManagerImpl.setInstance(new TestPasswordManagerProxy());
    paymentsManager = new TestPaymentsManager();
    PaymentsManagerImpl.setInstance(paymentsManager);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    OpenWindowProxyImpl.setInstance(new TestOpenWindowProxy());
  }

  async function setupPage(overrides: {[key: string]: boolean} = {}) {
    loadTimeData.overrideValues(overrides);
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    autofillPage = document.createElement('settings-autofill-page-v2');
    document.body.appendChild(autofillPage);
    await microtasksFinished();
  }

  setup(function() {
    setupProxies();
  });

  function getRowCount(rowSelector: string): string {
    const row =
        autofillPage.shadowRoot.querySelector<CrLinkRowElement>(rowSelector);
    assertTrue(!!row);
    const label = row.querySelector('span[slot="icon-label"]');
    assertTrue(!!label);
    return label.textContent;
  }

  test('CountersInitializeAndUpdate', async function() {
    // Populate initial data returned by proxy startup calls when the page
    // attaches (`getAddressList`, `getCreditCardList`, `getIbanList`,
    // `getPayOverTimeIssuerList`, and `loadEntityInstances`).
    autofillManager.data.addresses = [createAddressEntry()];
    paymentsManager.data.creditCards =
        [createCreditCardEntry(), createCreditCardEntry()];
    paymentsManager.data.ibans = [createIbanEntry()];
    paymentsManager.data.payOverTimeIssuers = [createPayOverTimeIssuerEntry()];
    entityDataManager.setLoadEntityInstancesResponse([
      // Identity docs (1 passport + 1 driver's license = 2).
      createEntityInstanceWithLabels(EntityTypeName.kPassport),
      createEntityInstanceWithLabels(EntityTypeName.kDriversLicense),
      // Travel (1 flight reservation + 1 vehicle = 2).
      createEntityInstanceWithLabels(EntityTypeName.kFlightReservation),
      createEntityInstanceWithLabels(EntityTypeName.kVehicle),
      // Shopping (1 order + 1 shipment = 2).
      createEntityInstanceWithLabels(EntityTypeName.kOrder),
      createEntityInstanceWithLabels(EntityTypeName.kShipment),
    ]);

    await setupPage({
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: true,
      shoppingIntegrationEnabled: true,
      showSuggestionsFromGeminiSettings: true,
    });
    await autofillManager.whenCalled('getAddressList');
    await paymentsManager.whenCalled('getCreditCardList');
    await paymentsManager.whenCalled('getIbanList');
    await paymentsManager.whenCalled('getPayOverTimeIssuerList');
    await entityDataManager.whenCalled('loadEntityInstances');

    // Payments sums 2 credit cards + 1 IBAN + 1 pay-over-time issuer = 4.
    assertEquals('4', getRowCount('#paymentManagerButton'));
    // Contact info reflects the 1 address from `getAddressList`.
    assertEquals('1', getRowCount('#addressesManagerButton'));
    // Autofill AI rows aggregate matching entity types from
    // `loadEntityInstances`.
    assertEquals('2', getRowCount('#identityManagerButton'));
    assertEquals('2', getRowCount('#travelManagerButton'));
    assertEquals('2', getRowCount('#shoppingManagerButton'));

    // Notify the page's personal data and Autofill AI listeners with updated
    // data so the row counters recompute without reloading the page.
    autofillManager.lastCallback.setPersonalDataManagerListener!(
        [createAddressEntry(), createAddressEntry()],
        [createCreditCardEntry()],
        [],
        [],
    );
    entityDataManager.callEntityInstancesChangedListener([
      createEntityInstanceWithLabels(EntityTypeName.kPassport),
    ]);
    await microtasksFinished();

    // Payments drops to 1 (1 credit card + 0 IBANs + 0 pay-over-time issuers)
    // and contact info rises to 2 from `setPersonalDataManagerListener`.
    assertEquals('1', getRowCount('#paymentManagerButton'));
    assertEquals('2', getRowCount('#addressesManagerButton'));
    // Identity docs updates to 1 (1 passport) while travel and shopping reset
    // to 0 because no matching entities remain in the updated list.
    assertEquals('1', getRowCount('#identityManagerButton'));
    assertEquals('0', getRowCount('#travelManagerButton'));
    assertEquals('0', getRowCount('#shoppingManagerButton'));
  });

  test('OffersAndLoyaltyCardsRowHiddenWhenFlagDisabled', async function() {
    await setupPage({offersAndLoyaltyCardsSettingsEnabled: false});
    assertFalse(!!autofillPage.shadowRoot.querySelector(
        '#offersAndLoyaltyCardsManagerButton'));
  });

  test('OffersAndLoyaltyCardsRowNavigates', async function() {
    await setupPage({offersAndLoyaltyCardsSettingsEnabled: true});
    const row = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#offersAndLoyaltyCardsManagerButton');
    assertTrue(!!row);
    assertEquals(
        loadTimeData.getString('offersAndLoyaltyCardsCardTitle'), row.label);
    assertEquals(
        loadTimeData.getString('offersAndLoyaltyCardsSublabel'), row.subLabel);

    row.click();

    assertEquals(
        '/offersAndLoyaltyCards', Router.getInstance().currentRoute.path);
    const [category] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoCategoryClick');
    assertEquals(YourSavedInfoDataCategory.OFFERS_AND_LOYALTY_CARDS, category);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'Settings.YourSavedInfo.CategoryClick.OFFERS_AND_LOYALTY_CARDS',
        action);
  });
});
