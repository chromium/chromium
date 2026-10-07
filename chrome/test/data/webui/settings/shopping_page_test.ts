// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiEnterpriseFeaturePrefName, EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsAutofillAiEntriesListElement, SettingsShoppingPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, MetricsBrowserProxyImpl, ModelExecutionEnterprisePolicyValue, PrefsBrowserProxy, PrefService, resetRouterForTesting, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('ShoppingPage', function() {
  let entityDataManager: TestEntityDataManagerProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefsBrowserProxy = new TestPrefsBrowserProxy([
      {
        key: 'autofill.autofill_ai.shopping_entities_enabled',
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
        key: 'autofill.types_blocked',
        type: chrome.settingsPrivate.PrefType.LIST,
        value: [],
      },
    ]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);
  });

  async function setupPage(): Promise<SettingsShoppingPageElement> {
    const page = document.createElement('settings-shopping-page');
    document.body.appendChild(page);
    await microtasksFinished();
    return page;
  }

  [{shoppingOptIn: true},
   {shoppingOptIn: false},
  ].forEach(({shoppingOptIn}) => {
    test('Toggle should show current opt-in status', async function() {
      loadTimeData.overrideValues({
        canEnableOrDisableAutofillAi: true,
      });

      entityDataManager.setGetOptInStatusResponse(true);

      prefService.setPrefValue(
          'autofill.autofill_ai.shopping_entities_enabled', shoppingOptIn);

      const page = await setupPage();

      assertFalse(page.$.optInToggle.disabled);
      assertEquals(page.$.optInToggle.checked, shoppingOptIn);
    });
  });

  test('Toggle should switch opt-in status in prefs', async function() {
    loadTimeData.overrideValues({canEnableOrDisableAutofillAi: true});

    entityDataManager.setGetOptInStatusResponse(true);

    prefService.setPrefValue(
        'autofill.autofill_ai.shopping_entities_enabled', true);

    const page = await setupPage();

    assertTrue(page.$.optInToggle.checked);
    assertTrue(
        prefService
            .getPref<boolean>('autofill.autofill_ai.shopping_entities_enabled')
            .value);

    const entriesList =
        page.shadowRoot.querySelector<SettingsAutofillAiEntriesListElement>(
            'settings-autofill-ai-entries-list')!;
    assertTrue(!!entriesList);
    assertTrue(entriesList.allowNewEntitiesAdditionPref!.value);

    page.$.optInToggle.click();
    await microtasksFinished();

    assertFalse(page.$.optInToggle.checked);
    assertFalse(
        prefService
            .getPref<boolean>('autofill.autofill_ai.shopping_entities_enabled')
            .value);
    assertFalse(entriesList.allowNewEntitiesAdditionPref!.value);
  });

  [{canEnableOrDisableAutofillAi: true},
   {canEnableOrDisableAutofillAi: false},
  ].forEach(({canEnableOrDisableAutofillAi}) => {
    test(
        'Toggle availability depends on canEnableOrDisableAutofillAi: ' +
            `canEnableOrDisableAutofillAi(${canEnableOrDisableAutofillAi})`,
        async function() {
          loadTimeData.overrideValues({
            canEnableOrDisableAutofillAi: canEnableOrDisableAutofillAi,
          });

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);

          const page = await setupPage();

          assertEquals(
              page.$.optInToggle.disabled, !canEnableOrDisableAutofillAi);
        });
  });

  [{
    experimentEnabled: true,
    addressAutofillStatus: true,
    toggleDisabled: false,
  },
   {
     experimentEnabled: true,
     addressAutofillStatus: false,
     toggleDisabled: false,
   },
   {
     experimentEnabled: false,
     addressAutofillStatus: true,
     toggleDisabled: false,
   },
   {
     experimentEnabled: false,
     addressAutofillStatus: false,
     toggleDisabled: true,
   },
  ].forEach(({experimentEnabled, addressAutofillStatus, toggleDisabled}) => {
    test(
        'Toggle takes into account address opt in status ' +
            `experimentEnabled(${experimentEnabled}) ` +
            `addressAutofillStatus(${addressAutofillStatus})`,
        async function() {
          loadTimeData.overrideValues({
            canEnableOrDisableAutofillAi: true,
            AutofillSettingsEnterprisePolicyEnabled: experimentEnabled,
          });

          entityDataManager.setGetOptInStatusResponse(true);

          prefService.setPrefValue(
              'autofill.autofill_ai.shopping_entities_enabled', true);
          prefService.setPrefValue(
              'autofill.profile_enabled', addressAutofillStatus);

          const page = await setupPage();

          assertEquals(page.$.optInToggle.disabled, toggleDisabled);
        });
  });

  test(
      'Policy controlled icon is shown when autofillProfileEnabled is ' +
          'controlled by policy',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', true);
        prefsBrowserProxy.fakeApi.sendPrefChanges([{
          key: 'autofill.profile_enabled',
          value: false,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        }]);

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        const extensionControlledIndicator =
            page.shadowRoot.querySelector('#autofillExtensionIndicator');

        assertTrue(!!policyIndicator);
        assertFalse(!!extensionControlledIndicator);
        assertFalse(page.$.optInToggle.checked);
      });

  test(
      'Extension indicator is shown when autofillProfileEnabled is ' +
          'controlled by extension',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', true);
        prefsBrowserProxy.fakeApi.sendPrefChanges([{
          key: 'autofill.profile_enabled',
          value: false,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
          extensionId: 'test-extension-id',
        }]);

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        const extensionControlledIndicator =
            page.shadowRoot.querySelector('#autofillExtensionIndicator');

        assertFalse(!!policyIndicator);
        assertTrue(!!extensionControlledIndicator);
        assertFalse(page.$.optInToggle.checked);
      });

  test(
      'Extension indicator is not shown when autofillProfileEnabled is ' +
          'controlled by extension and forced true',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', false);
        prefsBrowserProxy.fakeApi.sendPrefChanges([{
          key: 'autofill.profile_enabled',
          value: true,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
          extensionId: 'test-extension-id',
        }]);

        const page = await setupPage();
        const extensionControlledIndicator =
            page.shadowRoot.querySelector('#autofillExtensionIndicator');

        assertFalse(!!extensionControlledIndicator);
        assertFalse(page.$.optInToggle.checked);
      });

  test(
      'Policy controlled icon is shown when Autofill AI is ' +
          'controlled by policy',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', true);
        prefsBrowserProxy.fakeApi.sendPrefChanges([{
          key: AiEnterpriseFeaturePrefName.AUTOFILL_AI,
          value: ModelExecutionEnterprisePolicyValue.DISABLE,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        }]);

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');

        assertTrue(!!policyIndicator);
        assertFalse(page.$.optInToggle.checked);
      });

  test(
      'Policy controlled icon is not shown when Autofill AI is ' +
          'allowed by policy',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', true);
        prefsBrowserProxy.fakeApi.sendPrefChanges([{
          key: AiEnterpriseFeaturePrefName.AUTOFILL_AI,
          value: ModelExecutionEnterprisePolicyValue.ALLOW,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        }]);

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');

        assertFalse(!!policyIndicator);
        assertTrue(page.$.optInToggle.checked);
      });

  test(
      'Policy controlled icon is shown when shopping is blocked by ' +
          'types_blocked policy',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: true,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', true);
        prefService.setPrefValue(
            'autofill.types_blocked',
            [{url_pattern: '*', blocked_types: ['shopping']}]);

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');

        assertTrue(!!policyIndicator);
        assertTrue(page.$.optInToggle.controlDisabled());
        assertFalse(page.$.optInToggle.checked);

        const entriesList =
            page.shadowRoot.querySelector<SettingsAutofillAiEntriesListElement>(
                'settings-autofill-ai-entries-list')!;
        assertTrue(!!entriesList);
        assertFalse(entriesList.allowNewEntitiesAdditionPref!.value);
      });

  test(
      'Policy controlled icon is shown when all is blocked by ' +
          'types_blocked policy',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: true,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', true);
        prefService.setPrefValue(
            'autofill.types_blocked',
            [{url_pattern: '*', blocked_types: ['all']}]);

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');

        assertTrue(!!policyIndicator);
        assertTrue(page.$.optInToggle.controlDisabled());
        assertFalse(page.$.optInToggle.checked);

        const entriesList =
            page.shadowRoot.querySelector<SettingsAutofillAiEntriesListElement>(
                'settings-autofill-ai-entries-list')!;
        assertTrue(!!entriesList);
        assertFalse(entriesList.allowNewEntitiesAdditionPref!.value);
      });

  test(
      'types_blocked policy is ignored when enterprise policy flag is disabled',
      async function() {
        loadTimeData.overrideValues({
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        prefService.setPrefValue(
            'autofill.autofill_ai.shopping_entities_enabled', true);
        prefService.setPrefValue(
            'autofill.types_blocked',
            [{url_pattern: '*', blocked_types: ['shopping']}]);

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');

        assertFalse(!!policyIndicator);
        assertFalse(page.$.optInToggle.controlDisabled());
        assertTrue(page.$.optInToggle.checked);
      });

  suite('SuggestionsFromGemini', function() {
    let metricsBrowserProxy: TestMetricsBrowserProxy;

    setup(function() {
      metricsBrowserProxy = new TestMetricsBrowserProxy();
      MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
      loadTimeData.overrideValues({
        showSuggestionsFromGeminiSettings: false,
      });
      resetRouterForTesting();
    });

    teardown(function() {
      loadTimeData.overrideValues({
        showSuggestionsFromGeminiSettings: false,
      });
      resetRouterForTesting();
    });

    test('row is visible and navigates when flag is enabled', async function() {
      loadTimeData.overrideValues({
        showSuggestionsFromGeminiSettings: true,
      });
      resetRouterForTesting();

      const page = await setupPage();

      const button = page.shadowRoot.querySelector<HTMLElement>(
          '#suggestionsFromGeminiLinkRow');
      assertTrue(!!button);

      button.click();
      assertEquals('/enhancedAutofill', Router.getInstance().currentRoute.path);
      const action = await metricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          'PersonalContext.Settings.EntryPoint.ShoppingSettings', action);
    });

    test('row is hidden when flag is disabled', async function() {
      loadTimeData.overrideValues({
        showSuggestionsFromGeminiSettings: false,
      });
      resetRouterForTesting();

      const page = await setupPage();

      const button = page.shadowRoot.querySelector<HTMLElement>(
          '#suggestionsFromGeminiLinkRow');
      assertFalse(!!button);
    });
  });
});
