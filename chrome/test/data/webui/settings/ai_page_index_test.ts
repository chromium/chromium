// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import type {SettingsAiPageIndexElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('AiPageIndex', function() {
  let index: SettingsAiPageIndexElement;

  async function createAiPageIndex(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
    index = document.createElement('settings-ai-page-index');
    index.prefs = settingsPrefs.prefs!;
    document.body.appendChild(index);
    return flushTasks();
  }

  function assertActiveViews(ids: string[]) {
    for (const id of ids) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
    }
  }

  setup(function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      showAiPageAiFeatureSection: true,
      showComposeControl: true,
      showHistorySearchControl: true,
      showGlicSettings: true,
      enableAiModeSearchSetting: true,
      actorLoginFederatedLoginSupportEnabled: true,
    });
    resetRouterForTesting();
    return createAiPageIndex();
  });

  test('Routing', async function() {
    const defaultViews = [
      'aiInfoCard',
      'aiModeSearch',
      'glic',
      'parent',
    ];

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveViews(defaultViews);

    Router.getInstance().navigateTo(routes.AI);
    await microtasksFinished();
    assertActiveViews(defaultViews);

    Router.getInstance().navigateTo(routes.HISTORY_SEARCH);
    await microtasksFinished();
    assertActiveViews(['historySearch']);

    Router.getInstance().navigateTo(routes.OFFER_WRITING_HELP);
    await microtasksFinished();
    assertActiveViews(['compose']);

    Router.getInstance().navigateTo(routes.GEMINI);
    await microtasksFinished();
    assertActiveViews(['gemini']);

    Router.getInstance().navigateTo(routes.GEMINI_LOGIN);
    await microtasksFinished();
    assertActiveViews(['geminiLoginPermissions']);
  });

  test('aiFeaturesSectionVisibility', async function() {
    assertTrue(!!index.$.viewManager.querySelector('#parent[slot=view]'));

    loadTimeData.overrideValues({
      showAiPage: true,
      showAiPageAiFeatureSection: false,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertFalse(!!index.$.viewManager.querySelector('#parent[slot=view]'));
  });

  test('aiModeSearchSectionVisibility', async function() {
    assertTrue(!!index.$.viewManager.querySelector('#aiModeSearch[slot=view]'));

    loadTimeData.overrideValues({
      showAiPage: true,
      enableAiModeSearchSetting: false,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertFalse(
        !!index.$.viewManager.querySelector('#aiModeSearch[slot=view]'));
  });

  test('glicSectionVisibility', async function() {
    assertTrue(!!index.$.viewManager.querySelector('#glic[slot=view]'));

    loadTimeData.overrideValues({
      showAiPage: true,
      showGlicSettings: false,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertFalse(!!index.$.viewManager.querySelector('#glic[slot=view]'));
  });

  // Test that the child views are properly annotated.
  test('DataParentViewId', function() {
    const childViewsId = [
      'historySearch',
      'compose',
    ];
    for (const id of childViewsId) {
      assertTrue(!!index.$.viewManager.querySelector(
          `#${id}[slot=view][data-parent-view-id=parent]`));
    }

    assertTrue(!!index.$.viewManager.querySelector(
        '#gemini[slot=view][data-parent-view-id=glic]'));
    assertTrue(!!index.$.viewManager.querySelector(
        '#geminiLoginPermissions[slot=view][data-parent-view-id=gemini]'));
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    function assertVisibleViews(visible: string[], hidden: string[]) {
      for (const id of visible) {
        assertTrue(isVisible(index.$.viewManager.querySelector(`#${id}`)), id);
      }

      for (const id of hidden) {
        assertFalse(isVisible(index.$.viewManager.querySelector(`#${id}`)), id);
      }
    }

    // Case1: Results only in the "AI Innovations" card.
    let result = await index.searchContents('history search');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['parent'], ['glic']);

    // Case2: Results only in the "Glic" card.
    result = await index.searchContents('keyboard shortcut');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['glic'], ['parent']);

    // Case3: Results only in both "AI Innovations" and "Glic" card.
    result = await index.searchContents('a');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(
        [
          'parent',
          'glic',
        ],
        []);
  });
});
