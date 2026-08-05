// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {AiEnterpriseFeaturePrefName, EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsIdentityDocsPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, ModelExecutionEnterprisePolicyValue, resetRouterForTesting, Router} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {MetricsBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('IdentityDocsPage', function() {
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

  async function setupPage(): Promise<SettingsIdentityDocsPageElement> {
    const page = document.createElement('settings-identity-docs-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    await flushTasks();
    return page;
  }

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  [{identityDocsOptIn: true},
   {identityDocsOptIn: false},
  ].forEach(({identityDocsOptIn}) => {
    test(`Toggle should show current opt-in status`, async function() {
      loadTimeData.overrideValues({
        canEnableOrDisableAutofillAi: true,
      });

      entityDataManager.setGetOptInStatusResponse(true);

      settingsPrefs.set(
          'prefs.autofill.autofill_ai.identity_entities_enabled.value',
          identityDocsOptIn);

      const page = await setupPage();

      assertFalse(page.$.optInToggle.disabled);
      assertEquals(page.$.optInToggle.checked, identityDocsOptIn);
    });
  });

  test(`Toggle should switch opt-in status in prefs`, async function() {
    loadTimeData.overrideValues({canEnableOrDisableAutofillAi: true});

    entityDataManager.setGetOptInStatusResponse(true);

    settingsPrefs.set(
        'prefs.autofill.autofill_ai.identity_entities_enabled.value', true);

    const page = await setupPage();

    assertTrue(page.$.optInToggle.checked);
    assertTrue(settingsPrefs.get(
        'prefs.autofill.autofill_ai.identity_entities_enabled.value'));

    page.$.optInToggle.click();

    assertFalse(page.$.optInToggle.checked);
    assertFalse(settingsPrefs.get(
        'prefs.autofill.autofill_ai.identity_entities_enabled.value'));
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
        `Toggle takes into account address opt in status ` +
            `experimentEnabled(${experimentEnabled}) ` +
            `addressAutofillStatus(${addressAutofillStatus})`,
        async function() {
          loadTimeData.overrideValues({
            canEnableOrDisableAutofillAi: true,
            AutofillSettingsEnterprisePolicyEnabled: experimentEnabled,
          });

          entityDataManager.setGetOptInStatusResponse(true);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.identity_entities_enabled.value',
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
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.identity_entities_enabled.value', true);
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
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.identity_entities_enabled.value', true);
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
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.identity_entities_enabled.value',
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
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.identity_entities_enabled.value', true);
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
          AutofillSettingsEnterprisePolicyEnabled: false,
          canEnableOrDisableAutofillAi: true,
        });

        settingsPrefs.set(
            'prefs.autofill.autofill_ai.identity_entities_enabled.value', true);
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

      const button = page.shadowRoot!.querySelector<HTMLElement>(
          '#suggestionsFromGeminiLinkRow');
      assertTrue(!!button);

      button.click();
      assertEquals('/enhancedAutofill', Router.getInstance().currentRoute.path);
      const action = await metricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          'PersonalContext.Settings.EntryPoint.IdentityDocsSettings',
          action);
    });

    test('row is hidden when flag is disabled', async function() {
      loadTimeData.overrideValues({
        showSuggestionsFromGeminiSettings: false,
      });
      resetRouterForTesting();

      const page = await setupPage();

      const button = page.shadowRoot!.querySelector<HTMLElement>(
          '#suggestionsFromGeminiLinkRow');
      assertFalse(!!button);
    });
  });
});
