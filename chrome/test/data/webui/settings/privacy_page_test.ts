// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ClearBrowsingDataBrowserProxyImpl, ContentSettingsTypes, CookieControlsMode, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, HatsBrowserProxyImpl, MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyPageBrowserProxyImpl, Route, Router, routes, SettingsPrefsElement, SettingsPrivacyPageElement, StatusAction, SyncStatus, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

const redesignedPages: Route[] = [
  routes.SITE_SETTINGS_ADS,
  routes.SITE_SETTINGS_AR,
  routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
  routes.SITE_SETTINGS_BACKGROUND_SYNC,
  routes.SITE_SETTINGS_CAMERA,
  routes.SITE_SETTINGS_CLIPBOARD,
  routes.SITE_SETTINGS_FEDERATED_IDENTITY_API,
  routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
  routes.SITE_SETTINGS_HANDLERS,
  routes.SITE_SETTINGS_HID_DEVICES,
  routes.SITE_SETTINGS_IDLE_DETECTION,
  routes.SITE_SETTINGS_IMAGES,
  routes.SITE_SETTINGS_JAVASCRIPT,
  routes.SITE_SETTINGS_LOCAL_FONTS,
  routes.SITE_SETTINGS_LOCATION,
  routes.SITE_SETTINGS_MICROPHONE,
  routes.SITE_SETTINGS_MIDI_DEVICES,
  routes.SITE_SETTINGS_NOTIFICATIONS,
  routes.SITE_SETTINGS_PAYMENT_HANDLER,
  routes.SITE_SETTINGS_PDF_DOCUMENTS,
  routes.SITE_SETTINGS_POPUPS,
  routes.SITE_SETTINGS_PROTECTED_CONTENT,
  routes.SITE_SETTINGS_SENSORS,
  routes.SITE_SETTINGS_SERIAL_PORTS,
  routes.SITE_SETTINGS_SOUND,
  routes.SITE_SETTINGS_USB_DEVICES,
  routes.SITE_SETTINGS_VR,

  // TODO(crbug.com/1128902) After restructure add coverage for elements on
  // routes which depend on flags being enabled.
  // routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
  // routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
  // routes.SITE_SETTINGS_WINDOW_MANAGEMENT,

  // Doesn't contain toggle or radio buttons
  // routes.SITE_SETTINGS_INSECURE_CONTENT,
  // routes.SITE_SETTINGS_ZOOM_LEVELS,
];

suite('PrivacyPage', function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testClearBrowsingDataBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testClearBrowsingDataBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(
        testClearBrowsingDataBrowserProxy);
    const testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
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

  // TODO(crbug.com/1378703): Remove the test once PrivacySandboxSettings4
  // has been rolled out.
  test('cookiesLinkRowLabel', function() {
    assertTrue(Boolean(page.shadowRoot!.querySelector<HTMLElement>(
        '#thirdPartyCookiesLinkRow')));
    assertFalse(Boolean(
        page.shadowRoot!.querySelector<HTMLElement>('#cookiesLinkRow')));
    assertEquals(
        page.i18n('thirdPartyCookiesLinkRowLabel'),
        page.shadowRoot!
            .querySelector<CrLinkRowElement>(
                '#thirdPartyCookiesLinkRow')!.label);
  });

  test('cookiesLinkRowSublabel', async function() {
    page.set(
        'prefs.profile.cookie_controls_mode.value', CookieControlsMode.OFF);
    assertEquals(
        page.i18n('thirdPartyCookiesLinkRowSublabelEnabled'),
        page.shadowRoot!
            .querySelector<CrLinkRowElement>(
                '#thirdPartyCookiesLinkRow')!.subLabel);

    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.INCOGNITO_ONLY);
    assertEquals(
        page.i18n('thirdPartyCookiesLinkRowSublabelDisabledIncognito'),
        page.shadowRoot!
            .querySelector<CrLinkRowElement>(
                '#thirdPartyCookiesLinkRow')!.subLabel);

    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.BLOCK_THIRD_PARTY);
    assertEquals(
        page.i18n('thirdPartyCookiesLinkRowSublabelDisabled'),
        page.shadowRoot!
            .querySelector<CrLinkRowElement>(
                '#thirdPartyCookiesLinkRow')!.subLabel);
  });

  test('ContentSettingsVisibility', async function() {
    // Ensure pages are visited so that HTML components are stamped.
    redesignedPages.forEach(route => Router.getInstance().navigateTo(route));
    await flushTasks();

    // All redesigned pages, except notifications, protocol handlers, pdf
    // documents and protected content (except chromeos and win), will use a
    // settings-category-default-radio-group.
    // <if expr="is_chromeos or is_win">
    assertEquals(
        page.shadowRoot!
            .querySelectorAll('settings-category-default-radio-group')
            .length,
        redesignedPages.length - 3);
    // </if>
    // <if expr="not is_chromeos and not is_win">
    assertEquals(
        page.shadowRoot!
            .querySelectorAll('settings-category-default-radio-group')
            .length,
        redesignedPages.length - 4);
    // </if>
  });

  test('NotificationPage', async function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_NOTIFICATIONS);
    await flushTasks();

    assertTrue(isChildVisible(page, '#notificationRadioGroup'));
    const categorySettingExceptions =
        page.shadowRoot!.querySelector('category-setting-exceptions')!;
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.NOTIFICATIONS, categorySettingExceptions.category);
    assertFalse(isChildVisible(page, 'category-default-setting'));
  });

  test('privacySandboxRestricted', function() {
    assertFalse(isChildVisible(page, '#privacySandboxLinkRow'));
  });
});

