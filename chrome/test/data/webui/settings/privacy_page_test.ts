// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {ClearBrowsingDataBrowserProxyImpl, CookieControlsMode, TimePeriod} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, SettingsPrivacyPageElement, SyncStatus} from 'chrome://settings/settings.js';
import {HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, PrefsBrowserProxy, PrefService, PrivacyGuideInteractions, resetRouterForTesting, Router, routes, StatusAction, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getInitialPrivacyGuideTestPrefs} from './privacy_guide_test_util.js';
import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    ...getInitialPrivacyGuideTestPrefs(),
    {
      key: 'browser.clear_data.time_period',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: TimePeriod.LAST_HOUR,
    },
    {
      key: 'browser.clear_data.browsing_history',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'browser.clear_data.cache',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'browser.clear_data.cookies',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'browser.clear_data.form_data',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'browser.clear_data.site_settings',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'browser.clear_data.download_history',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'browser.clear_data.hosted_apps_data',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
  ];
}

suite('PrivacyPage', function() {
  let page: SettingsPrivacyPageElement;
  let testClearBrowsingDataBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    resetRouterForTesting();
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);

    return microtasksFinished();
  }

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    testClearBrowsingDataBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(
        testClearBrowsingDataBrowserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    return createPage();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
    resetRouterForTesting();
  });

  test('showDeleteBrowsingDataDialog', async function() {
    assertFalse(
        !!page.shadowRoot.querySelector('settings-clear-browsing-data-dialog'));
    page.$.clearBrowsingData.click();
    await microtasksFinished();

    const dialog =
        page.shadowRoot.querySelector('settings-clear-browsing-data-dialog');
    assertTrue(!!dialog);
  });

  test('showDeletionConfirmationToast', async function() {
    assertFalse(page.$.deleteBrowsingDataToast.open);
    page.$.clearBrowsingData.click();
    await microtasksFinished();

    const dialog =
        page.shadowRoot.querySelector('settings-clear-browsing-data-dialog');
    assertTrue(!!dialog);
    dialog.fire('browsing-data-deleted', {deletionConfirmationText: 'test'});
    dialog.$.deleteBrowsingDataDialog.close();
    await eventToPromise('close', dialog);
    await microtasksFinished();

    assertTrue(page.$.deleteBrowsingDataToast.open);
    assertEquals('test', page.$.deleteBrowsingDataToast.textContent.trim());
  });

  // Test that clicking on the security page row navigates to
  // chrome://settings/security
  test('onSecurityPageClick', function() {
    page.$.securityLinkRow.click();
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });
});


suite('CookiesSubpage', function() {
  let page: SettingsPrivacyPageElement;

  suiteSetup(function() {
    resetRouterForTesting();
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);
    return microtasksFinished();
  });

  test('clickCookiesRow', async function() {
    const thirdPartyCookiesLinkRow =
        page.shadowRoot.querySelector<HTMLElement>('#thirdPartyCookiesLinkRow');
    assertTrue(!!thirdPartyCookiesLinkRow);
    thirdPartyCookiesLinkRow.click();
    // Check that the correct page was navigated to.
    await microtasksFinished();
    assertEquals(routes.COOKIES, Router.getInstance().getCurrentRoute());
  });
});

