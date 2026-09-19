// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsHistorySearchPageElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, AiPageActions, FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import {AiPageHistorySearchInteractions, loadTimeData, MetricsBrowserProxyImpl, ModelExecutionEnterprisePolicyValue, OpenWindowProxyImpl, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: PrefName.HISTORY_SEARCH,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: FeatureOptInState.NOT_INITIALIZED,
    },
    {
      key: AiEnterpriseFeaturePrefName.HISTORY_SEARCH,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ModelExecutionEnterprisePolicyValue.ALLOW,
    },
  ];
}

suite('HistorySearchSubpage', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsHistorySearchPageElement;
  let prefService: PrefService;

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-history-search-page');
    document.body.appendChild(subpage);
    return microtasksFinished();
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

    const toggle = subpage.shadowRoot.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    const policyIndicator =
        subpage.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertFalse(!!policyIndicator);

    // Check NOT_INITIALIZED case.
    assertEquals(
        FeatureOptInState.NOT_INITIALIZED,
        prefService.getPref(PrefName.HISTORY_SEARCH).value);
    assertFalse(toggle.checked);

    // Check ENABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageHistorySearchInteractions.HISTORY_SEARCH_ENABLED,
        AiPageActions.HISTORY_SEARCH_ENABLED);
    assertEquals(
        FeatureOptInState.ENABLED,
        prefService.getPref(PrefName.HISTORY_SEARCH).value);
    assertTrue(toggle.checked);

    metricsBrowserProxy.reset();

    // Check DISABLED case.
    toggle.click();
    await assertFeatureInteractionMetrics(
        AiPageHistorySearchInteractions.HISTORY_SEARCH_DISABLED,
        AiPageActions.HISTORY_SEARCH_DISABLED);
    assertEquals(
        FeatureOptInState.DISABLED,
        prefService.getPref(PrefName.HISTORY_SEARCH).value);
    assertFalse(toggle.checked);
  });

  test('historySearchToggleDisabled', async () => {
    await prefService.setPrefValue(
        AiEnterpriseFeaturePrefName.HISTORY_SEARCH,
        ModelExecutionEnterprisePolicyValue.DISABLE);
    await createPage();

    const indicator =
        subpage.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);

    const toggle = subpage.shadowRoot.querySelector('settings-toggle-button');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);

    const linkout = subpage.shadowRoot.querySelector('cr-link-row');
    assertFalse(!!linkout);
  });

  test('historySearchLinkout', async function() {
    await createPage();

    const linkout = subpage.shadowRoot.querySelector('cr-link-row');
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

    const learnMoreLink = subpage.shadowRoot.querySelector('a');
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
    await prefService.setPrefValue(
        AiEnterpriseFeaturePrefName.HISTORY_SEARCH,
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING);
    await createPage();

    const learnMoreLink = subpage.shadowRoot.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href,
        loadTimeData.getString('historySearchLearnMoreManagedUrl'));
  });

  test('historySearchTextWithAnswers', async () => {
    function checkVisibility(selector: string) {
      const element = subpage.shadowRoot.querySelector(selector);
      assertTrue(!!element);
      return isVisible(element);
    }

    await createPage();
    const linkout =
        subpage.shadowRoot.querySelector<HTMLElement>('#linkoutText');
    assertTrue(!!linkout);
    assertEquals(
        loadTimeData.getString('historySearchSettingSublabelV2') +
            loadTimeData.getString('sentenceEnd'),
        linkout.innerText.trim());

    assertTrue(checkVisibility('#whenOnPageContentText'));
    assertFalse(checkVisibility('#whenOnPageContentTextWithAnswers'));
    assertFalse(checkVisibility('#whenOnRecallInfoWithAnswers'));
    assertTrue(checkVisibility('#whenOnLogStartItem'));
    assertTrue(checkVisibility('#considerDataEncryptedText'));
    assertFalse(checkVisibility('#considerDataEncryptedTextWithAnswers'));
    assertFalse(checkVisibility('#considerOutDatedItem'));

    loadTimeData.overrideValues({historyEmbeddingsAnswersFeatureEnabled: true});
    await createPage();
    const linkoutWithAnswers =
        subpage.shadowRoot.querySelector<HTMLElement>('#linkoutText');
    assertTrue(!!linkoutWithAnswers);
    assertEquals(
        loadTimeData.getString('historySearchWithAnswersSettingSublabelV2') +
            loadTimeData.getString('sentenceEnd'),
        linkoutWithAnswers.innerText.trim());

    assertFalse(checkVisibility('#whenOnPageContentText'));
    assertTrue(checkVisibility('#whenOnPageContentTextWithAnswers'));
    assertTrue(checkVisibility('#whenOnRecallInfoWithAnswers'));
    assertTrue(checkVisibility('#whenOnLogStartItem'));
    assertFalse(checkVisibility('#considerDataEncryptedText'));
    assertTrue(checkVisibility('#considerDataEncryptedTextWithAnswers'));
    assertTrue(checkVisibility('#considerOutDatedItem'));
  });
});
