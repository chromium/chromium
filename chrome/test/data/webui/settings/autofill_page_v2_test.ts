// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {AiEnterpriseFeaturePrefName, AutofillManagerImpl, EntityDataManagerProxyImpl, EntityTypeName, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, PasswordCount, SettingsAutofillPageV2Element, SettingsSectionElement} from 'chrome://settings/settings.js';
import {DataManagementSurvey, loadTimeData, MetricsBrowserProxyImpl, ModelExecutionEnterprisePolicyValue, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage, PrefsBrowserProxy, PrefService, resetRouterForTesting, Router, SavedInfoHandlerImpl, SuggestionsFromGeminiEntryPoint, YourSavedInfoDataCategory, YourSavedInfoRelatedService} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createAddressEntry, createCreditCardEntry, createIbanEntry, createLoyaltyCardEntry, createPayOverTimeIssuerEntry, makeGuid, TestAutofillManager, TestPaymentsManager} from './autofill_fake_data.js';
import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSavedInfoHandlerProxy} from './test_saved_info_handler_proxy.js';

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
  let passwordManager: TestPasswordManagerProxy;
  let paymentsManager: TestPaymentsManager;
  let savedInfoHandler: TestSavedInfoHandlerProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  const initialGlicAssetsV2Enabled =
      loadTimeData.getBoolean('glicAssetsV2Enabled');

  suiteSetup(function() {
    return setupPrefs();
  });

  function setupProxies() {
    autofillManager = new TestAutofillManager();
    AutofillManagerImpl.setInstance(autofillManager);
    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    paymentsManager = new TestPaymentsManager();
    PaymentsManagerImpl.setInstance(paymentsManager);
    savedInfoHandler = new TestSavedInfoHandlerProxy();
    SavedInfoHandlerImpl.setInstance(savedInfoHandler);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  }

  async function setupPage(overrides: {[key: string]: boolean|string} = {}) {
    loadTimeData.overrideValues(overrides);
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    autofillPage = document.createElement('settings-autofill-page-v2');
    document.body.appendChild(autofillPage);
    await microtasksFinished();
  }

  setup(async function() {
    setupProxies();
    await setupPage({
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: true,
      shoppingIntegrationEnabled: true,
      showSuggestionsFromGeminiSettings: true,
      offersAndLoyaltyCardsSettingsEnabled: false,
      glicAssetsV2Enabled: initialGlicAssetsV2Enabled,
    });
  });

  function getPasswordManagerRow(): CrLinkRowElement|null {
    const firstSection =
        autofillPage.shadowRoot.querySelector('settings-section');
    assertTrue(!!firstSection);
    return firstSection.querySelector('cr-link-row');
  }

  function getRowCount(row: CrLinkRowElement|null): string {
    assertTrue(!!row);
    const label = row.querySelector('span[slot="icon-label"]');
    assertTrue(!!label);
    return label.textContent;
  }

  function getRowCountBySelector(rowSelector: string): string {
    return getRowCount(
        autofillPage.shadowRoot.querySelector<CrLinkRowElement>(rowSelector));
  }

  test('SectionsRenderCorrectly', function() {
    const sections =
        autofillPage.shadowRoot.querySelectorAll<SettingsSectionElement>(
            'settings-section');
    assertEquals(3, sections.length);
    assertEquals(
        loadTimeData.getString('autofillPageTitle'), sections[0]!.pageTitle);
    assertEquals('', sections[1]!.pageTitle);
    assertTrue(
        !!sections[1]!.querySelector('collapsible-autofill-settings-card'));
    assertEquals(
        loadTimeData.getString('yourSavedInfoRelatedServicesTitle'),
        sections[2]!.pageTitle);

    assertFalse(
        !!autofillPage.shadowRoot.querySelector('settings-account-card'));
    assertFalse(
        !!autofillPage.shadowRoot.querySelector('category-reference-card'));
  });

  test('CategoryRowsRenderCorrectly', function() {
    const firstSection =
        autofillPage.shadowRoot.querySelector('settings-section');
    assertTrue(!!firstSection);
    const rows = firstSection.querySelectorAll('cr-link-row');

    const expectedRows = [
      {
        label: loadTimeData.getString('localPasswordManager'),
        subLabel: loadTimeData.getString('passwordManagerSublabel'),
        startIcon: 'cr20:password-manager',
        external: true,
      },
      {
        label: loadTimeData.getString('paymentsTitle'),
        subLabel: loadTimeData.getString('paymentsSublabel'),
        startIcon: 'settings20:credit-card',
        external: false,
      },
      {
        label: loadTimeData.getString('addresses'),
        subLabel: loadTimeData.getString('contactInfoSublabel'),
        startIcon: 'settings20:location-on',
        external: false,
      },
      {
        label: loadTimeData.getString('identityDocsCardTitle'),
        subLabel: loadTimeData.getString('identityDocsSublabel'),
        startIcon: 'settings20:id-card',
        external: false,
      },
      {
        label: loadTimeData.getString('travelCardTitle'),
        subLabel: loadTimeData.getString('travelSublabel'),
        startIcon: 'settings20:travel',
        external: false,
      },
      {
        label: loadTimeData.getString('shoppingCardTitle'),
        subLabel: loadTimeData.getString('shoppingSublabel'),
        startIcon: 'settings20:shopping-bag',
        external: false,
      },
      {
        label: loadTimeData.getString('autofillPersonalContextSettingsTitle'),
        subLabel:
            loadTimeData.getString('autofillPersonalContextSettingsSummary'),
        // <if expr="_google_chrome">
        startIcon: initialGlicAssetsV2Enabled ? 'settings-internal:sparkv2' :
                                                'settings-internal:spark',
        // </if>
        // <if expr="not _google_chrome">
        startIcon: 'settings20:lightbulb',
        // </if>
        external: false,
      },
    ];

    assertEquals(expectedRows.length, rows.length);
    for (let i = 0; i < expectedRows.length; i++) {
      assertEquals(expectedRows[i]!.label, rows[i]!.label);
      assertEquals(expectedRows[i]!.subLabel, rows[i]!.subLabel);
      assertEquals(expectedRows[i]!.startIcon, rows[i]!.startIcon);
      assertEquals(expectedRows[i]!.external, rows[i]!.external);
    }
  });

  test('ShoppingRowHiddenWhenFlagDisabled', async function() {
    await setupPage({
      shoppingIntegrationEnabled: false,
    });

    assertFalse(
        !!autofillPage.shadowRoot.querySelector('#shoppingManagerButton'));
  });

  test('SuggestionsFromGeminiHiddenWhenFlagDisabled', async function() {
    await setupPage({
      showSuggestionsFromGeminiSettings: false,
    });

    assertFalse(!!autofillPage.shadowRoot.querySelector(
        '#suggestionsFromGeminiLinkRow'));
  });

  // <if expr="_google_chrome">
  test('SuggestionsFromGeminiIconReflectsGlicAssetsV2Flag', async function() {
    await setupPage({
      showSuggestionsFromGeminiSettings: true,
      glicAssetsV2Enabled: false,
    });
    let row = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#suggestionsFromGeminiLinkRow');
    assertTrue(!!row);
    assertEquals('settings-internal:spark', row.startIcon);

    await setupPage({
      showSuggestionsFromGeminiSettings: true,
      glicAssetsV2Enabled: true,
    });
    row = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#suggestionsFromGeminiLinkRow');
    assertTrue(!!row);
    assertEquals('settings-internal:sparkv2', row.startIcon);
  });
  // </if>

  test('PasswordsRowOpensPasswordManager', async function() {
    const passwordsRow = getPasswordManagerRow();
    assertTrue(!!passwordsRow);

    passwordsRow.click();
    await passwordManager.whenCalled('recordPasswordsPageAccessInSettings');
    const page = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.PASSWORDS, page);
    const [survey, isFromHomePage] =
        await savedInfoHandler.whenCalled('requestDataManagementSurvey');
    assertEquals(DataManagementSurvey.PASSWORDS, survey);
    assertTrue(isFromHomePage);
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
    rowSelector: '#paymentManagerButton',
    expectedRoute: '/payments',
    expectedCategory: YourSavedInfoDataCategory.PAYMENTS,
  },
   {
     rowSelector: '#addressesManagerButton',
     expectedRoute: '/contactInfo',
     expectedCategory: YourSavedInfoDataCategory.CONTACT_INFO,
   },
   {
     rowSelector: '#identityManagerButton',
     expectedRoute: '/identityDocs',
     expectedCategory: YourSavedInfoDataCategory.IDENTITY_DOCS,
   },
   {
     rowSelector: '#travelManagerButton',
     expectedRoute: '/travel',
     expectedCategory: YourSavedInfoDataCategory.TRAVEL,
   },
   {
     rowSelector: '#shoppingManagerButton',
     expectedRoute: '/shopping',
     expectedCategory: YourSavedInfoDataCategory.SHOPPING,
   },
  ].forEach(({rowSelector, expectedRoute, expectedCategory}) => {
    test(`${rowSelector} navigates to ${expectedRoute}`, async function() {
      const row =
          autofillPage.shadowRoot.querySelector<HTMLElement>(rowSelector);
      assertTrue(!!row);

      row.click();
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

  test('SuggestionsFromGeminiRowNavigates', async function() {
    const row = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#suggestionsFromGeminiLinkRow');
    assertTrue(!!row);
    assertEquals(
        loadTimeData.getString('autofillPersonalContextSettingsSummary'),
        row.subLabel);

    row.click();
    assertEquals('/enhancedAutofill', Router.getInstance().currentRoute.path);
    const entryPoint = await metricsBrowserProxy.whenCalled(
        'recordSuggestionsFromGeminiEntryPointClick');
    assertEquals(SuggestionsFromGeminiEntryPoint.YOUR_SAVED_INFO, entryPoint);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'PersonalContext.Settings.EntryPoint.AutofillAndPasswordsSettings',
        action);
  });

  test('CountersInitializeAndUpdate', async function() {
    setupProxies();
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
    assertEquals('4', getRowCountBySelector('#paymentManagerButton'));
    // Contact info reflects the 1 address from `getAddressList`.
    assertEquals('1', getRowCountBySelector('#addressesManagerButton'));
    // Autofill AI rows aggregate matching entity types from
    // `loadEntityInstances`.
    assertEquals('2', getRowCountBySelector('#identityManagerButton'));
    assertEquals('2', getRowCountBySelector('#travelManagerButton'));
    assertEquals('2', getRowCountBySelector('#shoppingManagerButton'));

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
    assertEquals('1', getRowCountBySelector('#paymentManagerButton'));
    assertEquals('2', getRowCountBySelector('#addressesManagerButton'));
    // Identity docs updates to 1 (1 passport) while travel and shopping reset
    // to 0 because no matching entities remain in the updated list.
    assertEquals('1', getRowCountBySelector('#identityManagerButton'));
    assertEquals('0', getRowCountBySelector('#travelManagerButton'));
    assertEquals('0', getRowCountBySelector('#shoppingManagerButton'));
  });

  test('PasswordManagerCounterInitializesAndUpdates', async function() {
    setupProxies();
    const passwordCountResolver = new PromiseResolver<PasswordCount>();
    savedInfoHandler.getPasswordCount = () => passwordCountResolver.promise;
    await setupPage();

    // Before getPasswordCount() resolves, the counter label is empty.
    assertEquals('', getRowCount(getPasswordManagerRow()));

    passwordCountResolver.resolve({passwordCount: 3, passkeyCount: 2});
    await microtasksFinished();
    assertEquals('5', getRowCount(getPasswordManagerRow()));

    // Updates via WebUI listener.
    webUIListenerCallback(
        'password-count-changed', {passwordCount: 0, passkeyCount: 0});
    await microtasksFinished();
    assertEquals('0', getRowCount(getPasswordManagerRow()));

    webUIListenerCallback(
        'password-count-changed', {passwordCount: 7, passkeyCount: 4});
    await microtasksFinished();
    assertEquals('11', getRowCount(getPasswordManagerRow()));
  });

  test(
      'PaymentsCounterRemainsEmptyWhileAllSourcesUnresolved', async function() {
        setupProxies();
        const creditCardsResolver =
            new PromiseResolver<chrome.autofillPrivate.CreditCardEntry[]>();
        const ibansResolver =
            new PromiseResolver<chrome.autofillPrivate.IbanEntry[]>();
        const payOverTimeResolver = new PromiseResolver<
            chrome.autofillPrivate.PayOverTimeIssuerEntry[]>();
        const loyaltyCardsResolver =
            new PromiseResolver<chrome.autofillPrivate.LoyaltyCardEntry[]>();
        paymentsManager.getCreditCardList = () => creditCardsResolver.promise;
        paymentsManager.getIbanList = () => ibansResolver.promise;
        paymentsManager.getPayOverTimeIssuerList = () =>
            payOverTimeResolver.promise;
        paymentsManager.getLoyaltyCardList = () => loyaltyCardsResolver.promise;

        await setupPage();
        const paymentsRow =
            autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
                '#paymentManagerButton');

        // All 4 payment sources are unresolved.
        assertEquals('', getRowCount(paymentsRow));

        // Once payment lists resolve empty, paymentsCount_ becomes 0.
        creditCardsResolver.resolve([]);
        ibansResolver.resolve([]);
        payOverTimeResolver.resolve([]);
        loyaltyCardsResolver.resolve([]);
        await microtasksFinished();
        assertEquals('0', getRowCount(paymentsRow));
      });

  test('PaymentsCounterInitializesAndUpdates', async function() {
    setupProxies();
    paymentsManager.data.creditCards =
        [createCreditCardEntry(), createCreditCardEntry()];
    paymentsManager.data.ibans = [createIbanEntry()];
    paymentsManager.data.payOverTimeIssuers = [
      createPayOverTimeIssuerEntry(),
      createPayOverTimeIssuerEntry(),
      createPayOverTimeIssuerEntry(),
    ];
    paymentsManager.data.loyaltyCards =
        Array.from({length: 4}, () => createLoyaltyCardEntry());

    await setupPage();
    await paymentsManager.whenCalled('getCreditCardList');
    await paymentsManager.whenCalled('getIbanList');
    await paymentsManager.whenCalled('getPayOverTimeIssuerList');
    await paymentsManager.whenCalled('getLoyaltyCardList');

    const paymentsRow = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#paymentManagerButton');
    // 2 credit cards + 1 IBAN + 3 pay over time + 4 loyalty cards = 10.
    assertEquals('10', getRowCount(paymentsRow));

    // Personal data change updates credit cards, IBANs, and pay over time
    // while preserving loyalty cards count (4).
    autofillManager.lastCallback.setPersonalDataManagerListener!(
        [createAddressEntry()],
        [createCreditCardEntry()],
        [createIbanEntry(), createIbanEntry()],
        [],
    );
    await microtasksFinished();
    // 1 credit card + 2 IBANs + 0 pay over time + 4 loyalty cards = 7.
    assertEquals('7', getRowCount(paymentsRow));

    // Loyalty cards listener updates loyalty cards count while preserving
    // personal data counts.
    paymentsManager.lastCallback.setLoyaltyCardsChangedListener!
        (Array.from({length: 6}, () => createLoyaltyCardEntry()));
    await microtasksFinished();
    // 1 + 2 + 0 + 6 = 9.
    assertEquals('9', getRowCount(paymentsRow));

    // Empty loyalty cards list sets loyalty cards count to 0.
    paymentsManager.lastCallback.setLoyaltyCardsChangedListener!([]);
    await microtasksFinished();
    assertEquals('3', getRowCount(paymentsRow));
  });

  test('ContactInfoCounterInitializesAndUpdates', async function() {
    setupProxies();
    autofillManager.data.addresses = [createAddressEntry()];
    await setupPage();
    await autofillManager.whenCalled('getAddressList');

    const addressesRow =
        autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
            '#addressesManagerButton');
    assertEquals('1', getRowCount(addressesRow));

    autofillManager.lastCallback.setPersonalDataManagerListener!(
        [createAddressEntry(), createAddressEntry(), createAddressEntry()],
        [],
        [],
        [],
    );
    await microtasksFinished();
    assertEquals('3', getRowCount(addressesRow));

    autofillManager.lastCallback.setPersonalDataManagerListener!
        ([], [], [], []);
    await microtasksFinished();
    assertEquals('0', getRowCount(addressesRow));
  });

  test('AutofillAiEntityCountersInitializeAndUpdate', async function() {
    setupProxies();
    entityDataManager.setLoadEntityInstancesResponse([
      // Identity docs (2 passports + 1 driver's license + 1 national ID = 4).
      createEntityInstanceWithLabels(EntityTypeName.kPassport),
      createEntityInstanceWithLabels(EntityTypeName.kPassport),
      createEntityInstanceWithLabels(EntityTypeName.kDriversLicense),
      createEntityInstanceWithLabels(EntityTypeName.kNationalIdCard),
      // Travel (1 flight + 2 KTN + 1 redress + 1 vehicle = 5).
      createEntityInstanceWithLabels(EntityTypeName.kFlightReservation),
      createEntityInstanceWithLabels(EntityTypeName.kKnownTravelerNumber),
      createEntityInstanceWithLabels(EntityTypeName.kKnownTravelerNumber),
      createEntityInstanceWithLabels(EntityTypeName.kRedressNumber),
      createEntityInstanceWithLabels(EntityTypeName.kVehicle),
      // Shopping (3 orders + 2 shipments = 5).
      createEntityInstanceWithLabels(EntityTypeName.kOrder),
      createEntityInstanceWithLabels(EntityTypeName.kOrder),
      createEntityInstanceWithLabels(EntityTypeName.kOrder),
      createEntityInstanceWithLabels(EntityTypeName.kShipment),
      createEntityInstanceWithLabels(EntityTypeName.kShipment),
    ]);

    await setupPage({
      shoppingIntegrationEnabled: true,
    });
    await entityDataManager.whenCalled('loadEntityInstances');
    assertEquals(0, entityDataManager.getCallCount('getWritableEntityTypes'));

    const identityRow = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#identityManagerButton');
    const travelRow = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#travelManagerButton');
    const shoppingRow = autofillPage.shadowRoot.querySelector<CrLinkRowElement>(
        '#shoppingManagerButton');

    assertEquals('4', getRowCount(identityRow));
    assertEquals('5', getRowCount(travelRow));
    assertEquals('5', getRowCount(shoppingRow));

    // Update entities via listener.
    entityDataManager.callEntityInstancesChangedListener([
      createEntityInstanceWithLabels(EntityTypeName.kPassport),
      createEntityInstanceWithLabels(EntityTypeName.kVehicle),
    ]);
    await microtasksFinished();

    assertEquals('1', getRowCount(identityRow));
    assertEquals('1', getRowCount(travelRow));
    assertEquals('0', getRowCount(shoppingRow));

    // Clear all entities.
    entityDataManager.callEntityInstancesChangedListener([]);
    await microtasksFinished();

    assertEquals('0', getRowCount(identityRow));
    assertEquals('0', getRowCount(travelRow));
    assertEquals('0', getRowCount(shoppingRow));
  });

  test('RelatedServicesSectionRendersCorrectly', function() {
    const relatedServicesSection =
        autofillPage.shadowRoot.querySelector<HTMLElement>(
            `settings-section[page-title="${
                loadTimeData.getString(
                    'yourSavedInfoRelatedServicesTitle')}"]`);
    assertTrue(!!relatedServicesSection);

    const passwordManagerRow =
        relatedServicesSection.querySelector<CrLinkRowElement>(
            '#passwordManagerButton');
    assertTrue(!!passwordManagerRow, 'Password manager button not found');
    assertEquals(
        loadTimeData.getString('localPasswordManager'),
        passwordManagerRow.label);
    assertEquals('cr20:password-manager', passwordManagerRow.startIcon);
    assertTrue(passwordManagerRow.external);

    const walletRow = relatedServicesSection.querySelector<CrLinkRowElement>(
        '#googleWalletButton');
    assertTrue(!!walletRow, 'Wallet button not found');
    assertEquals(loadTimeData.getString('googleWalletTitle'), walletRow.label);
    assertEquals('settings20:wallet', walletRow.startIcon);
    assertTrue(walletRow.external);

    const profileRow = relatedServicesSection.querySelector<CrLinkRowElement>(
        '#googleAccountButton');
    assertTrue(!!profileRow, 'Profile button not found');
    assertEquals(loadTimeData.getString('googleAccount'), profileRow.label);
    assertEquals('settings20:google', profileRow.startIcon);
    assertTrue(profileRow.external);
  });

  async function testRelatedServiceRowOpensUrl(
      selector: string, urlStringId: string,
      expectedService: YourSavedInfoRelatedService) {
    const row = autofillPage.shadowRoot.querySelector<HTMLElement>(selector);
    assertTrue(!!row);
    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString(urlStringId), url);
    const [service] = await metricsBrowserProxy.whenCalled(
        'recordYourSavedInfoRelatedServiceClick');
    assertEquals(expectedService, service);
    const action = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        `Settings.YourSavedInfo.RelatedServiceClick.${
            YourSavedInfoRelatedService[expectedService]}`,
        action);
  }

  test(
      'PasswordManagerRelatedServiceRowOpensPasswordManager', async function() {
        const passwordManagerRow =
            autofillPage.shadowRoot.querySelector<HTMLElement>(
                '#passwordManagerButton');
        assertTrue(!!passwordManagerRow);
        passwordManagerRow.click();

        await passwordManager.whenCalled('recordPasswordsPageAccessInSettings');
        const page = await passwordManager.whenCalled('showPasswordManager');
        assertEquals(PasswordManagerPage.PASSWORDS, page);
        const [survey, isFromHomePage] =
            await savedInfoHandler.whenCalled('requestDataManagementSurvey');
        assertEquals(DataManagementSurvey.PASSWORDS, survey);
        assertTrue(isFromHomePage);
        const [service] = await metricsBrowserProxy.whenCalled(
            'recordYourSavedInfoRelatedServiceClick');
        assertEquals(
            YourSavedInfoRelatedService.GOOGLE_PASSWORD_MANAGER, service);
        const action = await metricsBrowserProxy.whenCalled('recordAction');
        assertEquals(
            'Settings.YourSavedInfo.RelatedServiceClick.' +
                'GOOGLE_PASSWORD_MANAGER',
            action);
      });

  test('WalletRelatedServiceRowOpensWallet', async function() {
    await testRelatedServiceRowOpensUrl(
        '#googleWalletButton', 'googleWalletUrl',
        YourSavedInfoRelatedService.GOOGLE_WALLET);
  });

  test('ProfileRelatedServiceRowOpensProfile', async function() {
    await testRelatedServiceRowOpensUrl(
        '#googleAccountButton', 'googleAccountUrl',
        YourSavedInfoRelatedService.GOOGLE_ACCOUNT);
  });

  test('RemovesListenersOnDisconnect', async function() {
    autofillPage.remove();
    await autofillManager.whenCalled('removePersonalDataManagerListener');
    await entityDataManager.whenCalled('removeEntityInstancesChangedListener');
    await paymentsManager.whenCalled('removeLoyaltyCardsChangedListener');
  });

  test('GetFocusConfigReturnsExpectedRouteSelectors', async function() {
    assertDeepEquals(
        [
          ['/payments', '#paymentManagerButton'],
          ['/contactInfo', '#addressesManagerButton'],
          ['/identityDocs', '#identityManagerButton'],
          ['/travel', '#travelManagerButton'],
          ['/shopping', '#shoppingManagerButton'],
          ['/enhancedAutofill', '#suggestionsFromGeminiLinkRow'],
        ],
        Array.from(autofillPage.getFocusConfig().entries()));

    await setupPage({
      shoppingIntegrationEnabled: false,
      showSuggestionsFromGeminiSettings: false,
    });
    assertDeepEquals(
        [
          ['/payments', '#paymentManagerButton'],
          ['/contactInfo', '#addressesManagerButton'],
          ['/identityDocs', '#identityManagerButton'],
          ['/travel', '#travelManagerButton'],
        ],
        Array.from(autofillPage.getFocusConfig().entries()));
  });

  test('GetAssociatedControlForReturnsExpectedElements', function() {
    assertEquals(
        autofillPage.shadowRoot.querySelector('#addressesManagerButton'),
        autofillPage.getAssociatedControlFor('contactInfo'));
    // <if expr="is_win or is_macosx">
    assertEquals(
        autofillPage.shadowRoot.querySelector('#passwordManagerButton'),
        autofillPage.getAssociatedControlFor('passkeys'));
    // </if>
    assertEquals(
        autofillPage.shadowRoot.querySelector('#paymentManagerButton'),
        autofillPage.getAssociatedControlFor('payments'));
    assertEquals(
        autofillPage.shadowRoot.querySelector('#identityManagerButton'),
        autofillPage.getAssociatedControlFor('identityDocs'));
    assertEquals(
        autofillPage.shadowRoot.querySelector('#travelManagerButton'),
        autofillPage.getAssociatedControlFor('travel'));
    assertEquals(
        autofillPage.shadowRoot.querySelector('#shoppingManagerButton'),
        autofillPage.getAssociatedControlFor('shopping'));
    assertEquals(
        autofillPage.shadowRoot.querySelector('#suggestionsFromGeminiLinkRow'),
        autofillPage.getAssociatedControlFor('suggestionsFromGemini'));

    assertThrows(() => autofillPage.getAssociatedControlFor('unknown'));
  });

  test('OffersAndLoyaltyCardsRowHiddenWhenFlagDisabled', async function() {
    await setupPage({offersAndLoyaltyCardsSettingsEnabled: false});
    assertFalse(!!autofillPage.shadowRoot.querySelector(
        '#offersAndLoyaltyCardsManagerButton'));
  });

  test('LoyaltyCardsCountedInPaymentsWhenFlagDisabled', async function() {
    setupProxies();
    paymentsManager.data.creditCards = [createCreditCardEntry()];
    paymentsManager.data.loyaltyCards = [
      createLoyaltyCardEntry(),
      createLoyaltyCardEntry(),
    ];

    await setupPage({offersAndLoyaltyCardsSettingsEnabled: false});
    await paymentsManager.whenCalled('getCreditCardList');
    await paymentsManager.whenCalled('getLoyaltyCardList');
    await microtasksFinished();

    // 1 credit card + 2 loyalty cards.
    assertEquals('3', getRowCountBySelector('#paymentManagerButton'));
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

  test('LoyaltyCardsCountedInOffersAndLoyaltyCardsRow', async function() {
    setupProxies();
    paymentsManager.data.creditCards = [createCreditCardEntry()];
    paymentsManager.data.loyaltyCards = [
      createLoyaltyCardEntry(),
      createLoyaltyCardEntry(),
    ];

    await setupPage({offersAndLoyaltyCardsSettingsEnabled: true});
    await paymentsManager.whenCalled('getCreditCardList');
    await paymentsManager.whenCalled('getLoyaltyCardList');
    await microtasksFinished();

    assertEquals('1', getRowCountBySelector('#paymentManagerButton'));
    assertEquals(
        '2', getRowCountBySelector('#offersAndLoyaltyCardsManagerButton'));

    paymentsManager.lastCallback.setLoyaltyCardsChangedListener!
        (Array.from({length: 5}, () => createLoyaltyCardEntry()));
    await microtasksFinished();

    assertEquals('1', getRowCountBySelector('#paymentManagerButton'));
    assertEquals(
        '5', getRowCountBySelector('#offersAndLoyaltyCardsManagerButton'));
  });
});
