// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ClearBrowsingDataBrowserProxyImpl, CookieControlsMode, SafeBrowsingSetting, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {HatsBrowserProxyImpl, MetricsBrowserProxyImpl, PrivacyElementInteractions, PrivacyPageBrowserProxyImpl, Route, Router, routes, SecureDnsMode, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {flushTasks} from '../test_util.m.js';

import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.m.js';

// clang-format on

/** @type {!Array<!Route>} */
const redesignedPages = [
  routes.SITE_SETTINGS_ADS,
  routes.SITE_SETTINGS_AR,
  routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
  routes.SITE_SETTINGS_BACKGROUND_SYNC,
  routes.SITE_SETTINGS_CAMERA,
  routes.SITE_SETTINGS_CLIPBOARD,
  routes.SITE_SETTINGS_FLASH,
  routes.SITE_SETTINGS_IMAGES,
  routes.SITE_SETTINGS_JAVASCRIPT,
  routes.SITE_SETTINGS_LOCATION,
  routes.SITE_SETTINGS_MICROPHONE,
  routes.SITE_SETTINGS_MIDI_DEVICES,
  routes.SITE_SETTINGS_POPUPS,
  routes.SITE_SETTINGS_SENSORS,
  routes.SITE_SETTINGS_SERIAL_PORTS,
  routes.SITE_SETTINGS_SOUND,
  routes.SITE_SETTINGS_UNSANDBOXED_PLUGINS,
  routes.SITE_SETTINGS_USB_DEVICES,
  routes.SITE_SETTINGS_VR,

  // TODO(crbug.com/1128902) After restructure add coverage for elements on
  // routes which depend on flags being enabled.
  // routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
  // routes.SITE_SETTINGS_PAYMENT_HANDLER,

  // Doesn't contain toggle or radio buttons
  // routes.SITE_SETTINGS_ZOOM_LEVELS,
];

/** @type {!Array<!Route>} */
const notRedesignedPages = [
  routes.SITE_SETTINGS_NOTIFICATIONS,
  routes.SITE_SETTINGS_HID_DEVICES,

  // Content settings that depend on flags being enabled.
  // routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
  // routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
  // routes.SITE_SETTINGS_WINDOW_PLACEMENT,
  // routes.SITE_SETTINGS_FONT_ACCESS,
];

suite('PrivacyPage', function() {
  /** @type {!SettingsPrivacyPageElement} */
  let page;

  /** @type {!TestClearBrowsingDataBrowserProxy} */
  let testClearBrowsingDataBrowserProxy;

  /** @type {!TestSiteSettingsPrefsBrowserProxy}*/
  let siteSettingsBrowserProxy;

  /** @type {!Array<string>} */
  const testLabels = ['test label 1', 'test label 2'];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: false,
    });
  });

  setup(async function() {
    testClearBrowsingDataBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.instance_ =
        testClearBrowsingDataBrowserProxy;
    const testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    const testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = testSyncBrowserProxy;
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]);

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
    };
    document.body.appendChild(page);
    return testSyncBrowserProxy.whenCalled('getSyncStatus');
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('showClearBrowsingDataDialog', function() {
    assertFalse(!!page.$$('settings-clear-browsing-data-dialog'));
    page.$$('#clearBrowsingData').click();
    flush();

    const dialog = page.$$('settings-clear-browsing-data-dialog');
    assertTrue(!!dialog);
  });

  test('CookiesLinkRowSublabel', async function() {
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    assertEquals(page.$$('#cookiesLinkRow').subLabel, testLabels[0]);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(page.$$('#cookiesLinkRow').subLabel, testLabels[1]);
  });

  test('ContentSettingsRedesignVisibility', async function() {
    // Ensure pages are visited so that HTML components are stamped.
    redesignedPages.forEach(route => Router.getInstance().navigateTo(route));
    notRedesignedPages.forEach(route => Router.getInstance().navigateTo(route));

    assertFalse(loadTimeData.getBoolean('enableContentSettingsRedesign'));
    assertEquals(
        page.root.querySelectorAll('category-default-setting').length,
        redesignedPages.length + notRedesignedPages.length);
    assertEquals(
        page.root.querySelectorAll('settings-category-default-radio-group')
            .length,
        0);
  });
});


