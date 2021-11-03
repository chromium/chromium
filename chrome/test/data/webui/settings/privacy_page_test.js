// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CategorySettingExceptionsElement, ClearBrowsingDataBrowserProxyImpl, ContentSettingsTypes, CookieControlsMode, CookiePrimarySetting, SafeBrowsingSetting, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {HatsBrowserProxyImpl, MetricsBrowserProxyImpl, PrivacyElementInteractions, PrivacyPageBrowserProxyImpl, Route, Router, routes, SecureDnsMode, SettingsPrivacyPageElement, TrustSafetyInteraction} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

/** @type {!Array<!Route>} */
const redesignedPages = [
  routes.SITE_SETTINGS_ADS,
  routes.SITE_SETTINGS_AR,
  routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
  routes.SITE_SETTINGS_BACKGROUND_SYNC,
  routes.SITE_SETTINGS_CAMERA,
  routes.SITE_SETTINGS_CLIPBOARD,
  routes.SITE_SETTINGS_FONT_ACCESS,
  routes.SITE_SETTINGS_FILE_HANDLING,
  routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
  routes.SITE_SETTINGS_HANDLERS,
  routes.SITE_SETTINGS_HID_DEVICES,
  routes.SITE_SETTINGS_IDLE_DETECTION,
  routes.SITE_SETTINGS_IMAGES,
  routes.SITE_SETTINGS_JAVASCRIPT,
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
  // routes.SITE_SETTINGS_WINDOW_PLACEMENT,

  // Doesn't contain toggle or radio buttons
  // routes.SITE_SETTINGS_INSECURE_CONTENT,
  // routes.SITE_SETTINGS_ZOOM_LEVELS,
];

suite('PrivacyPage', function() {
  /** @type {!SettingsPrivacyPageElement} */
  let page;

  let testClearBrowsingDataBrowserProxy;

  /** @type {!TestSiteSettingsPrefsBrowserProxy}*/
  let siteSettingsBrowserProxy;

  /** @type {!TestMetricsBrowserProxy} */
  let metricsBrowserProxy;

  /** @type {!Array<string>} */
  const testLabels = ['test label 1', 'test label 2'];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacyReviewEnabled: false,
    });
  });

  setup(function() {
    testClearBrowsingDataBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(
        testClearBrowsingDataBrowserProxy);
    const testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyPageElement} */
        (document.createElement('settings-privacy-page'));
    page.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
      },
      safebrowsing: {
        enabled: {value: true},
        scout_reporting_enabled: {value: true},
        enhanced: {value: false}
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
      privacy_sandbox: {
        apis_enabled: {value: true},
      },
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('showClearBrowsingDataDialog', function() {
    assertFalse(
        !!page.shadowRoot.querySelector('settings-clear-browsing-data-dialog'));
    page.shadowRoot.querySelector('#clearBrowsingData').click();
    flush();

    const dialog =
        page.shadowRoot.querySelector('settings-clear-browsing-data-dialog');
    assertTrue(!!dialog);
  });

  test('CookiesLinkRowSublabel', async function() {
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    assertEquals(
        page.shadowRoot.querySelector('#cookiesLinkRow').subLabel,
        testLabels[0]);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(
        page.shadowRoot.querySelector('#cookiesLinkRow').subLabel,
        testLabels[1]);
  });

  test('privacyReviewRowNotVisible', function() {
    assertFalse(isChildVisible(page, '#privacyReviewLinkRow'));
  });

  test('ContentSettingsVisibility', async function() {
    // Ensure pages are visited so that HTML components are stamped.
    redesignedPages.forEach(route => Router.getInstance().navigateTo(route));
    await flushTasks();

    // All redesigned pages, except notifications, protocol handlers, pdf
    // documents and protected content (except chromeos and win), will use a
    // settings-category-default-radio-group.
    // <if expr="chromeos or is_win">
    assertEquals(
        page.root.querySelectorAll('settings-category-default-radio-group')
            .length,
        redesignedPages.length - 3);
    // </if>
    // <if expr="not chromeos and not is_win">
    assertEquals(
        page.root.querySelectorAll('settings-category-default-radio-group')
            .length,
        redesignedPages.length - 4);
    // </if>
  });

  test('NotificationPage', async function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_NOTIFICATIONS);
    await flushTasks();

    assertTrue(isChildVisible(page, '#notificationRadioGroup'));
    const categorySettingExceptions =
        /** @type {!CategorySettingExceptionsElement} */
        (page.shadowRoot.querySelector('category-setting-exceptions'));
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.NOTIFICATIONS, categorySettingExceptions.category);
    assertFalse(isChildVisible(page, 'category-default-setting'));
  });

  test('privacySandboxRowSublabel', async function() {
    page.set('prefs.privacy_sandbox.apis_enabled.value', true);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsEnabled'),
        page.shadowRoot.querySelector('#privacySandboxLinkRow').subLabel);

    page.set('prefs.privacy_sandbox.apis_enabled.value', false);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsDisabled'),
        page.shadowRoot.querySelector('#privacySandboxLinkRow').subLabel);
  });

  test('clickPrivacySandboxRow', async function() {
    page.shadowRoot.querySelector('#privacySandboxLinkRow').click();
    // Ensure UMA is logged.
    assertEquals(
        'Settings.PrivacySandbox.OpenedFromSettingsParent',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });
});

