// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import type {SettingsAiSuggestionsPageElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, AiPageActions, FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {AiPageSuggestionsInteractions, CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, ModelExecutionEnterprisePolicyValue, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('SuggestionsPage', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsAiSuggestionsPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  teardown(function() {
    // Reset prefs to default values.
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.CONTEXTUAL_CUEING}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW);
    settingsPrefs.set(
        `prefs.${PrefName.CONTEXTUAL_CUEING}.value`,
        FeatureOptInState.NOT_INITIALIZED);
    metricsBrowserProxy.reset();
    openWindowProxy.reset();
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-ai-suggestions-page');
    subpage.prefs = settingsPrefs.prefs;
    document.body.appendChild(subpage);
    return flushTasks();
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

    const toggle = subpage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    const policyIndicator =
        subpage.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertFalse(!!policyIndicator);

    // Check NOT_INITIALIZED case.
    assertEquals(
        FeatureOptInState.NOT_INITIALIZED,
        subpage.getPref(PrefName.CONTEXTUAL_CUEING).value);
    assertTrue(toggle.checked);

    // Check DISABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.SUGGESTIONS_DISABLED,
        AiPageActions.AI_SUGGESTIONS_DISABLED);
    assertEquals(
        FeatureOptInState.DISABLED,
        subpage.getPref(PrefName.CONTEXTUAL_CUEING).value);
    assertFalse(toggle.checked);

    metricsBrowserProxy.reset();

    // Check ENABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.SUGGESTIONS_ENABLED,
        AiPageActions.AI_SUGGESTIONS_ENABLED);
    assertEquals(
        FeatureOptInState.ENABLED,
        subpage.getPref(PrefName.CONTEXTUAL_CUEING).value);
    assertTrue(toggle.checked);
  });

  test('suggestionsToggleDisabled', async () => {
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.CONTEXTUAL_CUEING}.value`,
        ModelExecutionEnterprisePolicyValue.DISABLE);
    await createPage();

    const indicator =
        subpage.shadowRoot!.querySelector('settings-ai-policy-indicator');
    assertTrue(!!indicator);

    const toggle = subpage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);

    const linkout = subpage.shadowRoot!.querySelector('cr-link-row');
    assertFalse(!!linkout);
  });

  test('learnMoreLinkRow', async function() {
    loadTimeData.overrideValues({
      aiSuggestionsHelpCenterArticleLink:
          'https://support.google.com/chrome?p=',
    });

    await createPage();

    const linkout = subpage.shadowRoot!.querySelector('cr-link-row');
    assertTrue(!!linkout);

    linkout.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('https://support.google.com/chrome?p=', url);
    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_LEARN_MORE_CLICKED);
    openWindowProxy.reset();
  });

  test('learnMoreLink', async () => {
    loadTimeData.overrideValues({
      aiSuggestionsHelpCenterArticleLink:
          'https://support.google.com/chrome?p=',
    });
    await createPage();

    const learnMoreLink =
        subpage.shadowRoot!.querySelector<HTMLAnchorElement>('#learnMoreLink');
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
        subpage.shadowRoot!.querySelector<HTMLAnchorElement>(
            '#syncSettingsLink');
    assertTrue(!!syncSettingsLink);
    assertEquals('chrome://settings/syncSetup', syncSettingsLink.href);
    syncSettingsLink.click();

    await assertFeatureInteractionMetrics(
        AiPageSuggestionsInteractions.SYNC_SETTINGS_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_SYNC_SETTINGS_CLICKED);
  });
});
