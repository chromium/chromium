// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import type {SettingsAutofillPageIndexElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('AutofillPageIndex', function() {
  let index: SettingsAutofillPageIndexElement;

  async function createAutofillPageIndex(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
    index = document.createElement('settings-autofill-page-index');
    index.prefs = settingsPrefs.prefs!;
    document.body.appendChild(index);
    return flushTasks();
  }

  function assertActiveView(id: string) {
    assertTrue(!!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
    assertFalse(
        !!index.$.viewManager.querySelector(`.active[slot=view]:not(#${id})`));
  }

  setup(function() {
    loadTimeData.overrideValues({
      enableYourSavedInfoSettingsPage: false,
      showAutofillAiControl: false,
    });
    resetRouterForTesting();
    return createAutofillPageIndex();
  });

  test('Routing', async function() {
    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.PAYMENTS);
    await microtasksFinished();
    assertActiveView('payments');

    Router.getInstance().navigateTo(routes.ADDRESSES);
    await microtasksFinished();
    assertActiveView('addresses');

    // <if expr="is_win or is_macosx">
    Router.getInstance().navigateTo(routes.PASSKEYS);
    await microtasksFinished();
    assertActiveView('passkeys');
    // </if>
  });

  test('RoutingAutofillAi', async function() {
    assertFalse(loadTimeData.getBoolean('showAutofillAiControl'));
    assertFalse(
        !!index.$.viewManager.querySelector('settings-autofill-ai-section'));

    loadTimeData.overrideValues({showAutofillAiControl: true});
    resetRouterForTesting();
    await createAutofillPageIndex();

    assertTrue(
        !!index.$.viewManager.querySelector('settings-autofill-ai-section'));
    Router.getInstance().navigateTo(routes.AUTOFILL_AI);
    await microtasksFinished();
    assertActiveView('autofillAi');
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    // Test that the child views are properly annotated.
    const childViewsId = [
      'payments', 'addresses',
      // <if expr="is_win or is_macosx">
      'passkeys',
      // </if>
    ];
    for (const id of childViewsId) {
      assertTrue(!!index.$.viewManager.querySelector(
          `#${id}[slot=view][data-parent-view-id=parent]`));
    }

    // Test that search finds results in both parent and child views.
    const result = await index.searchContents('Addresses and more');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
  });
});
