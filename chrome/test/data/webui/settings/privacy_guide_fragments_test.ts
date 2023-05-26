// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookiePrimarySetting, PrivacyGuideCompletionFragmentElement, PrivacyGuideCookiesFragmentElement, PrivacyGuideHistorySyncFragmentElement, PrivacyGuideMsbbFragmentElement, PrivacyGuideSafeBrowsingFragmentElement, PrivacyGuideWelcomeFragmentElement, SafeBrowsingSetting, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyGuideSettingsStates, Router, routes, SettingsPrefsElement, SyncBrowserProxyImpl, SyncPrefs, syncPrefsIndividualDataTypes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

/** Fire a sign in status change event and flush the UI. */
function setSignInState(signedIn: boolean) {
  const event = {
    signedIn: signedIn,
  };
  webUIListenerCallback('update-sync-state', event);
  flush();
}

suite('WelcomeFragmentTests', function() {
  let fragment: PrivacyGuideWelcomeFragmentElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fragment = document.createElement('privacy-guide-welcome-fragment');
    document.body.appendChild(fragment);
    return flushTasks();
  });

  teardown(function() {
    fragment.remove();
  });

  test('nextNavigation', async function() {
    const nextEventPromise = eventToPromise('start-button-click', fragment);

    fragment.$.startButton.click();

    // Ensure the event is sent.
    return nextEventPromise;
  });
});

suite('MsbbFragmentTests', function() {
  let fragment: PrivacyGuideMsbbFragmentElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fragment = document.createElement('privacy-guide-msbb-fragment');
    fragment.prefs = settingsPrefs.prefs!;
    document.body.appendChild(fragment);

    return flushTasks();
  });

  async function assertMsbbMetrics({
    msbbStartOn,
    changeSetting,
    expectedMetric,
  }: {
    msbbStartOn: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    fragment.set(
        'prefs.url_keyed_anonymized_data_collection.enabled.value',
        msbbStartOn);

    // The fragment is informed that it becomes visible by a receiving
    // a view-enter-start event.
    fragment.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

    if (changeSetting) {
      fragment.shadowRoot!.querySelector<HTMLElement>(
                              '#urlCollectionToggle')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          msbbStartOn ? 'Settings.PrivacyGuide.ChangeMSBBOff' :
                        'Settings.PrivacyGuide.ChangeMSBBOn');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.dispatchEvent(
        new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('msbbMetricsOnToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_ON,
    });
  });

  test('msbbMetricsOnToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_OFF,
    });
  });

  test('msbbMetricsOffToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_ON,
    });
  });

  test('msbbMetricsOffToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF,
    });
  });
});

suite('HistorySyncFragmentTests', function() {
  let fragment: PrivacyGuideHistorySyncFragmentElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

suite('SafeBrowsingFragmentTests', function() {
  let fragment: PrivacyGuideSafeBrowsingFragmentElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fragment = document.createElement('privacy-guide-safe-browsing-fragment');
    fragment.prefs = settingsPrefs.prefs!;
    document.body.appendChild(fragment);

    return flushTasks();
  });

  async function assertSafeBrowsingMetrics({
    safeBrowsingStartsEnhanced,
    changeSetting,
    expectedMetric,
  }: {
    safeBrowsingStartsEnhanced: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const safeBrowsingStartState = safeBrowsingStartsEnhanced ?
        SafeBrowsingSetting.ENHANCED :
        SafeBrowsingSetting.STANDARD;
    fragment.set('prefs.generated.safe_browsing.value', safeBrowsingStartState);

    // The fragment is informed that it becomes visible by a receiving
    // a view-enter-start event.
    fragment.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

    if (changeSetting) {
      fragment.shadowRoot!
          .querySelector<HTMLElement>(
              safeBrowsingStartsEnhanced ?
                  '#safeBrowsingRadioStandard' :
                  '#safeBrowsingRadioEnhanced')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          safeBrowsingStartsEnhanced ?
              'Settings.PrivacyGuide.ChangeSafeBrowsingStandard' :
              'Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.dispatchEvent(
        new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('safeBrowsingMetricsEnhancedToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD,
    });
  });

  test('safeBrowsingMetricsStandardToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED,
    });
  });

  test('safeBrowsingMetricsStandardToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD,
    });
  });

  test('safeBrowsingMetricsEnhancedToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED,
    });
  });

  test('fragmentUpdatesFromSafeBrowsingChanges', async function() {
    const radioButtonGroup =
        fragment.shadowRoot!.querySelector<SettingsRadioGroupElement>(
            '#safeBrowsingRadioGroup');
    assertTrue(!!radioButtonGroup);

    fragment.set(
        'prefs.generated.safe_browsing.value', SafeBrowsingSetting.ENHANCED);
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.ENHANCED);

    fragment.set(
        'prefs.generated.safe_browsing.value', SafeBrowsingSetting.STANDARD);
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.STANDARD);
  });
});

