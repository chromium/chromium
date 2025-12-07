// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsTravelPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';

suite('TravelPage', function() {
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

  async function setupPage(): Promise<SettingsTravelPageElement> {
    const page = document.createElement('settings-travel-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    await flushTasks();
    return page;
  }

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  [{travelOptIn: true},
   {travelOptIn: false},
  ].forEach(({travelOptIn}) => {
    test(`Toggle should show current opt-in status`, async function() {
      loadTimeData.overrideValues({userEligibleForAutofillAi: true});

      entityDataManager.setGetOptInStatusResponse(true);

      settingsPrefs.set(
          'prefs.autofill.autofill_ai.travel_entities_enabled.value',
          travelOptIn);

      const page = await setupPage();

      assertFalse(page.$.optInToggle.disabled);
      assertEquals(page.$.optInToggle.checked, travelOptIn);
    });
  });

  test(`Toggle should switch opt-in status in prefs`, async function() {
    loadTimeData.overrideValues({userEligibleForAutofillAi: true});

    entityDataManager.setGetOptInStatusResponse(true);

    settingsPrefs.set(
        'prefs.autofill.autofill_ai.travel_entities_enabled.value', true);

    const page = await setupPage();

    assertTrue(page.$.optInToggle.checked);
    assertTrue(settingsPrefs.get(
        'prefs.autofill.autofill_ai.travel_entities_enabled.value'));

    page.$.optInToggle.click();

    assertFalse(page.$.optInToggle.checked);
    assertFalse(settingsPrefs.get(
        'prefs.autofill.autofill_ai.travel_entities_enabled.value'));
  });

  [{enhancedAutofillOptIn: true, travelOptIn: true},
   {enhancedAutofillOptIn: true, travelOptIn: false},
   {enhancedAutofillOptIn: false, travelOptIn: true},
   {enhancedAutofillOptIn: false, travelOptIn: false},
  ].forEach(({enhancedAutofillOptIn, travelOptIn}) => {
    test(
        'When not elligible for enhanced autofill, toggle should' +
            'always be disabled and off: ' +
            `enhancedAutofillOptIn(${enhancedAutofillOptIn}) ` +
            `travelOptIn(${travelOptIn})`,
        async function() {
          loadTimeData.overrideValues({userEligibleForAutofillAi: false});

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);
          entityDataManager.setGetOptInStatusResponse(enhancedAutofillOptIn);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.travel_entities_enabled.value',
              travelOptIn);

          const page = await setupPage();

          assertTrue(page.$.optInToggle.disabled);
          assertFalse(page.$.optInToggle.checked);
        });
  });

  [{travelOptIn: true},
   {travelOptIn: false},
  ].forEach(({travelOptIn}) => {
    test(
        'When opted out from travel autofill, toggle should always ' +
            `be disabled and off, travelOptIn(${travelOptIn})`,
        async function() {
          loadTimeData.overrideValues({userEligibleForAutofillAi: true});

          entityDataManager = new TestEntityDataManagerProxy();
          EntityDataManagerProxyImpl.setInstance(entityDataManager);
          entityDataManager.setGetOptInStatusResponse(false);

          settingsPrefs.set(
              'prefs.autofill.autofill_ai.travel_entities_enabled.value',
              travelOptIn);

          const page = await setupPage();

          assertTrue(page.$.optInToggle.disabled);
          assertFalse(page.$.optInToggle.checked);
        });
  });
});