suite('CookiesSubpageRedesignDisabled', function() {
  let page: SettingsPrivacyPageElement;
  let prefService: PrefService;

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);

    return microtasksFinished();
  }

  test(
      'cookiesLinkRow', async function() {
        resetRouterForTesting();

        await createPage();

        const thirdPartyCookiesLinkRow =
            page.shadowRoot.querySelector<CrLinkRowElement>(
                '#thirdPartyCookiesLinkRow');
        assertTrue(!!thirdPartyCookiesLinkRow);
        assertEquals(
            page.i18n('thirdPartyCookiesLinkRowLabel'),
            thirdPartyCookiesLinkRow.label);

        prefService.setPrefValue(
            'profile.cookie_controls_mode', CookieControlsMode.OFF);
        await microtasksFinished();
        assertEquals(
            page.i18n('thirdPartyCookiesLinkRowSublabelEnabled'),
            thirdPartyCookiesLinkRow.subLabel);

        prefService.setPrefValue(
            'profile.cookie_controls_mode', CookieControlsMode.INCOGNITO_ONLY);
        await microtasksFinished();
        assertEquals(
            page.i18n('thirdPartyCookiesLinkRowSublabelEnabled'),
            thirdPartyCookiesLinkRow.subLabel,
        );

        prefService.setPrefValue(
            'profile.cookie_controls_mode',
            CookieControlsMode.BLOCK_THIRD_PARTY);
        await microtasksFinished();
        assertEquals(
            page.i18n('thirdPartyCookiesLinkRowSublabelDisabled'),
            thirdPartyCookiesLinkRow.subLabel);
      });

  test('cookiesLinkRowWithUniversalOptOut', async function() {
    loadTimeData.overrideValues({showUniversalOptOutSettings: true});
    resetRouterForTesting();

    await createPage();

    const thirdPartyCookiesLinkRow =
        page.shadowRoot.querySelector<CrLinkRowElement>(
            '#thirdPartyCookiesLinkRow');
    assertTrue(!!thirdPartyCookiesLinkRow);
    assertEquals(
        page.i18n('thirdPartyCookiesAndSiteDataLinkRowLabel'),
        thirdPartyCookiesLinkRow.label);
    assertEquals(
        page.i18n('thirdPartyCookiesAndSiteDataLinkRowSublabel'),
        thirdPartyCookiesLinkRow.subLabel);
  });
});

suite('PrivacyGuideRow', function() {
  let page: SettingsPrivacyPageElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    loadTimeData.overrideValues({showPrivacyGuide: true});
    resetRouterForTesting();

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    return createPage();
  });

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);
    return microtasksFinished();
  }

  test('rowNotShown', async function() {
    loadTimeData.overrideValues({showPrivacyGuide: false});
    resetRouterForTesting();

    page.remove();
    await createPage();

    assertFalse(
        loadTimeData.getBoolean('showPrivacyGuide'),
        'showPrivacyGuide was not overwritten');
    assertFalse(
        isChildVisible(page, '#privacyGuideLinkRow'),
        'privacyGuideLinkRow is visible');
  });

  test('privacyGuideRowVisibleSupervisedAccount', async function() {
    assertTrue(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user signs in to a supervised user account. This hides the privacy
    // guide entry point.
    const syncStatus: SyncStatus = {
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback('sync-status-changed', syncStatus);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user is no longer signed in to a supervised user account. This
    // doesn't show the entry point.
    syncStatus.supervisedUser = false;
    webUIListenerCallback('sync-status-changed', syncStatus);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));
  });

  test('privacyGuideRowVisibleManaged', async function() {
    assertTrue(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user becomes managed. This hides the privacy guide entry point.
    webUIListenerCallback('is-managed-changed', true);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user is no longer managed. This doesn't show the entry point.
    webUIListenerCallback('is-managed-changed', false);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));
  });

  test('privacyGuideRowClick', async function() {
    const privacyGuideLinkRow =
        page.shadowRoot.querySelector<HTMLElement>('#privacyGuideLinkRow');
    assertTrue(!!privacyGuideLinkRow);
    privacyGuideLinkRow.click();

    const result = await metricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY, result);

    // Ensure the correct route has been navigated to.
    assertEquals(routes.PRIVACY_GUIDE, Router.getInstance().getCurrentRoute());

    // Ensure the privacy guide dialog is shown.
    assertTrue(
        !!page.shadowRoot.querySelector<HTMLElement>('#privacyGuideDialog'));
  });
});

suite('HappinessTrackingSurveys', function() {
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let page: SettingsPrivacyPageElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);
    return microtasksFinished();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('ClearBrowsingDataTrigger', async function() {
    page.$.clearBrowsingData.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('CookiesTrigger', async function() {
    const thirdPartyCookiesLinkRow =
        page.shadowRoot.querySelector<HTMLElement>('#thirdPartyCookiesLinkRow');
    assertTrue(!!thirdPartyCookiesLinkRow);
    thirdPartyCookiesLinkRow.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('SecurityTrigger', async function() {
    page.$.securityLinkRow.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('SiteSettingsTrigger', async function() {
    page.$.siteSettingsLinkRow.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });
});