// TODO(crbug.com/1378703): Remove once PrivacySandboxSettings4 has been rolled
// out.
suite(`PrivacySandbox4Disabled`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  const testLabels: string[] = ['test label 1', 'test label 2'];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
      isPrivacySandboxSettings4: false,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]!);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('cookiesLinkRowLabel', function() {
    assertTrue(Boolean(
        page.shadowRoot!.querySelector<HTMLElement>('#cookiesLinkRow')));
    assertFalse(Boolean(page.shadowRoot!.querySelector<HTMLElement>(
        '#thirdPartyCookiesLinkRow')));
    assertEquals(
        page.i18n('cookiePageTitle'),
        page.shadowRoot!.querySelector<CrLinkRowElement>(
                            '#cookiesLinkRow')!.label);
  });

  test('cookiesLinkRowSublabel', async function() {
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    assertEquals(
        testLabels[0],
        page.shadowRoot!.querySelector<CrLinkRowElement>(
                            '#cookiesLinkRow')!.subLabel);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(
        testLabels[1],
        page.shadowRoot!.querySelector<CrLinkRowElement>(
                            '#cookiesLinkRow')!.subLabel);
  });

  test('privacySandboxRestricted', function() {
    assertTrue(isChildVisible(page, '#privacySandboxLinkRow'));
  });

  test('privacySandboxRowLabel', function() {
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow')!;
    assertEquals(
        loadTimeData.getString('privacySandboxTitle'),
        privacySandboxLinkRow.label);
  });

  test('privacySandboxRowSublabel', async function() {
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', true);
    assertTrue(isChildVisible(page, '#privacySandboxLinkRow'));
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow')!;
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsEnabled'),
        privacySandboxLinkRow.subLabel);

    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', false);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsDisabled'),
        privacySandboxLinkRow.subLabel);
  });

  test('privacySandboxExternalLink', function() {
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow');
    assertTrue(!!privacySandboxLinkRow);
    assertTrue(privacySandboxLinkRow.external);
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
  });

  test('cookiesSubpageAttributes', async function() {
    // The subpage is only in the DOM if the corresponding route is open.
    page.shadowRoot!.querySelector<CrLinkRowElement>(
                        '#cookiesLinkRow')!.click();
    await flushTasks();

    const cookiesSubpage =
        page.shadowRoot!.querySelector<PolymerElement>('#cookies');
    assertTrue(!!cookiesSubpage);
    assertEquals(
        page.i18n('cookiePageTitle'),
        cookiesSubpage.getAttribute('page-title'));
    const associatedControl = cookiesSubpage.get('associatedControl');
    assertTrue(!!associatedControl);
    assertEquals('cookiesLinkRow', associatedControl.id);
  });
});

suite(`PrivacySandbox4Enabled`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
            '#privacySandboxLinkRow')!;
    assertEquals(
        loadTimeData.getString('adPrivacyLinkRowLabel'),
        privacySandboxLinkRow.label);
  });

  test('privacySandboxRowSublabel', async function() {
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', true);
    assertTrue(isChildVisible(page, '#privacySandboxLinkRow'));
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow')!;
    await flushTasks();
    assertEquals(
        loadTimeData.getString('adPrivacyLinkRowSubLabel'),
        privacySandboxLinkRow.subLabel);

    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', false);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('adPrivacyLinkRowSubLabel'),
        privacySandboxLinkRow.subLabel);
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

  test('cookiesSubpageAttributes', async function() {
    // The subpage is only in the DOM if the corresponding route is open.
    page.shadowRoot!
        .querySelector<CrLinkRowElement>('#thirdPartyCookiesLinkRow')!.click();
    await flushTasks();

    const cookiesSubpage =
        page.shadowRoot!.querySelector<PolymerElement>('#cookies');
    assertTrue(!!cookiesSubpage);
    assertEquals(
        page.i18n('thirdPartyCookiesPageTitle'),
        cookiesSubpage.getAttribute('page-title'));
    const associatedControl = cookiesSubpage.get('associatedControl');
    assertTrue(!!associatedControl);
    assertEquals('thirdPartyCookiesLinkRow', associatedControl.id);
  });
});

