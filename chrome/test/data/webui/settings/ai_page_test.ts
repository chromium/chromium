// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {EntityDataManagerProxyImpl, FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, SettingsAiPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {AiPageInteractions, CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('AiPage', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let page: SettingsAiPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let entityDataManager: TestEntityDataManagerProxy;

  suiteSetup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    loadTimeData.overrideValues({
      showAiPage: true,
      showAiPageAiFeatureSection: true,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
    metricsBrowserProxy.reset();
    openWindowProxy.reset();
  });

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-ai-page');
    page.prefs = settingsPrefs.prefs;
    Router.getInstance().navigateTo(routes.AI);
    document.body.appendChild(page);
    return flushTasks();
  }

  async function verifyFeatureVisibilityMetrics(
      histogramName: string, visible: boolean) {
    const recordedHistograms =
        await metricsBrowserProxy.getArgs('recordBooleanHistogram');
    assertTrue(recordedHistograms.some(
        histogram =>
            histogramName === histogram[0] && visible === histogram[1]));
  }

  async function verifyFeatureInteractionMetrics(
      interaction: AiPageInteractions, action: string) {
    const result =
        await metricsBrowserProxy.whenCalled('recordAiPageInteractions');
    assertEquals(interaction, result);

    assertEquals(action, await metricsBrowserProxy.whenCalled('recordAction'));
  }

  test('FeatureRowsVisibility', async () => {
    // Case 1, a subset of the controls should be visible.
    loadTimeData.overrideValues({
      showHistorySearchControl: false,
      showCompareControl: true,
      showComposeControl: true,
      showTabOrganizationControl: false,
      showPasswordChangeControl: false,
    });
    resetRouterForTesting();
    await createPage();

    assertEquals(5, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));

    assertFalse(isChildVisible(page, '#historySearchRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.HistorySearch', false);

    assertTrue(isChildVisible(page, '#compareRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.Compare', true);

    assertTrue(isChildVisible(page, '#composeRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.Compose', true);

    assertFalse(isChildVisible(page, '#tabOrganizationRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.TabOrganization', false);

    assertFalse(isChildVisible(page, '#passwordChangeRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.PasswordChange', false);

    metricsBrowserProxy.resetResolver('recordBooleanHistogram');

    // No new metrics should get recorded on next AI page navigation.
    Router.getInstance().navigateTo(routes.AI);
    assertEquals(0, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));

    // Case 2, a different subset of the controls should be visible.
    loadTimeData.overrideValues({
      showHistorySearchControl: true,
      showCompareControl: false,
      showComposeControl: false,
      showTabOrganizationControl: true,
      showPasswordChangeControl: true,
    });
    resetRouterForTesting();
    await createPage();
    assertEquals(5, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));

    assertTrue(isChildVisible(page, '#historySearchRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.HistorySearch', true);

    assertFalse(isChildVisible(page, '#compareRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.Compare', false);

    assertFalse(isChildVisible(page, '#composeRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.Compose', false);

    assertTrue(isChildVisible(page, '#tabOrganizationRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.TabOrganization', true);

    assertTrue(isChildVisible(page, '#passwordChangeRowV2'));
    await verifyFeatureVisibilityMetrics(
        'Settings.AiPage.ElementVisibility.PasswordChange', true);

    metricsBrowserProxy.resetResolver('recordBooleanHistogram');

    // No new metrics should get recorded on next AI page navigation.
    Router.getInstance().navigateTo(routes.AI);
    assertEquals(0, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));
  });

  test('historySearchRow', async () => {
    loadTimeData.overrideValues({
      showAiPage: true,
      showHistorySearchControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const historySearchRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>('#historySearchRowV2');

    assertTrue(!!historySearchRow);
    assertTrue(isVisible(historySearchRow));

    page.setPrefValue(
        PrefName.HISTORY_SEARCH, FeatureOptInState.NOT_INITIALIZED);
    assertEquals(
        loadTimeData.getString('historySearchSublabelOff'),
        historySearchRow.subLabel);

    page.setPrefValue(PrefName.HISTORY_SEARCH, FeatureOptInState.DISABLED);
    assertEquals(
        loadTimeData.getString('historySearchSublabelOff'),
        historySearchRow.subLabel);

    page.setPrefValue(PrefName.HISTORY_SEARCH, FeatureOptInState.ENABLED);
    assertEquals(
        loadTimeData.getString('historySearchSublabelOn'),
        historySearchRow.subLabel);

    historySearchRow.click();
    await verifyFeatureInteractionMetrics(
        AiPageInteractions.HISTORY_SEARCH_CLICK,
        'Settings.AiPage.HistorySearchEntryPointClick');

    const currentRoute = Router.getInstance().getCurrentRoute();
    assertEquals(routes.HISTORY_SEARCH, currentRoute);
    assertEquals(routes.AI, currentRoute.parent);
  });

  test('compareRow', async () => {
    loadTimeData.overrideValues({
      showAiPage: true,
      showCompareControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const compareRow =
        page.shadowRoot!.querySelector<HTMLElement>('#compareRowV2');

    assertTrue(!!compareRow);
    assertTrue(isVisible(compareRow));
    compareRow.click();
    await verifyFeatureInteractionMetrics(
        AiPageInteractions.COMPARE_CLICK,
        'Settings.AiPage.CompareEntryPointClick');

    const currentRoute = Router.getInstance().getCurrentRoute();
    assertEquals(routes.COMPARE, currentRoute);
    assertEquals(routes.AI, currentRoute.parent);
  });

  test('composeRow', async () => {
    loadTimeData.overrideValues({
      showAiPage: true,
      showComposeControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const composeRow =
        page.shadowRoot!.querySelector<HTMLElement>('#composeRowV2');

    assertTrue(!!composeRow);
    assertTrue(isVisible(composeRow));
    composeRow.click();
    await verifyFeatureInteractionMetrics(
        AiPageInteractions.COMPOSE_CLICK,
        'Settings.AiPage.ComposeEntryPointClick');

    const currentRoute = Router.getInstance().getCurrentRoute();
    assertEquals(routes.OFFER_WRITING_HELP, currentRoute);
    assertEquals(routes.AI, currentRoute.parent);
  });

  test('tabOrganizationRow', async () => {
    loadTimeData.overrideValues({
      showAiPage: true,
      showTabOrganizationControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const tabOrganizationRow =
        page.shadowRoot!.querySelector<HTMLElement>('#tabOrganizationRowV2');

    assertTrue(!!tabOrganizationRow);
    assertTrue(isVisible(tabOrganizationRow));
    tabOrganizationRow.click();
    await verifyFeatureInteractionMetrics(
        AiPageInteractions.TAB_ORGANIZATION_CLICK,
        'Settings.AiPage.TabOrganizationEntryPointClick');

    assertEquals(
        routes.AI_TAB_ORGANIZATION, Router.getInstance().getCurrentRoute());
  });

  test('PasswordChangeRow', async () => {
    loadTimeData.overrideValues({
      showPasswordChangeControl: true,
    });
    await createPage();

    const passwordChangeRow =
        page.shadowRoot!.querySelector<HTMLElement>('#passwordChangeRowV2');
    assertTrue(!!passwordChangeRow);
    assertTrue(isVisible(passwordChangeRow));

    passwordChangeRow.click();
    await verifyFeatureInteractionMetrics(
        AiPageInteractions.PASSWORD_CHANGE_CLICK,
        'Settings.AiPage.PasswordChangeEntryPointClick');

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('passwordChangeSettingsUrl'));
  });

  test('NoPasswordChangeRowWhenFeatureDisabled', async () => {
    loadTimeData.overrideValues({
      showPasswordChangeControl: false,
    });
    await createPage();

    const passwordChangeRow =
        page.shadowRoot!.querySelector<HTMLElement>('#passwordChangeRowV2');
    assertTrue(!!passwordChangeRow);
    assertFalse(isVisible(passwordChangeRow));
  });
});
