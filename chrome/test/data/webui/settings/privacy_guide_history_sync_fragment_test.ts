// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {PrivacyGuideHistorySyncFragmentElement} from 'chrome://settings/lazy_load.js';
import type {SyncPrefs} from 'chrome://settings/settings.js';
import {MetricsBrowserProxyImpl, PrivacyGuideSettingsStates, Router, routes, SyncBrowserProxyImpl, syncPrefsIndividualDataTypes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('HistorySyncFragment', function() {
  let fragment: PrivacyGuideHistorySyncFragmentElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    fragment = document.createElement('privacy-guide-history-sync-fragment');
    document.body.appendChild(fragment);

    return flushTasks();
  });

  async function assertHistorySyncMetrics({
    historySyncStartOn,
    changeSetting,
    expectedMetric,
  }: {
    historySyncStartOn: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    setSyncStatus({
      syncAllDataTypes: historySyncStartOn,
      typedUrlsSynced: historySyncStartOn,
      passwordsSynced: historySyncStartOn,
    });

    if (changeSetting) {
      fragment.shadowRoot!.querySelector<HTMLElement>(
                              '#historyToggle')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          historySyncStartOn ? 'Settings.PrivacyGuide.ChangeHistorySyncOff' :
                               'Settings.PrivacyGuide.ChangeHistorySyncOn');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.dispatchEvent(
        new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  function setSyncStatus({
    syncAllDataTypes,
    typedUrlsSynced,
    passwordsSynced,
  }: {
    syncAllDataTypes: boolean,
    typedUrlsSynced: boolean,
    passwordsSynced: boolean,
  }) {
    if (syncAllDataTypes) {
      assertTrue(typedUrlsSynced);
      assertTrue(passwordsSynced);
    }
    const event: SyncPrefs = {} as unknown as SyncPrefs;
    for (const datatype of syncPrefsIndividualDataTypes) {
      (event as unknown as {[key: string]: boolean})[datatype] = true;
    }
    // Overwrite datatypes needed in tests.
    event.syncAllDataTypes = syncAllDataTypes;
    event.typedUrlsSynced = typedUrlsSynced;
    event.passwordsSynced = passwordsSynced;
    webUIListenerCallback('sync-prefs-changed', event);
  }

  async function assertSyncBrowserProxyCall({
    syncAllDatatypesExpected,
    typedUrlsSyncedExpected,
  }: {
    syncAllDatatypesExpected: boolean,
    typedUrlsSyncedExpected: boolean,
  }) {
    const syncPrefs = await syncBrowserProxy.whenCalled('setSyncDatatypes');
    assertEquals(syncAllDatatypesExpected, syncPrefs.syncAllDataTypes);
    assertEquals(typedUrlsSyncedExpected, syncPrefs.typedUrlsSynced);
    syncBrowserProxy.resetResolver('setSyncDatatypes');
  }

  test('historySyncMetricsOnToOn', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: true,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON,
    });
  });

  test('historySyncMetricsOnToOff', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF,
    });
  });

  test('historySyncMetricsOffToOn', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON,
    });
  });

  test('historySyncMetricsOffToOff', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF,
    });
  });

  test('syncAllOnDisableReenableHistorySync', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    fragment.$.historyToggle.click();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync re-enables sync all if sync all was on before
    // and if all sync datatypes are still enabled.
    fragment.$.historyToggle.click();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: true,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOnDisableReenableHistorySyncOtherDatatypeOff', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    fragment.$.historyToggle.click();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // The user disables another datatype in a different tab.
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: false,
    });

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    fragment.$.historyToggle.click();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOnDisableReenableHistorySyncWithNavigation', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    fragment.$.historyToggle.click();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // The user navigates to another card, then back to the history sync card.
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=msbb'));
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=historySync'));

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    fragment.$.historyToggle.click();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOffDisableReenableHistorySync', async function() {
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    fragment.$.historyToggle.click();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync doesn't re-enable sync all if sync all wasn't on
    // originally.
    fragment.$.historyToggle.click();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOffEnableHistorySync', function() {
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: true,
    });
    fragment.$.historyToggle.click();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });
});