suite('CookiesFragmentTests', function() {
  let fragment: PrivacyGuideCookiesFragmentElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fragment = document.createElement('privacy-guide-cookies-fragment');
    fragment.prefs = settingsPrefs.prefs!;
    document.body.appendChild(fragment);

    return flushTasks();
  });

  async function assertCookieMetrics({
    cookieStartsBlock3PIncognito,
    changeSetting,
    expectedMetric,
  }: {
    cookieStartsBlock3PIncognito: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const cookieStartState = cookieStartsBlock3PIncognito ?
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO :
        CookiePrimarySetting.BLOCK_THIRD_PARTY;
    fragment.set(
        'prefs.generated.cookie_primary_setting.value', cookieStartState);

    // The fragment is informed that it becomes visible by a receiving
    // a view-enter-start event.
    fragment.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

    if (changeSetting) {
      fragment.shadowRoot!
          .querySelector<HTMLElement>(
              cookieStartsBlock3PIncognito ? '#block3P' :
                                             '#block3PIncognito')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          cookieStartsBlock3PIncognito ?
              'Settings.PrivacyGuide.ChangeCookiesBlock3P' :
              'Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.dispatchEvent(
        new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('cookiesMetrics3PIncognitoTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PIncognitoTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P,
    });
  });

  test('cookiesMetrics3PTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P,
    });
  });

  test('fragmentUpdatesFromCookieChanges', async function() {
    const radioButtonGroup =
        fragment.shadowRoot!.querySelector<SettingsRadioGroupElement>(
            '#cookiesRadioGroup')!;

    fragment.set(
        'prefs.generated.cookie_primary_setting.value',
        CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY);

    fragment.set(
        'prefs.generated.cookie_primary_setting.value',
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);
  });
});

suite('CompletionFragmentTests', function() {
  let fragment: PrivacyGuideCompletionFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
      isPrivacySandboxRestrictedNoticeEnabled: false,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fragment = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(fragment);

    return flushTasks();
  });

  teardown(function() {
    fragment.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('backNavigation', async function() {
    const nextEventPromise = eventToPromise('back-button-click', fragment);

    fragment.$.backButton.click();

    // Ensure the event is sent.
    return nextEventPromise;
  });

  test('backToSettingsNavigation', async function() {
    const closeEventPromise = eventToPromise('close', fragment);

    fragment.shadowRoot!.querySelector<HTMLElement>('#leaveButton')!.click();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCompletion');

    // Ensure the |close| event has been sent.
    return closeEventPromise;
  });

  test('SWAALinkClick', async function() {
    setSignInState(true);

    assertTrue(isChildVisible(fragment, '#waaRow'));
    fragment.shadowRoot!.querySelector<HTMLElement>('#waaRow')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.SWAA_COMPLETION_LINK, result);
  });

  test('privacySandboxLinkClick', async function() {
    fragment.shadowRoot!.querySelector<HTMLElement>(
                            '#privacySandboxRow')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(
        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK, result);
  });

  test('updateFragmentFromSignIn', function() {
    setSignInState(true);
    assertTrue(isChildVisible(fragment, '#privacySandboxRow'));
    assertTrue(isChildVisible(fragment, '#waaRow'));

    // Sign the user out and expect the waa row to no longer be visible.
    setSignInState(false);
    assertTrue(isChildVisible(fragment, '#privacySandboxRow'));
    assertFalse(isChildVisible(fragment, '#waaRow'));
  });
});

suite('CompletionFragmentPrivacySandboxRestricted', function() {
  let fragment: PrivacyGuideCompletionFragmentElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
      isPrivacySandboxRestrictedNoticeEnabled: false,
    });
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fragment = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(fragment);

    return flushTasks();
  });

  teardown(function() {
    fragment.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('updateFragmentFromSignIn', function() {
    setSignInState(true);
    assertFalse(isChildVisible(fragment, '#privacySandboxRow'));
    assertTrue(isChildVisible(fragment, '#waaRow'));
    const subheader =
        fragment.shadowRoot!.querySelector<HTMLElement>('.cr-secondary-text')!;
    assertEquals(
        fragment.i18n('privacyGuideCompletionCardSubHeader'),
        subheader.innerText);

    setSignInState(false);
    assertFalse(isChildVisible(fragment, '#privacySandboxRow'));
    assertFalse(isChildVisible(fragment, '#waaRow'));
    assertEquals(
        fragment.i18n('privacyGuideCompletionCardSubHeaderNoLinks'),
        subheader.innerText);
  });
});


suite(
    'CompletionFragmentPrivacySandboxRestrictedWithNoticeEnabled', function() {
      let fragment: PrivacyGuideCompletionFragmentElement;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          isPrivacySandboxRestricted: true,
          isPrivacySandboxRestrictedNoticeEnabled: true,
        });
      });

      setup(function() {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        fragment = document.createElement('privacy-guide-completion-fragment');
        document.body.appendChild(fragment);

        return flushTasks();
      });

      teardown(function() {
        fragment.remove();
        // The browser instance is shared among the tests, hence the route needs
        // to be reset between tests.
        Router.getInstance().navigateTo(routes.BASIC);
      });

      test('privacySandboxRowVisibility', function() {
        assertTrue(isChildVisible(fragment, '#privacySandboxRow'));
      });
    });
