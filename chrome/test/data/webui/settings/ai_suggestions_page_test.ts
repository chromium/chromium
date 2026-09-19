// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import type {SettingsAiSuggestionsPageElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, AiPageActions, FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import {AiPageSuggestionsInteractions, ChromeSuggestionsSettingsValue, loadTimeData, MetricsBrowserProxyImpl, ModelExecutionEnterprisePolicyValue, OpenWindowProxyImpl, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: PrefName.CONTEXTUAL_CUEING,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: FeatureOptInState.NOT_INITIALIZED,
    },
    {
      key: AiEnterpriseFeaturePrefName.CONTEXTUAL_CUEING,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW,
    },
  ];
}

suite('SuggestionsPage', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsAiSuggestionsPageElement;
  let prefService: PrefService;

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  teardown(function() {
    metricsBrowserProxy.reset();
    openWindowProxy.reset();
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-ai-suggestions-page');
    document.body.appendChild(subpage);
    return microtasksFinished();
  }

  async function assertFeatureInteractionMetrics(
      interaction: AiPageSuggestionsInteractions, action: string) {
    const result = await metricsBrowserProxy.whenCalled(
        'recordAiPageSuggestionsInteractions');
    assertEquals(interaction, result);

    assertEquals(action, await metricsBrowserProxy.whenCalled('recordAction'));
  }

  test('showSuggestionsToggle', async () => {
    await createPage();

    const toggle = subpage.shadowRoot.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    const policyIndicator =
        subpage.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertFalse(!!policyIndicator);

    // Check NOT_INITIALIZED case.
    assertEquals(
        FeatureOptInState.NOT_INITIALIZED,
        prefService.getPref(PrefName.CONTEXTUAL_CUEING).value);
    assertTrue(toggle.checked);

    // Check DISABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.SUGGESTIONS_DISABLED,
        AiPageActions.AI_SUGGESTIONS_DISABLED);
    assertEquals(
        FeatureOptInState.DISABLED,
        prefService.getPref(PrefName.CONTEXTUAL_CUEING).value);
    assertFalse(toggle.checked);

    metricsBrowserProxy.reset();

    // Check ENABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.SUGGESTIONS_ENABLED,
        AiPageActions.AI_SUGGESTIONS_ENABLED);
    assertEquals(
        FeatureOptInState.ENABLED,
        prefService.getPref(PrefName.CONTEXTUAL_CUEING).value);
    assertTrue(toggle.checked);
  });

  test('suggestionsToggleDisabled', async () => {
    await prefService.setPrefValue(
        AiEnterpriseFeaturePrefName.CONTEXTUAL_CUEING,
        ChromeSuggestionsSettingsValue.DISABLED);
    await createPage();

    const indicator =
        subpage.shadowRoot.querySelector('settings-ai-policy-indicator');
    assertTrue(!!indicator);

    const toggle = subpage.shadowRoot.querySelector('settings-toggle-button');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);

    const linkout = subpage.shadowRoot.querySelector('.cr-row.flex');
    assertFalse(!!linkout);
  });

  test('learnMoreLink', async () => {
    loadTimeData.overrideValues({
      aiSuggestionsHelpCenterArticleLink:
          'https://support.google.com/chrome?p=',
    });
    await createPage();

    const learnMoreLink =
        subpage.shadowRoot.querySelector<HTMLAnchorElement>('#learnMoreLink');
    assertTrue(!!learnMoreLink);
    assertEquals('https://support.google.com/chrome?p=', learnMoreLink.href);
    learnMoreLink.click();

    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_LEARN_MORE_CLICKED);
  });

  test('syncSettingsLink', async () => {
    await createPage();

    const syncSettingsLink =
        subpage.shadowRoot.querySelector<HTMLAnchorElement>(
            '#syncSettingsLink');
    assertTrue(!!syncSettingsLink);
    assertEquals('chrome://settings/syncSetup', syncSettingsLink.href);
    syncSettingsLink.click();

    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.SYNC_SETTINGS_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_SYNC_SETTINGS_CLICKED);
  });
});
