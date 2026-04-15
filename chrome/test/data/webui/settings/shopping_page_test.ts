// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiEnterpriseFeaturePrefName, EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsShoppingPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';

suite('ShoppingPage', function() {
  let entityDataManager: TestEntityDataManagerProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);
  });

  async function setupPage(): Promise<SettingsShoppingPageElement> {
    const page = document.createElement('settings-shopping-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    await flushTasks();
    return page;
  }

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  [{shoppingOptIn: true},
   {shoppingOptIn: false},
  ].forEach(({shoppingOptIn}) => {
    test(`Toggle should show current opt-in status`, async function() {
      loadTimeData.overrideValues({
        userEligibleForAutofillAi: true,
        autofillAiAvailableByDefault: false,
      });

      entityDataManager.setGetOptInStatusResponse(true);

      settingsPrefs.set(
          'prefs.autofill.autofill_ai.shopping_entities_enabled.value',
          shoppingOptIn);

      const page = await setupPage();

      assertFalse(page.$.optInToggle.disabled);
      assertEquals(page.$.optInToggle.checked, shoppingOptIn);
    });
  });

  test(`Toggle should switch opt-in status in prefs`, async function() {
    loadTimeData.overrideValues({userEligibleForAutofillAi: true});

    entityDataManager.setGetOptInStatusResponse(true);

    settingsPrefs.set(
        'prefs.autofill.autofill_ai.shopping_entities_enabled.value', true);

    const page = await setupPage();

    assertTrue(page.$.optInToggle.checked);
    assertTrue(settingsPrefs.get(
        'prefs.autofill.autofill_ai.shopping_entities_enabled.value'));

    page.$.optInToggle.click();

    assertFalse(page.$.optInToggle.checked);
    assertFalse(settingsPrefs.get(
        'prefs.autofill.autofill_ai.shopping_entities_enabled.value'));
  });

  [{enhancedAutofillOptIn: true, shoppingOptIn: true},
   {enhancedAutofillOptIn: true, shoppingOptIn: false},
   {enhancedAutofillOptIn: false, shoppingOptIn: true},
   {enhancedAutofillOptIn: false, shoppingOptIn: false},
  ].forEach(({enhancedAutofillOptIn, shoppingOptIn}) => {
    test(
        'When not elligible for enhanced autofill, toggle should' +
            'always be disabled and off: ' +
            `enhancedAutofillOptIn(${enhancedAutofillOptIn}) ` +
            `shoppingOptIn(${shoppingOptIn})`,
        async function() {
          loadTimeData.overrideValues({userEligibleForAutofillAi: false});

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);
          entityDataManager.setGetOptInStatusResponse(enhancedAutofillOptIn);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.shopping_entities_enabled.value',
              shoppingOptIn);

          const page = await setupPage();

          assertTrue(page.$.optInToggle.disabled);
          assertFalse(page.$.optInToggle.checked);
        });
  });

  [{canEnableOrDisableAutofillAi: true},
   {canEnableOrDisableAutofillAi: false},
  ].forEach(({canEnableOrDisableAutofillAi}) => {
    test(
        'When Autofill AI is available by default ' +
            '(autofillAiAvailableByDefault is true) the toggle ' +
            'availability depends on ' +
            'canEnableOrDisableAutofillAi, not on the opt-in status: ' +
            `canEnableOrDisableAutofillAi(${canEnableOrDisableAutofillAi})`,
        async function() {
          loadTimeData.overrideValues({
            userEligibleForAutofillAi: false,
            autofillAiAvailableByDefault: true,
            canEnableOrDisableAutofillAi: canEnableOrDisableAutofillAi,
          });

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);
          entityDataManager.setGetOptInStatusResponse(false);

          const page = await setupPage();

          assertEquals(
              page.$.optInToggle.disabled, !canEnableOrDisableAutofillAi);
        });
  });

  [{shoppingOptIn: true},
   {shoppingOptIn: false},
  ].forEach(({shoppingOptIn}) => {
    test(
        'When opted out from shopping autofill, toggle should always ' +
            `be disabled and off, shoppingOptIn(${shoppingOptIn})`,
        async function() {
          loadTimeData.overrideValues({userEligibleForAutofillAi: true});

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);
          entityDataManager.setGetOptInStatusResponse(false);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.shopping_entities_enabled.value',
              shoppingOptIn);

          const page = await setupPage();

          assertTrue(page.$.optInToggle.disabled);
          assertFalse(page.$.optInToggle.checked);
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
        `Toggle takes into account address opt in status ` +
            `experimentEnabled(${experimentEnabled}) ` +
            `addressAutofillStatus(${addressAutofillStatus})`,
        async function() {
          loadTimeData.overrideValues({
            userEligibleForAutofillAi: true,
            AutofillAddOtherDatatypesPrefIsEnabled: experimentEnabled,
            autofillAiAvailableByDefault: false,
          });

          entityDataManager.setGetOptInStatusResponse(true);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.shopping_entities_enabled.value',
              true);
          settingsPrefs.set(
              'prefs.autofill.profile_enabled.value', addressAutofillStatus);

          const page = await setupPage();

          assertEquals(page.$.optInToggle.disabled, toggleDisabled);
        });
  });

  test(
      'Policy controlled icon is shown when autofillProfileEnabled is ' +
          'controlled by policy',
      async function() {
        loadTimeData.overrideValues({
          userEligibleForAutofillAi: true,
          AutofillAddOtherDatatypesPrefIsEnabled: false,
          autofillAiAvailableByDefault: true,
          canEnableOrDisableAutofillAi: true,
          enableYourSavedInfoPolicyAndExtentionToggleIndicators: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.shopping_entities_enabled.value', true);
        settingsPrefs.set('prefs.autofill.profile_enabled', {
          value: false,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        });

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        const extensionControlledIndicator =
            page.shadowRoot!.querySelector('#autofillExtensionIndicator');

        assertTrue(!!policyIndicator);
        assertFalse(!!extensionControlledIndicator);
        assertFalse(page.$.optInToggle.checked);
      });

  test(
      'Extension indicator is shown when autofillProfileEnabled is ' +
          'controlled by extension',
      async function() {
        loadTimeData.overrideValues({
          userEligibleForAutofillAi: true,
          AutofillAddOtherDatatypesPrefIsEnabled: false,
          autofillAiAvailableByDefault: true,
          canEnableOrDisableAutofillAi: true,
          enableYourSavedInfoPolicyAndExtentionToggleIndicators: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.shopping_entities_enabled.value', true);
        settingsPrefs.set('prefs.autofill.profile_enabled', {
          value: false,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
          extensionId: 'test-extension-id',
        });

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        const extensionControlledIndicator =
            page.shadowRoot!.querySelector('#autofillExtensionIndicator');

        assertFalse(!!policyIndicator);
        assertTrue(!!extensionControlledIndicator);
        assertFalse(page.$.optInToggle.checked);
      });

  test(
      'Extension indicator is not shown when autofillProfileEnabled is ' +
          'controlled by extension and forced true',
      async function() {
        loadTimeData.overrideValues({
          userEligibleForAutofillAi: true,
          AutofillAddOtherDatatypesPrefIsEnabled: false,
          autofillAiAvailableByDefault: true,
          canEnableOrDisableAutofillAi: true,
          enableYourSavedInfoPolicyAndExtentionToggleIndicators: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.shopping_entities_enabled.value',
            false);
        settingsPrefs.set('prefs.autofill.profile_enabled', {
          value: true,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
          extensionId: 'test-extension-id',
        });

        const page = await setupPage();
        const extensionControlledIndicator =
            page.shadowRoot!.querySelector('#autofillExtensionIndicator');

        assertFalse(!!extensionControlledIndicator);
        assertFalse(page.$.optInToggle.checked);
      });

  test(
      'Policy controlled icon is shown when Autofill AI is ' +
          'controlled by policy',
      async function() {
        loadTimeData.overrideValues({
          userEligibleForAutofillAi: true,
          AutofillAddOtherDatatypesPrefIsEnabled: false,
          autofillAiAvailableByDefault: true,
          canEnableOrDisableAutofillAi: true,
          enableYourSavedInfoPolicyAndExtentionToggleIndicators: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.shopping_entities_enabled.value', true);
        settingsPrefs.set(`prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}`, {
          value: ModelExecutionEnterprisePolicyValue.DISABLE,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        });

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
          userEligibleForAutofillAi: true,
          AutofillAddOtherDatatypesPrefIsEnabled: false,
          autofillAiAvailableByDefault: true,
          canEnableOrDisableAutofillAi: true,
          enableYourSavedInfoPolicyAndExtentionToggleIndicators: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.shopping_entities_enabled.value', true);
        settingsPrefs.set(`prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}`, {
          value: ModelExecutionEnterprisePolicyValue.ALLOW,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        });

        const page = await setupPage();
        const policyIndicator = page.$.optInToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');

        assertFalse(!!policyIndicator);
        assertTrue(page.$.optInToggle.checked);
      });
});
