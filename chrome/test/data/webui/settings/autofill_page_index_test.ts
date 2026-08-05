// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {AiEnterpriseFeaturePrefName} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsAutofillPageIndexElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('AutofillPageIndex', function() {
  let index: SettingsAutofillPageIndexElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      showSuggestionsFromGeminiSettings: true,
      shoppingIntegrationEnabled: true,
    });
    resetRouterForTesting();

    index = document.createElement('settings-autofill-page-index');

    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW);
    settingsPrefs.set(
        'prefs.optimization_guide.model_execution.autofill_prediction_improvements_enterprise_policy_allowed.value',
        ModelExecutionEnterprisePolicyValue.ALLOW);
    index.prefs = settingsPrefs.prefs!;

    document.body.appendChild(index);
    return flushTasks();
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  test('Routing', async function() {
    function assertActiveView(id: string) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
      assertFalse(!!index.$.viewManager.querySelector(
          `.active[slot=view]:not(#${id})`));
    }

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.AUTOFILL);
    await microtasksFinished();
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.PAYMENTS);
    await microtasksFinished();
    assertActiveView('payments');

    Router.getInstance().navigateTo(routes.CONTACT_INFO);
    await microtasksFinished();
    assertActiveView('contactInfo');

    // <if expr="is_win or is_macosx">
    Router.getInstance().navigateTo(routes.PASSKEYS);
    await microtasksFinished();
    assertActiveView('passkeys');
    // </if>

    Router.getInstance().navigateTo(routes.SHOPPING);
    await microtasksFinished();
    assertActiveView('shopping');

    Router.getInstance().navigateTo(routes.SUGGESTIONS_FROM_GEMINI);
    await microtasksFinished();
    assertActiveView('suggestionsFromGemini');
  });

  test('GeminiRouteDisabled', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      showSuggestionsFromGeminiSettings: false,
    });
    resetRouterForTesting();

    index = document.createElement('settings-autofill-page-index');
    index.prefs = settingsPrefs.prefs!;
    document.body.appendChild(index);
    await flushTasks();

    assertEquals(undefined, routes.SUGGESTIONS_FROM_GEMINI);
    const subpage = index.$.viewManager.querySelector('#suggestionsFromGemini');
    assertFalse(!!subpage);
  });

  test('ShoppingRouteDisabled', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      shoppingIntegrationEnabled: false,
    });
    resetRouterForTesting();

    index = document.createElement('settings-autofill-page-index');
    index.prefs = settingsPrefs.prefs!;
    document.body.appendChild(index);
    await flushTasks();

    assertEquals(undefined, routes.SHOPPING);
    const subpage = index.$.viewManager.querySelector('#shopping');
    assertFalse(!!subpage);
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    // Test that the child views are properly annotated.
    const childViewsId = [
      'payments',
      'shopping',
      'suggestionsFromGemini',
    ];
    for (const id of childViewsId) {
      assertTrue(!!index.$.viewManager.querySelector(
          `#${id}[slot=view][data-parent-view-id=parent]`));
    }

    // Test that search finds results in both parent and child views.
    const result = await index.searchContents('Payments');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
  });

  // Test that no errors happen during search.
  test('SearchNoErrors', async function() {
    // Searching for 'a' is expected to return results in all sections.
    const result = await index.searchContents('a');
    assertGT(result.matchCount, 0);
    assertFalse(result.canceled);
    assertFalse(result.wasClearSearch);
  });
});
