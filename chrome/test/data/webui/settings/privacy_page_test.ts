// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrToastElement} from 'chrome://settings/lazy_load.js';
import {ClearBrowsingDataBrowserProxyImpl, CookieControlsMode} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, SettingsPrefsElement, SettingsPrivacyPageElement, SyncStatus} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, PrivacyGuideInteractions, resetRouterForTesting, Router, routes, StatusAction, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue, assertThrows} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';



// clang-format on

suite('PrivacyPage', function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testClearBrowsingDataBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
    });
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);

    return flushTasks();
  }

  setup(function() {
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

  test('showDeleteBrowsingDataDialog', function() {
    assertFalse(!!page.shadowRoot!.querySelector(
        'settings-clear-browsing-data-dialog-v2'));
    page.$.clearBrowsingData.click();
    flush();

    const dialog = page.shadowRoot!.querySelector(
        'settings-clear-browsing-data-dialog-v2');
    assertTrue(!!dialog);
  });

  test('showDeletionConfirmationToast', async function() {
    const toast = page.shadowRoot!.querySelector<CrToastElement>(
        '#deleteBrowsingDataToast');
    assertTrue(!!toast);
    assertFalse(toast.open);
    page.$.clearBrowsingData.click();
    flush();

    const dialog = page.shadowRoot!.querySelector(
        'settings-clear-browsing-data-dialog-v2');
    assertTrue(!!dialog);
    dialog.dispatchEvent(new CustomEvent('browsing-data-deleted', {
      bubbles: true,
      composed: true,
      detail: {deletionConfirmationText: 'test'},
    }));
    dialog.$.deleteBrowsingDataDialog.close();
    await eventToPromise('close', dialog);
    flush();

    assertTrue(toast.open);
    assertEquals('test', toast.textContent.trim());
  });

  // Test that clicking on the security page row navigates to
  // chrome://settings/security
  test('onSecurityPageClick', function() {
    page.$.securityLinkRow.click();
    flush();
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });

  test('privacySandboxRestricted', function() {
    assertFalse(isChildVisible(page, '#privacySandboxLinkRow'));
  });
});



suite(`PrivacySandbox`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('privacySandboxRestricted', function() {
    assertTrue(isChildVisible(page, '#privacySandboxLinkRow'));
  });

  test('privacySandboxRowLabel', function() {
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow');
    assertTrue(!!privacySandboxLinkRow);
    assertEquals(
        loadTimeData.getString('adPrivacyLinkRowLabel'),
        privacySandboxLinkRow.label);
  });

  test('privacySandboxNotExternalLink', function() {
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow');
    assertTrue(!!privacySandboxLinkRow);
    assertFalse(privacySandboxLinkRow.external);
  });

  test('clickPrivacySandboxRow', async function() {
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<HTMLElement>('#privacySandboxLinkRow');
    assertTrue(!!privacySandboxLinkRow);
    privacySandboxLinkRow.click();
    // Ensure UMA is logged.
    assertEquals(
        'Settings.PrivacySandbox.OpenedFromSettingsParent',
        await metricsBrowserProxy.whenCalled('recordAction'));

    // Ensure the correct route has been navigated to when enabling
    // kPrivacySandboxSettings4.
    await flushTasks();
    assertEquals(
        routes.PRIVACY_SANDBOX, Router.getInstance().getCurrentRoute());
  });
});

suite(`CookiesSubpage`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('clickCookiesRow', async function() {
    const thirdPartyCookiesLinkRow =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#thirdPartyCookiesLinkRow');
    assertTrue(!!thirdPartyCookiesLinkRow);
    thirdPartyCookiesLinkRow.click();
    // Check that the correct page was navigated to.
    await flushTasks();
    assertEquals(routes.COOKIES, Router.getInstance().getCurrentRoute());
  });
});

suite('CookiesSubpageRedesignDisabled', function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);

    return flushTasks();
  }

  test(
      'cookiesLinkRowSublabel', async function() {
        loadTimeData.overrideValues({
          is3pcdCookieSettingsRedesignEnabled: false,
        });
        resetRouterForTesting();

        await createPage();

        page.set(
            'prefs.profile.cookie_controls_mode.value', CookieControlsMode.OFF);
        const thirdPartyCookiesLinkRow =
            page.shadowRoot!.querySelector<CrLinkRowElement>(
                '#thirdPartyCookiesLinkRow');
        assertTrue(!!thirdPartyCookiesLinkRow);
        assertEquals(
            page.i18n('thirdPartyCookiesLinkRowSublabelEnabled'),
            thirdPartyCookiesLinkRow.subLabel);

        page.set(
            'prefs.profile.cookie_controls_mode.value',
            CookieControlsMode.INCOGNITO_ONLY);
        assertEquals(
            page.i18n('thirdPartyCookiesLinkRowSublabelEnabled'),
            thirdPartyCookiesLinkRow.subLabel,
        );

        page.set(
            'prefs.profile.cookie_controls_mode.value',
            CookieControlsMode.BLOCK_THIRD_PARTY);
        assertEquals(
            page.i18n('thirdPartyCookiesLinkRowSublabelDisabled'),
            thirdPartyCookiesLinkRow.subLabel);
      });
});

