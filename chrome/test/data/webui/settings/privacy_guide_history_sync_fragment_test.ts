// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {PrivacyGuideHistorySyncFragmentElement} from 'chrome://settings/lazy_load.js';
import type {SyncPrefs, SyncStatus} from 'chrome://settings/settings.js';
import {MetricsBrowserProxyImpl, loadTimeData, PrivacyGuideSettingsStates, Router, routes, SignedInState, SyncBrowserProxyImpl, syncPrefsIndividualDataTypes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('HistorySyncFragment', function() {
  let fragment: PrivacyGuideHistorySyncFragmentElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    fragment = document.createElement('privacy-guide-history-sync-fragment');
    document.body.appendChild(fragment);
    await microtasksFinished();
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
    setSyncPrefs({
      syncAllDataTypes: historySyncStartOn,
      typedUrlsSynced: historySyncStartOn,
      passwordsSynced: historySyncStartOn,
    });
    await microtasksFinished();

    if (changeSetting) {
      const toggle =
          fragment.shadowRoot.querySelector<HTMLElement>('#historyToggle');
      assertTrue(!!toggle);
      toggle.click();
      await microtasksFinished();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          historySyncStartOn ? 'Settings.PrivacyGuide.ChangeHistorySyncOff' :
                               'Settings.PrivacyGuide.ChangeHistorySyncOn');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.fire('view-exit-finish');

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  function setSyncStatus({
    signedInState,
  }: {
    signedInState: SignedInState,
  }) {
    const event: SyncStatus = {} as unknown as SyncStatus;
    event.signedInState = signedInState;
    webUIListenerCallback('sync-status-changed', event);
  }

  function setSyncPrefs({
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
    event.savedTabGroupsSynced = typedUrlsSynced;
    event.tabsSynced = typedUrlsSynced;
    event.passwordsSynced = passwordsSynced;
    webUIListenerCallback('sync-prefs-changed', event);
  }

  function setSyncTypes({
    typedUrlsSynced,
    tabsSynced,
    savedTabGroupsSynced,
  }: {
    typedUrlsSynced: boolean,
    tabsSynced: boolean,
    savedTabGroupsSynced: boolean,
  }) {
    const event: SyncPrefs = {} as unknown as SyncPrefs;
    for (const datatype of syncPrefsIndividualDataTypes) {
      (event as unknown as {[key: string]: boolean})[datatype] = true;
    }
    // Overwrite datatypes needed in tests.
    event.typedUrlsSynced = typedUrlsSynced;
    event.tabsSynced = tabsSynced;
    event.savedTabGroupsSynced = savedTabGroupsSynced;
    webUIListenerCallback('sync-prefs-changed', event);
  }

  async function assertSyncBrowserProxyCall({
    syncAllDatatypesExpected,
    typedUrlsSyncedExpected,
    tabsSyncedExpected,
    savedTabGroupsSyncedExpected,
  }: {
    syncAllDatatypesExpected: boolean,
    typedUrlsSyncedExpected: boolean,
    tabsSyncedExpected: boolean,
    savedTabGroupsSyncedExpected: boolean,
  }) {
    const syncPrefs = await syncBrowserProxy.whenCalled('setSyncDatatypes');
    assertEquals(syncAllDatatypesExpected, syncPrefs.syncAllDataTypes);
    assertEquals(typedUrlsSyncedExpected, syncPrefs.typedUrlsSynced);
    assertEquals(tabsSyncedExpected, syncPrefs.tabsSynced);
    assertEquals(savedTabGroupsSyncedExpected, syncPrefs.savedTabGroupsSynced);
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
    // `syncAllDataTypes` is only relevant when the user is syncing.
    setSyncStatus({
      signedInState: SignedInState.SYNCING,
    });

    setSyncPrefs({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    await microtasksFinished();

    fragment.$.historyToggle.click();
    await microtasksFinished();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });

    // Re-enabling history sync re-enables sync all if sync all was on before
    // and if all sync datatypes are still enabled.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: true,
      typedUrlsSyncedExpected: true,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });
  });

  test('syncAllOnDisableReenableHistorySyncOtherDatatypeOff', async function() {
    // `syncAllDataTypes` is only relevant when the user is syncing.
    setSyncStatus({
      signedInState: SignedInState.SYNCING,
    });

    setSyncPrefs({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    await microtasksFinished();

    fragment.$.historyToggle.click();
    await microtasksFinished();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });

    // The user disables another datatype in a different tab.
    setSyncPrefs({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: false,
    });
    await microtasksFinished();

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
      tabsSyncedExpected: false,
      savedTabGroupsSyncedExpected: false,
    });
  });

  test('syncAllOnDisableReenableHistorySyncWithNavigation', async function() {
    // `syncAllDataTypes` is only relevant when the user is syncing.
    setSyncStatus({
      signedInState: SignedInState.SYNCING,
    });

    setSyncPrefs({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    await microtasksFinished();

    fragment.$.historyToggle.click();
    await microtasksFinished();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });

    // The user navigates to another card, then back to the history sync card.
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=msbb'));
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=historySync'));
    await microtasksFinished();

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });
  });

  test('syncAllOffDisableReenableHistorySync', async function() {
    setSyncPrefs({
      syncAllDataTypes: false,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    await microtasksFinished();

    fragment.$.historyToggle.click();
    await microtasksFinished();
    await assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
      tabsSyncedExpected: false,
      savedTabGroupsSyncedExpected: false,
    });

    // Re-enabling history sync doesn't re-enable sync all if sync all wasn't on
    // originally.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });
  });

  test('syncAllOffEnableHistorySync', async function() {
    setSyncPrefs({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: true,
    });
    await microtasksFinished();

    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });
  });

  test('syncingUserInitialToggleValue', async function() {
    setSyncStatus({
      signedInState: SignedInState.SYNCING,
    });
    setSyncTypes({
      typedUrlsSynced: false,
      tabsSynced: false,
      savedTabGroupsSynced: false,
    });
    await microtasksFinished();
    // The toggle is only set based on history sync state.
    assertFalse(fragment.$.historyToggle.checked);
    setSyncTypes({
      typedUrlsSynced: false,
      tabsSynced: true,
      savedTabGroupsSynced: true,
    });
    await microtasksFinished();
    assertFalse(fragment.$.historyToggle.checked);
    setSyncTypes({
      typedUrlsSynced: true,
      tabsSynced: false,
      savedTabGroupsSynced: false,
    });
    await microtasksFinished();
    assertTrue(fragment.$.historyToggle.checked);
  });

  test('syncingUserToggleOn', async function() {
    setSyncStatus({
      signedInState: SignedInState.SYNCING,
    });
    setSyncTypes({
      typedUrlsSynced: false,
      tabsSynced: false,
      savedTabGroupsSynced: false,
    });
    await microtasksFinished();
    assertFalse(fragment.$.historyToggle.checked);
    // Toggle on.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
      tabsSyncedExpected: false,
      savedTabGroupsSyncedExpected: false,
    });
  });

  test('syncingUserToggleOff', async function() {
    setSyncStatus({
      signedInState: SignedInState.SYNCING,
    });
    setSyncTypes({
      typedUrlsSynced: true,
      tabsSynced: true,
      savedTabGroupsSynced: true,
    });
    await microtasksFinished();
    assertTrue(fragment.$.historyToggle.checked);
    // Toggle off.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });
  });

  test('signedInUserInitialToggleValue', async function() {
    setSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
    });
    setSyncTypes({
      typedUrlsSynced: false,
      tabsSynced: false,
      savedTabGroupsSynced: false,
    });
    await microtasksFinished();
    // The toggle is set if any of (history, tabs, saved tab groups) types is
    // enabled.
    assertFalse(fragment.$.historyToggle.checked);
    setSyncTypes({
      typedUrlsSynced: true,
      tabsSynced: false,
      savedTabGroupsSynced: false,
    });
    await microtasksFinished();
    assertTrue(fragment.$.historyToggle.checked);
    setSyncTypes({
      typedUrlsSynced: false,
      tabsSynced: true,
      savedTabGroupsSynced: false,
    });
    await microtasksFinished();
    assertTrue(fragment.$.historyToggle.checked);
    setSyncTypes({
      typedUrlsSynced: false,
      tabsSynced: false,
      savedTabGroupsSynced: true,
    });
    await microtasksFinished();
    assertTrue(fragment.$.historyToggle.checked);
  });

  test('signedInUserToggleOn', async function() {
    setSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
    });
    setSyncTypes({
      typedUrlsSynced: false,
      tabsSynced: false,
      savedTabGroupsSynced: false,
    });
    await microtasksFinished();
    assertFalse(fragment.$.historyToggle.checked);
    // Toggle on.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
      tabsSyncedExpected: true,
      savedTabGroupsSyncedExpected: true,
    });
  });

  test('signedInUserToggleOff', async function() {
    setSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
    });
    setSyncTypes({
      typedUrlsSynced: true,
      tabsSynced: true,
      savedTabGroupsSynced: true,
    });
    await microtasksFinished();
    assertTrue(fragment.$.historyToggle.checked);
    // Toggle off.
    fragment.$.historyToggle.click();
    await microtasksFinished();
    return assertSyncBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
      tabsSyncedExpected: false,
      savedTabGroupsSyncedExpected: false,
    });
  });

  test('stringUpdatesOnSyncStateChange', async function() {
    function getCardHeader() {
      const header =
          fragment.shadowRoot.querySelector('.settings-fragment-header-label');
      assertTrue(!!header);
      return header.textContent.trim();
    }
    function getFeatureDescription1() {
      const li = fragment.shadowRoot.querySelector('.icon-bulleted-list li');
      assertTrue(!!li);
      const secondary = li.querySelector('.secondary');
      assertTrue(!!secondary);
      return secondary.textContent.trim();
    }

    // The user is syncing.
    setSyncStatus({
      signedInState: SignedInState.SYNCING,
    });
    await microtasksFinished();
    assertEquals(
        getCardHeader(),
        loadTimeData.getString('privacyGuideHistorySyncCardHeader'));
    assertEquals(
        fragment.$.historyToggle.label,
        loadTimeData.getString('privacyGuideHistorySyncSettingLabel'));
    assertEquals(
        getFeatureDescription1(),
        loadTimeData.getString('privacyGuideHistorySyncFeatureDescription1'));

    // The user is signed in non-syncing.
    setSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
    });
    await microtasksFinished();
    assertEquals(
        getCardHeader(),
        loadTimeData.getString('privacyGuideHistoryAndTabsSyncCardHeader'));
    assertEquals(
        fragment.$.historyToggle.label,
        loadTimeData.getString('privacyGuideHistoryAndTabsSyncSettingLabel'));
    assertEquals(
        getFeatureDescription1(),
        loadTimeData.getString(
            'privacyGuideHistoryAndTabsSyncFeatureDescription1'));
  });
});
