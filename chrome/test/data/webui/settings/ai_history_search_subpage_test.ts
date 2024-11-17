// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsHistorySearchPageElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, AiPageActions, FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {AiPageHistorySearchInteractions, CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('HistorySearchSubpage', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsHistorySearchPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-history-search-page');
    subpage.prefs = settingsPrefs.prefs;
    document.body.appendChild(subpage);
    return flushTasks();
  }

  async function assertFeatureInteractionMetrics(
      interaction: AiPageHistorySearchInteractions, action: string) {
    const result = await metricsBrowserProxy.whenCalled(
        'recordAiPageHistorySearchInteractions');
    assertEquals(interaction, result);

    assertEquals(action, await metricsBrowserProxy.whenCalled('recordAction'));
  }

  test('historySearchToggle', async () => {
    await createPage();

    const toggle = subpage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    // Check NOT_INITIALIZED case.
    assertEquals(
        FeatureOptInState.NOT_INITIALIZED,
        subpage.getPref(PrefName.HISTORY_SEARCH).value);
    assertFalse(toggle.checked);

    // Check ENABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageHistorySearchInteractions.HISTORY_SEARCH_ENABLED,
        AiPageActions.HISTORY_SEARCH_ENABLED);
    assertEquals(
        FeatureOptInState.ENABLED,
        subpage.getPref(PrefName.HISTORY_SEARCH).value);
    assertTrue(toggle.checked);

    metricsBrowserProxy.reset();

    // Check DISABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageHistorySearchInteractions.HISTORY_SEARCH_DISABLED,
        AiPageActions.HISTORY_SEARCH_DISABLED);
    assertEquals(
        FeatureOptInState.DISABLED,
        subpage.getPref(PrefName.HISTORY_SEARCH).value);
    assertFalse(toggle.checked);
  });

  test('historySearchLinkout', async function() {
    await createPage();

    const linkout = subpage.shadowRoot!.querySelector('cr-link-row');
    assertTrue(!!linkout);

    linkout.click();
    await assertFeatureInteractionMetrics(
        AiPageHistorySearchInteractions.FEATURE_LINK_CLICKED,
        AiPageActions.HISTORY_SEARCH_FEATURE_LINK_CLICKED);
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('historySearchDataHomeUrl'));
  });

  test('historySearchLearnMore', async () => {
    await createPage();

    const learnMoreLink = subpage.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href,
        loadTimeData.getString('historySearchLearnMoreUrl'));

    learnMoreLink.click();
    await assertFeatureInteractionMetrics(
        AiPageHistorySearchInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.HISTORY_SEARCH_LEARN_MORE_CLICKED);
  });

  test('historySearchLearnMoreManaged', async () => {
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.HISTORY_SEARCH}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING);
    await createPage();

    const learnMoreLink = subpage.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href,
        loadTimeData.getString('historySearchLearnMoreManagedUrl'));
  });

  test('historySearchTextWithAnswers', async () => {
    function checkVisibility(selector: string) {
      const element = subpage.shadowRoot!.querySelector(selector);
      assertTrue(!!element);
      return isVisible(element);
    }

    await createPage();
    assertTrue(checkVisibility('#linkoutText'));
    assertFalse(checkVisibility('#linkoutTextWithAnswers'));
    assertTrue(checkVisibility('#whenOnPageContentText'));
    assertFalse(checkVisibility('#whenOnPageContentTextWithAnswers'));
    assertFalse(checkVisibility('#whenOnLogStartItem'));
    assertTrue(checkVisibility('#considerDataEncryptedText'));
    assertFalse(checkVisibility('#considerDataEncryptedTextWithAnswers'));
    assertFalse(checkVisibility('#considerOutDatedItem'));

    loadTimeData.overrideValues({historyEmbeddingsAnswersFeatureEnabled: true});
    await createPage();
    assertFalse(checkVisibility('#linkoutText'));
    assertTrue(checkVisibility('#linkoutTextWithAnswers'));
    assertFalse(checkVisibility('#whenOnPageContentText'));
    assertTrue(checkVisibility('#whenOnPageContentTextWithAnswers'));
    assertTrue(checkVisibility('#whenOnLogStartItem'));
    assertFalse(checkVisibility('#considerDataEncryptedText'));
    assertTrue(checkVisibility('#considerDataEncryptedTextWithAnswers'));
    assertTrue(checkVisibility('#considerOutDatedItem'));
  });
});
