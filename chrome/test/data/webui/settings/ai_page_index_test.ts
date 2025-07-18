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
      showCompareControl: true,
      showComposeControl: true,
      showHistorySearchControl: true,
      showTabOrganizationControl: true,
      // <if expr="enable_glic">
      showGlicSettings: true,
      // </if>
    });
    resetRouterForTesting();
    return createAiPageIndex();
  });

  test('Routing', async function() {
    const defaultViews = [
      'aiInfoCard',
      // <if expr="enable_glic">
      'glic',
      // </if>
      'parent',
    ];

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveViews(defaultViews);

    Router.getInstance().navigateTo(routes.AI);
    await microtasksFinished();
    assertActiveViews(defaultViews);

    Router.getInstance().navigateTo(routes.AI_TAB_ORGANIZATION);
    await microtasksFinished();
    assertActiveViews(['tabOrganization']);

    Router.getInstance().navigateTo(routes.HISTORY_SEARCH);
    await microtasksFinished();
    assertActiveViews(['historySearch']);

    Router.getInstance().navigateTo(routes.OFFER_WRITING_HELP);
    await microtasksFinished();
    assertActiveViews(['compose']);

    Router.getInstance().navigateTo(routes.COMPARE);
    await microtasksFinished();
    assertActiveViews(['compare']);

    // <if expr="enable_glic">
    Router.getInstance().navigateTo(routes.GEMINI);
    await microtasksFinished();
    assertActiveViews(['gemini']);
    // </if>
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

  // <if expr="enable_glic">
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
  // </if>

  // Test that the child views are properly annotated.
  test('DataParentViewId', function() {
    const childViewsId =
        ['tabOrganization', 'historySearch', 'compose', 'compare'];
    for (const id of childViewsId) {
      assertTrue(!!index.$.viewManager.querySelector(
          `#${id}[slot=view][data-parent-view-id=parent]`));
    }

    // <if expr="enable_glic">
    assertTrue(!!index.$.viewManager.querySelector(
        '#gemini[slot=view][data-parent-view-id=glic]'));
    // </if>
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
    let result = await index.searchContents('tab organizer');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['parent'], ['glic']);

    // <if expr="enable_glic">
    // Case2: Results only in the "Glic" card.
    result = await index.searchContents('keyboard shortcut');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['glic'], ['parent']);
    // </if>

    // Case3: Results only in both "AI Innovations" and "Glic" card.
    result = await index.searchContents('a');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(
        [
          'parent',
          // <if expr="enable_glic">
          'glic',
          // </if>
        ],
        []);
  });
});
