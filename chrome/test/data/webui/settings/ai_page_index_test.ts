// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

// <if expr="_google_chrome">
import {OnDeviceAiBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
// </if>
import type {SettingsAiPageIndexElement} from 'chrome://settings/settings.js';
import {AiSettingsSection, loadTimeData, PrefService, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

// <if expr="_google_chrome">
import {TestOnDeviceAiBrowserProxy} from './test_on_device_ai_browser_proxy.js';
// </if>

suite('AiPageIndex', function() {
  let index: SettingsAiPageIndexElement;

  async function createAiPageIndex(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    await PrefService.getInstance().whenInitialized();
    index = document.createElement('settings-ai-page-index');
    document.body.appendChild(index);
    // Wait for deferred route switching in currentRouteChanged(), which is
    // scheduled in a microtask.
    return microtasksFinished();
  }

  function assertActiveViews(ids: string[]) {
    for (const id of ids) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
    }
  }

  setup(function() {
    // <if expr="_google_chrome">
    OnDeviceAiBrowserProxyImpl.setInstance(new TestOnDeviceAiBrowserProxy());
    // </if>
    loadTimeData.overrideValues({
      showAiPage: true,
      showAiPageAiFeatureSection: true,
      showComposeControl: true,
      aiSettingsSection: AiSettingsSection.GEMINI,
      enableAiModeSearchSetting: true,
      actorLoginFederatedLoginSupportEnabled: true,
      showAiSuggestionsControl: true,
      showInlineCueMenuControl: true,
      showDictationControl: true,
      // <if expr="_google_chrome">
      showOnDeviceAiAdvancedSettings: true,
      // </if>
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

    Router.getInstance().navigateTo(routes.OFFER_WRITING_HELP);
    await microtasksFinished();
    assertActiveViews(['compose']);

    Router.getInstance().navigateTo(routes.GEMINI);
    await microtasksFinished();
    assertActiveViews(['gemini']);

    Router.getInstance().navigateTo(routes.GEMINI_LOGIN);
    await microtasksFinished();
    assertActiveViews(['geminiLoginPermissions']);

    Router.getInstance().navigateTo(routes.AI_SUGGESTIONS);
    await microtasksFinished();
    assertActiveViews(['aiSuggestions']);

    Router.getInstance().navigateTo(routes.INLINE_CUE_MENU);
    await microtasksFinished();
    assertActiveViews(['inlineCueMenu']);

    Router.getInstance().navigateTo(routes.DICTATION);
    await microtasksFinished();
    assertActiveViews(['dictation']);

    // <if expr="_google_chrome">
    Router.getInstance().navigateTo(routes.ON_DEVICE_AI);
    await microtasksFinished();
    assertActiveViews(['onDeviceAi']);
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

  test('aiSettingsSectionVisibilityNone', async function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      aiSettingsSection: AiSettingsSection.NONE,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertFalse(!!index.$.viewManager.querySelector('#glic[slot=view]'));
    assertFalse(!!index.$.viewManager.querySelector('#geic[slot=view]'));
  });

  test('aiSettingsSectionVisibilityGemini', async function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      aiSettingsSection: AiSettingsSection.GEMINI,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertTrue(!!index.$.viewManager.querySelector('#glic[slot=view]'));
    assertFalse(!!index.$.viewManager.querySelector('#geic[slot=view]'));
  });

  test('aiSettingsSectionVisibilityGeminiEnterprise', async function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      aiSettingsSection: AiSettingsSection.GEMINI_ENTERPRISE,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertFalse(!!index.$.viewManager.querySelector('#glic[slot=view]'));
    assertTrue(!!index.$.viewManager.querySelector('#geic[slot=view]'));
  });

  test('GeminiEnterpriseRoutingAndDataParentViewId', async function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      aiSettingsSection: AiSettingsSection.GEMINI_ENTERPRISE,
    });
    resetRouterForTesting();
    await createAiPageIndex();

    Router.getInstance().navigateTo(routes.GEMINI_ENTERPRISE);
    await microtasksFinished();
    assertActiveViews(['geminiEnterprise']);

    assertTrue(!!index.$.viewManager.querySelector(
        '#geminiEnterprise[slot=view][data-parent-view-id=geic]'));
  });

  test('dictationSectionVisibility', async function() {
    assertTrue(!!index.$.viewManager.querySelector('#dictation[slot=view]'));

    loadTimeData.overrideValues({
      showAiPage: true,
      showDictationControl: false,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertFalse(!!index.$.viewManager.querySelector('#dictation[slot=view]'));
  });

  // <if expr="_google_chrome">
  test('onDeviceAiSectionVisibility', async function() {
    assertTrue(!!index.$.viewManager.querySelector('#onDeviceAi[slot=view]'));

    loadTimeData.overrideValues({
      showAiPage: true,
      showOnDeviceAiAdvancedSettings: false,
    });
    resetRouterForTesting();
    await createAiPageIndex();
    assertFalse(!!index.$.viewManager.querySelector('#onDeviceAi[slot=view]'));
  });
  // </if>

  // Test that the child views are properly annotated.
  test('DataParentViewId', function() {
    const childViewsId = [
      'compose',
      'aiSuggestions',
      'inlineCueMenu',
      'dictation',
      // <if expr="_google_chrome">
      'onDeviceAi',
      // </if>
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

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixinLit is
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
    let result = await index.searchContents('help me write');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['parent'], ['glic']);

    // Case2: Results only in the "Glic" card.
    result = await index.searchContents('takeaways');
    assertFalse(result.canceled);
    assertGT(result.matchCount, 0);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['glic'], ['parent']);

    // Case3: Results in both "AI Innovations" and "Glic" card.
    result = await index.searchContents('keyboard shortcut');
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
