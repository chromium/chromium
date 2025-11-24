// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {AiEnterpriseFeaturePrefName} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement, SettingsYourSavedInfoPageIndexElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('YourSavedInfoPageIndex', function() {
  let index: SettingsYourSavedInfoPageIndexElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // routes.YOUR_SAVED_INFO does not exist if enableYourSavedInfoSettingsPage
    // is false
    loadTimeData.overrideValues({enableYourSavedInfoSettingsPage: true});
    resetRouterForTesting();

    index = document.createElement('settings-your-saved-info-page-index');

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

    Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO);
    await microtasksFinished();
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.PAYMENTS);
    await microtasksFinished();
    assertActiveView('payments');

    Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO_CONTACT_INFO);
    await microtasksFinished();
    assertActiveView('contactInfo');

    // <if expr="is_win or is_macosx">
    Router.getInstance().navigateTo(routes.PASSKEYS);
    await microtasksFinished();
    assertActiveView('passkeys');
    // </if>
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    // Test that the child views are properly annotated.
    const childViewsId = [
      'payments',
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
});