suite('PrivacyReviewEnabled', function() {
  /** @type {!SettingsPrivacyPageElement} */
  let page;

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyPageElement} */
        (document.createElement('settings-privacy-page'));
    page.prefs = {
      // Need privacy_sandbox pref for the page's setup.
      privacy_sandbox: {
        apis_enabled: {value: true},
      },
      privacy_review: {
        show_welcome_card:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
      },
      generated: {
        cookie_primary_setting: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookiePrimarySetting.BLOCK_THIRD_PARTY,
        },
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  test('privacyReviewRowVisible', function() {
    assertTrue(isChildVisible(page, '#privacyReviewLinkRow'));
  });

  test('privacyReviewRowClick', function() {
    page.shadowRoot.querySelector('#privacyReviewLinkRow').click();
    // Ensure the correct Settings page is shown.
    assertEquals(routes.PRIVACY_REVIEW, Router.getInstance().getCurrentRoute());
  });
});

suite('PrivacyPageSound', function() {
  /** @type {!TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {!SettingsPrivacyPageElement} */
  let page;

  function getToggleElement() {
    return page.shadowRoot.querySelector('settings-animated-pages')
        .queryEffectiveChildren('settings-subpage')
        .queryEffectiveChildren('#block-autoplay-setting');
  }

  setup(() => {
    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});

    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SOUND);
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyPageElement} */
        (document.createElement('settings-privacy-page'));
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
    page = /** @type {!SettingsPrivacyPageElement} */
        (document.createElement('settings-privacy-page'));
    document.body.appendChild(page);

    return flushTasks().then(() => {
      assertFalse(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
      assertTrue(getToggleElement().hidden);
    });
  });

  test('Click', () => {
    assertTrue(getToggleElement().hasAttribute('disabled'));
    assertFalse(getToggleElement().hasAttribute('checked'));

    webUIListenerCallback(
        'onBlockAutoplayStatusChanged', {pref: {value: true}, enabled: true});

    return flushTasks().then(() => {
      // Check that we are on and enabled.
      assertFalse(getToggleElement().hasAttribute('disabled'));
      assertTrue(getToggleElement().hasAttribute('checked'));

      // Click on the toggle and wait for the proxy to be called.
      getToggleElement().click();
      return testBrowserProxy.whenCalled('setBlockAutoplayEnabled')
          .then((enabled) => {
            assertFalse(enabled);
          });
    });
  });
});

suite('HappinessTrackingSurveys', function() {
  /** @type {!TestHatsBrowserProxy} */
  let testHatsBrowserProxy;

  /** @type {!SettingsPrivacyPageElement} */
  let page;

  setup(function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyPageElement} */
        (document.createElement('settings-privacy-page'));
    // Initialize the privacy page pref. Security page manually expands
    // the initially selected safe browsing option so the pref object
    // needs to be defined.
    page.prefs = {
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
        cookie_session_only: {value: false},
        cookie_primary_setting:
            {type: chrome.settingsPrivate.PrefType.NUMBER, value: 0},
        password_manager_leak_detection: {value: false},
      },
      profile: {password_manager_leak_detection: {value: false}},
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('ClearBrowsingDataTrigger', async function() {
    page.shadowRoot.querySelector('#clearBrowsingData').click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('CookiesTrigger', async function() {
    page.shadowRoot.querySelector('#cookiesLinkRow').click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('SecurityTrigger', async function() {
    page.shadowRoot.querySelector('#securityLinkRow').click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('PermissionsTrigger', async function() {
    page.shadowRoot.querySelector('#permissionsLinkRow').click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });
});