suite('ContentSettingsRedesign', function() {
  /** @type {!SettingsPrivacyPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: true,
    });
  });

  setup(async function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyPageElement} */
        (document.createElement('settings-privacy-page'));
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('ContentSettingsRedesignVisibility', async function() {
    // Ensure pages are visited so that HTML components are stamped.
    redesignedPages.forEach(route => Router.getInstance().navigateTo(route));
    notRedesignedPages.forEach(route => Router.getInstance().navigateTo(route));

    assertTrue(loadTimeData.getBoolean('enableContentSettingsRedesign'));
    assertEquals(
        page.root.querySelectorAll('category-default-setting').length,
        notRedesignedPages.length);
    assertEquals(
        page.root.querySelectorAll('settings-category-default-radio-group')
            .length,
        redesignedPages.length);
  });
});

suite('PrivacyPageSound', function() {
  /** @type {!TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {!SettingsPrivacyPageElement} */
  let page;

  function flushAsync() {
    flush();
    return new Promise(resolve => {
      page.async(resolve);
    });
  }

  function getToggleElement() {
    return page.$$('settings-animated-pages')
        .queryEffectiveChildren('settings-subpage')
        .queryEffectiveChildren('#block-autoplay-setting');
  }

  setup(() => {
    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});

    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SOUND);
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyPageElement} */
        (document.createElement('settings-privacy-page'));
    document.body.appendChild(page);
    return flushAsync();
  });

  teardown(() => {
    page.remove();
  });

  test('UpdateStatus', () => {
    assertTrue(getToggleElement().hasAttribute('disabled'));
    assertFalse(getToggleElement().hasAttribute('checked'));

    webUIListenerCallback(
        'onBlockAutoplayStatusChanged', {pref: {value: true}, enabled: true});

    return flushAsync().then(() => {
      // Check that we are on and enabled.
      assertFalse(getToggleElement().hasAttribute('disabled'));
      assertTrue(getToggleElement().hasAttribute('checked'));

      // Toggle the pref off.
      webUIListenerCallback(
          'onBlockAutoplayStatusChanged',
          {pref: {value: false}, enabled: true});

      return flushAsync().then(() => {
        // Check that we are off and enabled.
        assertFalse(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        // Disable the autoplay status toggle.
        webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: false}, enabled: false});

        return flushAsync().then(() => {
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

    return flushAsync().then(() => {
      assertFalse(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
      assertTrue(getToggleElement().hidden);
    });
  });

  test('Click', () => {
    assertTrue(getToggleElement().hasAttribute('disabled'));
    assertFalse(getToggleElement().hasAttribute('checked'));

    webUIListenerCallback(
        'onBlockAutoplayStatusChanged', {pref: {value: true}, enabled: true});

    return flushAsync().then(() => {
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
    HatsBrowserProxyImpl.instance_ = testHatsBrowserProxy;
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
  });

  test('ClearBrowsingDataTrigger', function() {
    page.$$('#clearBrowsingData').click();
    return testHatsBrowserProxy.whenCalled('tryShowSurvey');
  });

  test('CookiesTrigger', function() {
    page.$$('#cookiesLinkRow').click();
    return testHatsBrowserProxy.whenCalled('tryShowSurvey');
  });

  test('SecurityTrigger', function() {
    page.$$('#securityLinkRow').click();
    return testHatsBrowserProxy.whenCalled('tryShowSurvey');
  });

  test('PermissionsTrigger', function() {
    page.$$('#permissionsLinkRow').click();
    return testHatsBrowserProxy.whenCalled('tryShowSurvey');
  });
});
