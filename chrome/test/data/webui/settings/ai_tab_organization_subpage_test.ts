// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsAiTabOrganizationSubpageElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, AiPageActions} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {AiPageTabOrganizationInteractions, CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('TabOrganizationSubpage', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let subpage: SettingsAiTabOrganizationSubpageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-ai-tab-organization-subpage');
    subpage.prefs = settingsPrefs.prefs;
    document.body.appendChild(subpage);
    return flushTasks();
  }

  async function assertFeatureInteractionMetrics(
      interaction: AiPageTabOrganizationInteractions, action: AiPageActions) {
    const result = await metricsBrowserProxy.whenCalled(
        'recordAiPageTabOrganizationInteractions');
    assertEquals(interaction, result);

    assertEquals(action, await metricsBrowserProxy.whenCalled('recordAction'));
  }

  test('tabOrganizationLearnMore', async () => {
    await createPage();

    const learnMoreLink = subpage.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href,
        loadTimeData.getString('tabOrganizationLearnMoreUrl'));

    learnMoreLink.click();
    await assertFeatureInteractionMetrics(
        AiPageTabOrganizationInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.TAB_ORGANIZATION_LEARN_MORE_CLICKED);
  });

  test('tabOrganizationLearnMoreManaged', async () => {
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.TAB_ORGANIZATION}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING);
    await createPage();

    const learnMoreLink = subpage.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href,
        loadTimeData.getString('tabOrganizationLearnMoreManagedUrl'));
  });
});
