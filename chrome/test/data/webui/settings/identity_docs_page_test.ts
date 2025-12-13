// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsIdentityDocsPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';

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

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  [{identityDocsOptIn: true},
   {identityDocsOptIn: false},
  ].forEach(({identityDocsOptIn}) => {
    test(`Toggle should show current opt-in status`, async function() {
      loadTimeData.overrideValues({userEligibleForAutofillAi: true});

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
    loadTimeData.overrideValues({userEligibleForAutofillAi: true});

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

  [{enhancedAutofillOptIn: true, identityDocsOptIn: true},
   {enhancedAutofillOptIn: true, identityDocsOptIn: false},
   {enhancedAutofillOptIn: false, identityDocsOptIn: true},
   {enhancedAutofillOptIn: false, identityDocsOptIn: false},
  ].forEach(({enhancedAutofillOptIn, identityDocsOptIn}) => {
    test(
        'When not elligible for enhanced autofill, toggle should' +
            'always be disabled and off: ' +
            `enhancedAutofillOptIn(${enhancedAutofillOptIn}) ` +
            `identityDocsOptIn(${identityDocsOptIn})`,
        async function() {
          loadTimeData.overrideValues({userEligibleForAutofillAi: false});

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);
          entityDataManager.setGetOptInStatusResponse(enhancedAutofillOptIn);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.identity_entities_enabled.value',
              identityDocsOptIn);

          const page = await setupPage();

          assertTrue(page.$.optInToggle.disabled);
          assertFalse(page.$.optInToggle.checked);
        });
  });

  [{identityDocsOptIn: true},
   {identityDocsOptIn: false},
  ].forEach(({identityDocsOptIn}) => {
    test(
        'When opted out from identity docs autofill, toggle should always ' +
            `be disabled and off, identityDocsOptIn(${identityDocsOptIn})`,
        async function() {
          loadTimeData.overrideValues({userEligibleForAutofillAi: true});

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);
          entityDataManager.setGetOptInStatusResponse(false);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.identity_entities_enabled.value',
              identityDocsOptIn);

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
            AutofillAiIgnoresWhetherAddressFillingIsEnabled: experimentEnabled,
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
});
