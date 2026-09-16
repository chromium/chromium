// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsPrivacyPageIndexElement, SettingsPrefsElement, SyncStatus} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, PrefService, PrefsBrowserProxy, PrivacyGuideBrowserProxyImpl, PrivacyGuideInteractions, resetRouterForTesting, routes, Router, StatusAction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestPrivacyGuideBrowserProxy} from './test_privacy_guide_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// TODO(crbug.com/40184479): Remove once the privacy guide promo has been
// removed.
suite('PrivacyGuidePromoVisibility', () => {
  let page: SettingsPrivacyPageIndexElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let privacyGuideBrowserProxy: TestPrivacyGuideBrowserProxy;
  let prefService: PrefService;

  suiteSetup(function() {
    loadTimeData.overrideValues({showPrivacyGuide: true});
    resetRouterForTesting();
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    privacyGuideBrowserProxy = new TestPrivacyGuideBrowserProxy();
    PrivacyGuideBrowserProxyImpl.setInstance(privacyGuideBrowserProxy);
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    const initialPrefs = [
      {
        key: 'privacy_guide.viewed',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    ];
    const prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(prefsBrowserProxy.fakeApi);
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    Router.getInstance().navigateTo(routes.PRIVACY);

    page = document.createElement('settings-privacy-page-index');
    page.prefs = settingsPrefs.prefs!;
    // TODO(crbug.com/40184479): Temporary bridge to notify the unmigrated
    // Polymer parent element of pref changes from Lit child elements until
    // settings-privacy-page-index is migrated to PrefService.
    prefsBrowserProxy.fakeApi.onPrefsChanged.addListener(
        (prefs: chrome.settingsPrivate.PrefObject[]) => {
          for (const pref of prefs) {
            page.setPrefValue(pref.key, pref.value);
          }
        });
    document.body.appendChild(page);
    await flushTasks();

    // Necessary for isChildVisible() calls below to not behave flakily.
    return page.whenViewSwitchingDone();
  });

  test('Visibility', async function() {
    await privacyGuideBrowserProxy.whenCalled('incrementPromoImpressionCount');
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('VisibilitySupervisedAccount', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // The user signs in to a supervised user account. This hides the privacy
    // guide promo.
    let syncStatus: SyncStatus = {
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback('sync-status-changed', syncStatus);
    await flushTasks();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));

    // The user is no longer signed in to a supervised user account. This
    // doesn't show the promo.
    syncStatus = {supervisedUser: false, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);
    await flushTasks();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('VisibilityManaged', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // The user becomes managed. This hides the privacy guide promo.
    webUIListenerCallback('is-managed-changed', true);
    await flushTasks();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));

    // The user is no longer managed. This doesn't show the promo.
    webUIListenerCallback('is-managed-changed', false);
    await flushTasks();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('NoThanksButton', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));
    assertFalse(prefService.getPref<boolean>('privacy_guide.viewed').value);

    // Click the no thanks button.
    const privacyGuidePromo = page.shadowRoot!.querySelector<HTMLElement>(
        '#privacyGuidePromo settings-privacy-guide-promo');
    assertTrue(!!privacyGuidePromo);
    const noThanksButton =
        privacyGuidePromo.shadowRoot!.querySelector<HTMLElement>(
            '#noThanksButton');
    assertTrue(!!noThanksButton);
    noThanksButton.click();
    await flushTasks();

    // The privacy guide should be marked as seen and the promo no longer
    // visible.
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('StartMetrics', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // Click the start button.
    const privacyGuidePromo = page.shadowRoot!.querySelector<HTMLElement>(
        '#privacyGuidePromo settings-privacy-guide-promo');
    assertTrue(!!privacyGuidePromo);
    const startButton =
        privacyGuidePromo.shadowRoot!.querySelector<HTMLElement>(
            '#startButton');
    assertTrue(!!startButton);
    startButton.click();
    await flushTasks();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(result, PrivacyGuideInteractions.PROMO_ENTRY);
  });
});