suite('PrivacyGuideRowTests', function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    loadTimeData.overrideValues({showPrivacyGuide: true});
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('rowNotShown', async function() {
    loadTimeData.overrideValues({showPrivacyGuide: false});

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

  test('privacyGuideRowVisibleChildAccount', function() {
    assertTrue(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user signs in to a child user account. This hides the privacy guide
    // entry point.
    const syncStatus:
        SyncStatus = {childUser: true, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user is no longer signed in to a child user account. This doesn't
    // show the entry point.
    syncStatus.childUser = false;
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
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#privacyGuideLinkRow')!.click();

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

suite('PrivacyPageSound', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let page: SettingsPrivacyPageElement;

  function getToggleElement() {
    return page.shadowRoot!.querySelector<HTMLElement>(
        '#block-autoplay-setting')!;
  }

  setup(() => {
    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});

    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SOUND);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(() => {
    page.remove();
  });

  test('UpdateStatus', () => {
    assertTrue(getToggleElement().hasAttribute('disabled'));
    assertFalse(getToggleElement().hasAttribute('checked'));

    webUIListenerCallback(
        'onBlockAutoplayStatusChanged', {pref: {value: true}, enabled: true});

    return flushTasks().then(() => {
      // Check that we are on and enabled.
      assertFalse(getToggleElement().hasAttribute('disabled'));
      assertTrue(getToggleElement().hasAttribute('checked'));

      // Toggle the pref off.
      webUIListenerCallback(
          'onBlockAutoplayStatusChanged',
          {pref: {value: false}, enabled: true});

      return flushTasks().then(() => {
        // Check that we are off and enabled.
        assertFalse(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        // Disable the autoplay status toggle.
        webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: false}, enabled: false});

        return flushTasks().then(() => {
          // Check that we are off and disabled.
          assertTrue(getToggleElement().hasAttribute('disabled'));
          assertFalse(getToggleElement().hasAttribute('checked'));
        });
      });
    });
  });

  test('Hidden', () => {
    assertTrue(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
    assertFalse(getToggleElement().hidden);

    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});

    page.remove();
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);

    return flushTasks().then(() => {
      assertFalse(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
      assertTrue(getToggleElement().hidden);
    });
  });

  test('Click', async () => {
    assertTrue(getToggleElement().hasAttribute('disabled'));
    assertFalse(getToggleElement().hasAttribute('checked'));

    webUIListenerCallback(
        'onBlockAutoplayStatusChanged', {pref: {value: true}, enabled: true});

    await flushTasks();
    // Check that we are on and enabled.
    assertFalse(getToggleElement().hasAttribute('disabled'));
    assertTrue(getToggleElement().hasAttribute('checked'));

    // Click on the toggle and wait for the proxy to be called.
    getToggleElement().click();
    const enabled =
        await testBrowserProxy.whenCalled('setBlockAutoplayEnabled');
    assertFalse(enabled);
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
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#thirdPartyCookiesLinkRow')!.click();
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

  test('PermissionsTrigger', async function() {
    page.$.permissionsLinkRow.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });
});

suite('NotificationPermissionReview', function() {
  let page: SettingsPrivacyPageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  const oneElementMockData = [{
    origin: 'www.example.com',
    notificationInfoString: 'About 4 notifications a day',
  }];

  setup(function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_NOTIFICATIONS);
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(function() {
    page.remove();
  });

  function createPage() {
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);
    return flushTasks();
  }

  test('InvisibleWhenFeatureDisabled', async function() {
    loadTimeData.overrideValues({
      safetyCheckNotificationPermissionsEnabled: false,
    });
    siteSettingsBrowserProxy.setNotificationPermissionReview([]);
    await createPage();

    assertFalse(isChildVisible(page, 'review-notification-permissions'));
  });

  test('InvisibleWhenFeatureDisabledWithItemsToReview', async function() {
    loadTimeData.overrideValues({
      safetyCheckNotificationPermissionsEnabled: false,
    });
    siteSettingsBrowserProxy.setNotificationPermissionReview(
        oneElementMockData);
    await createPage();

    assertFalse(isChildVisible(page, 'review-notification-permissions'));
  });

  test('VisibilityWithChangingPermissionList', async function() {
    loadTimeData.overrideValues({
      safetyCheckNotificationPermissionsEnabled: true,
    });

    // The element is not visible when there is nothing to review.
    siteSettingsBrowserProxy.setNotificationPermissionReview([]);
    await createPage();
    assertFalse(isChildVisible(page, 'review-notification-permissions'));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed',
        oneElementMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, 'review-notification-permissions'));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', []);
    await flushTasks();
    assertTrue(isChildVisible(page, 'review-notification-permissions'));
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed',
        oneElementMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, 'review-notification-permissions'));
  });
});