suite(`PrivacySandbox4EnabledButRestricted`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    // Note that the browsertest setup ensures these values are set correctly at
    // startup, such that routes are created (or not). They are included here to
    // make clear the intent of the test.
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
      isPrivacySandboxRestrictedNoticeEnabled: false,
    });
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('noPrivacySandboxRowShown', function() {
    assertFalse(isChildVisible(page, '#privacySandboxLinkRow'));
  });

  test('noRouteForAdPrivacyPaths', function() {
    const adPrivacyPaths = [
      routes.PRIVACY_SANDBOX,
      routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
      routes.PRIVACY_SANDBOX_TOPICS,
      routes.PRIVACY_SANDBOX_FLEDGE,
    ];
    for (const path of adPrivacyPaths) {
      assertThrows(() => Router.getInstance().navigateTo(path));
    }
  });
});

suite(`PrivacySandbox4EnabledButRestrictedWithNotice`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    // Note that the browsertest setup ensures these values are set correctly at
    // startup, such that routes are created (or not). They are included here to
    // make clear the intent of the test.
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
      isPrivacySandboxRestrictedNoticeEnabled: true,
    });
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('privacySandboxRowShown', function() {
    assertTrue(isChildVisible(page, '#privacySandboxLinkRow'));
  });

  test('noRouteForDisabledAdPrivacyPaths', function() {
    const removedAdPrivacyPaths = [
      routes.PRIVACY_SANDBOX_TOPICS,
      routes.PRIVACY_SANDBOX_FLEDGE,
    ];
    const presentAdPrivacyPaths = [
      routes.PRIVACY_SANDBOX,
      routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
    ];
    for (const path of removedAdPrivacyPaths) {
      assertThrows(() => Router.getInstance().navigateTo(path));
    }
    for (const path of presentAdPrivacyPaths) {
      Router.getInstance().navigateTo(path);
      assertEquals(path, Router.getInstance().getCurrentRoute());
    }
  });

  test('privacySandboxRowSublabel', function() {
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow');
    assertTrue(!!privacySandboxLinkRow);
    // Ensure that a measurement-specific message is shown in this
    // configuration. The default is tested in the regular
    // PrivacySandbox4Enabled suite.
    assertEquals(
        loadTimeData.getString('adPrivacyRestrictedLinkRowSubLabel'),
        privacySandboxLinkRow.subLabel);
  });
});

suite('PrivacyGuideRow', function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    loadTimeData.overrideValues({showPrivacyGuide: true});
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('rowNotShown', async function() {
    loadTimeData.overrideValues({showPrivacyGuide: false});
    resetRouterForTesting();

    page.remove();
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);

    await flushTasks();
    assertFalse(
        loadTimeData.getBoolean('showPrivacyGuide'),
        'showPrivacyGuide was not overwritten');
    assertFalse(
        isChildVisible(page, '#privacyGuideLinkRow'),
        'privacyGuideLinkRow is visible');
  });

  test('privacyGuideRowVisibleSupervisedAccount', function() {
    assertTrue(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user signs in to a supervised user account. This hides the privacy
    // guide entry point.
    const syncStatus: SyncStatus = {
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user is no longer signed in to a supervised user account. This
    // doesn't show the entry point.
    syncStatus.supervisedUser = false;
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));
  });

  test('privacyGuideRowVisibleManaged', function() {
    assertTrue(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user becomes managed. This hides the privacy guide entry point.
    webUIListenerCallback('is-managed-changed', true);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user is no longer managed. This doesn't show the entry point.
    webUIListenerCallback('is-managed-changed', false);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));
  });

  test('privacyGuideRowClick', async function() {
    const privacyGuideLinkRow =
        page.shadowRoot!.querySelector<HTMLElement>('#privacyGuideLinkRow');
    assertTrue(!!privacyGuideLinkRow);
    privacyGuideLinkRow.click();

    const result = await metricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY, result);

    // Ensure the correct route has been navigated to.
    assertEquals(routes.PRIVACY_GUIDE, Router.getInstance().getCurrentRoute());

    // Ensure the privacy guide dialog is shown.
    assertTrue(
        !!page.shadowRoot!.querySelector<HTMLElement>('#privacyGuideDialog'));
  });
});

suite('HappinessTrackingSurveys', function() {
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsPrivacyPageElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
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
        page.shadowRoot!.querySelector<HTMLElement>(
            '#thirdPartyCookiesLinkRow');
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

// TODO(crbug.com/397187800): Remove once kDbdRevampDesktop is launched.
suite('DeleteBrowsingDataRevampDisabled', () => {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    loadTimeData.overrideValues({
      enableDeleteBrowsingDataRevamp: false,
    });
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('showClearBrowsingDataDialog', function() {
    assertFalse(!!page.shadowRoot!.querySelector(
        'settings-clear-browsing-data-dialog'));
    page.$.clearBrowsingData.click();
    flush();

    const dialog =
        page.shadowRoot!.querySelector('settings-clear-browsing-data-dialog');
    assertTrue(!!dialog);
  });
});
