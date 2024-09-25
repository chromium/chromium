// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SiteDetailsElement, WebsiteUsageBrowserProxy} from 'chrome://settings/lazy_load.js';
import {ChooserType, ContentSetting, ContentSettingsTypes, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl, WebsiteUsageBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, Router, routes} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createRawChooserException, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

class TestWebsiteUsageBrowserProxy extends TestBrowserProxy implements
    WebsiteUsageBrowserProxy {
  constructor() {
    super(['clearUsage', 'fetchUsageTotal']);
  }

  fetchUsageTotal(host: string) {
    this.methodCalled('fetchUsageTotal', host);
  }

  clearUsage(origin: string) {
    this.methodCalled('clearUsage', origin);
  }
}

/** Suite of tests for site-details. */
suite('SiteDetails', function() {
  /** A site list element created before each test. */
  let testElement: SiteDetailsElement;

  /** An example pref with 1 pref in each category. */
  let prefs: SiteSettingsPref;

  /** The mock site settings prefs proxy object to use during test. */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  /** The mock website usage proxy object to use during test. */
  let websiteUsageProxy: TestWebsiteUsageBrowserProxy;

  // Initialize a site-details before each test.
  setup(function() {
    loadTimeData.overrideValues({
      enableWebPrintingContentSetting: true,
    });
    prefs = createSiteSettingsPrefs(
        [],
        [
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.COOKIES,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.IMAGES,
              [createRawSiteException('https://foo.com:443', {
                source: SiteSettingSource.DEFAULT,
              })]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.JAVASCRIPT,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.JAVASCRIPT_OPTIMIZER,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.SOUND,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.POPUPS,
              [createRawSiteException('https://foo.com:443', {
                setting: ContentSetting.BLOCK,
                source: SiteSettingSource.DEFAULT,
              })]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.GEOLOCATION,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.NOTIFICATIONS,
              [createRawSiteException('https://foo.com:443', {
                setting: ContentSetting.ASK,
                source: SiteSettingSource.POLICY,
              })]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.MIC,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.CAMERA,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.AUTO_PICTURE_IN_PICTURE,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.AUTOMATIC_DOWNLOADS,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.BACKGROUND_SYNC,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.MIDI_DEVICES,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.PROTECTED_CONTENT,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.ADS,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.CLIPBOARD,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.SENSORS,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.PAYMENT_HANDLER,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.SERIAL_PORTS,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.BLUETOOTH_SCANNING,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.FILE_SYSTEM_WRITE,
              [createRawSiteException('https://foo.com:443', {
                setting: ContentSetting.BLOCK,
              })]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.MIXEDSCRIPT,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.FEDERATED_IDENTITY_API,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.HID_DEVICES,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.BLUETOOTH_DEVICES,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.AR,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.VR,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.WEB_PRINTING,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.WINDOW_MANAGEMENT,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.LOCAL_FONTS,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.IDLE_DETECTION,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.AUTOMATIC_FULLSCREEN,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.CAPTURED_SURFACE_CONTROL,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.HAND_TRACKING,
              [createRawSiteException('https://foo.com:443')]),
        ],
        [
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.USB_DEVICES,
              [createRawChooserException(
                  ChooserType.USB_DEVICES,
                  [createRawSiteException('https://foo.com:443')])]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.BLUETOOTH_DEVICES,
              [createRawChooserException(
                  ChooserType.BLUETOOTH_DEVICES,
                  [createRawSiteException('https://foo.com:443')])]),
        ]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    websiteUsageProxy = new TestWebsiteUsageBrowserProxy();
    WebsiteUsageBrowserProxyImpl.setInstance(websiteUsageProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createSiteDetails(origin: string) {
    const siteDetailsElement = document.createElement('site-details');
    document.body.appendChild(siteDetailsElement);
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
    return siteDetailsElement;
  }

  test('usage heading shows properly', async function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    await websiteUsageProxy.whenCalled('fetchUsageTotal');

    // When there's no usage, there should be a string that says so.
    assertEquals(
        '',
        testElement.shadowRoot!.querySelector(
                                   '#storedData')!.textContent!.trim());
    assertFalse(testElement.$.noStorage.hidden);
    assertTrue(testElement.$.storage.hidden);
    assertTrue(testElement.$.usage.textContent!.includes('No usage data'));

    // If there is, check the correct amount of usage is specified.
    const usage = '1 KB';
    webUIListenerCallback(
        'usage-total-changed', 'https://foo.com:443', usage, '10 cookies');
    assertTrue(testElement.$.noStorage.hidden);
    assertFalse(testElement.$.storage.hidden);
    assertTrue(testElement.$.usage.textContent!.includes(usage));
  });

  test('storage gets trashed properly', async function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    flush();

    const results = await Promise.all([
      browserProxy.whenCalled('getOriginPermissions'),
      websiteUsageProxy.whenCalled('fetchUsageTotal'),
    ]);

    const hostRequested = results[1];
    assertEquals('https://foo.com:443', hostRequested);
    webUIListenerCallback(
        'usage-total-changed', hostRequested, '1 KB', '10 cookies');
    assertEquals(
        '1 KB',
        testElement.shadowRoot!.querySelector(
                                   '#storedData')!.textContent!.trim());
    assertTrue(testElement.$.noStorage.hidden);
    assertFalse(testElement.$.storage.hidden);

    testElement.shadowRoot!
        .querySelector<HTMLElement>(
            '#confirmClearStorage .action-button')!.click();
    const originCleared = await websiteUsageProxy.whenCalled('clearUsage');
    assertEquals('https://foo.com/', originCleared);
  });

  test('cookies gets deleted properly', async function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    const results = await Promise.all([
      browserProxy.whenCalled('getOriginPermissions'),
      websiteUsageProxy.whenCalled('fetchUsageTotal'),
    ]);

    // Ensure the mock's methods were called and check usage was cleared
    // on clicking the trash button.
    const hostRequested = results[1];
    assertEquals('https://foo.com:443', hostRequested);
    webUIListenerCallback(
        'usage-total-changed', hostRequested, '1 KB', '10 cookies');
    assertEquals(
        '10 cookies',
        testElement.shadowRoot!.querySelector(
                                   '#numCookies')!.textContent!.trim());
    assertTrue(testElement.$.noStorage.hidden);
    assertFalse(testElement.$.storage.hidden);

    testElement.shadowRoot!
        .querySelector<HTMLElement>(
            '#confirmClearStorage .action-button')!.click();
    const originCleared = await websiteUsageProxy.whenCalled('clearUsage');

    assertEquals('https://foo.com/', originCleared);
    const metric =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');

    assertEquals(PrivacyElementInteractions.SITE_DETAILS_CLEAR_DATA, metric);
  });

  test('correct pref settings are shown', async function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');

    await browserProxy.whenCalled('isOriginValid').then(async () => {
      await browserProxy.whenCalled('getOriginPermissions');
    });

    const siteDetailsPermissions =
        testElement.shadowRoot!.querySelectorAll('site-details-permission');
    siteDetailsPermissions.forEach((siteDetailsPermission) => {
      if (!isChromeOS &&
          siteDetailsPermission.category ===
              ContentSettingsTypes.PROTECTED_CONTENT) {
        return;
      }

      // Verify settings match the values specified in |prefs|.
      let expectedSetting = ContentSetting.ALLOW;
      let expectedSource = SiteSettingSource.PREFERENCE;
      let expectedMenuValue = ContentSetting.ALLOW;

      // For all the categories with non-user-set 'Allow' preferences,
      // update expected values.
      if (siteDetailsPermission.category ===
              ContentSettingsTypes.NOTIFICATIONS ||
          siteDetailsPermission.category === ContentSettingsTypes.JAVASCRIPT ||
          siteDetailsPermission.category === ContentSettingsTypes.IMAGES ||
          siteDetailsPermission.category === ContentSettingsTypes.POPUPS ||
          siteDetailsPermission.category ===
              ContentSettingsTypes.FILE_SYSTEM_WRITE) {
        expectedSetting =
            prefs.exceptions[siteDetailsPermission.category][0]!.setting;
        expectedSource =
            prefs.exceptions[siteDetailsPermission.category][0]!.source;
        expectedMenuValue = expectedSource === SiteSettingSource.DEFAULT ?
            ContentSetting.DEFAULT :
            expectedSetting;
      }
      assertEquals(expectedSetting, siteDetailsPermission.site.setting);
      assertEquals(expectedSource, siteDetailsPermission.site.source);
      assertEquals(expectedMenuValue, siteDetailsPermission.$.permission.value);
    });
  });

  test('categories can be hidden', async function() {
    browserProxy.setPrefs(prefs);
    // Only the categories in this list should be visible to the user.
    browserProxy.setCategoryList(
        [ContentSettingsTypes.NOTIFICATIONS, ContentSettingsTypes.GEOLOCATION]);
    testElement = createSiteDetails('https://foo.com:443');

    await browserProxy.whenCalled('isOriginValid');
    await browserProxy.whenCalled('getOriginPermissions');

    testElement.shadowRoot!.querySelectorAll('site-details-permission')
        .forEach((siteDetailsPermission) => {
          const shouldBeVisible = siteDetailsPermission.category ===
                  ContentSettingsTypes.NOTIFICATIONS ||
              siteDetailsPermission.category ===
                  ContentSettingsTypes.GEOLOCATION;
          assertEquals(
              !shouldBeVisible, siteDetailsPermission.$.details.hidden);
        });
  });

  test('show confirmation dialog on reset settings', async function() {
    browserProxy.setPrefs(prefs);
    const origin = 'https://foo.com:443';
    testElement = createSiteDetails(origin);
    flush();

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.shadowRoot!
          .querySelector<HTMLElement>('#resetSettingsButton')!.click();
      assertTrue(testElement.$.confirmResetSettings.open);
      const actionButtonList =
          testElement.shadowRoot!.querySelectorAll<HTMLElement>(
              `#confirmResetSettings .${buttonType}`);
      assertEquals(1, actionButtonList.length);
      actionButtonList[0]!.click();
      assertFalse(testElement.$.confirmResetSettings.open);
    });

    // Accepting the dialog will make a call to setOriginPermissions.
    const args = await browserProxy.whenCalled('setOriginPermissions');
    assertEquals(origin, args[0]);
    assertDeepEquals(null, args[1]);
    assertEquals(ContentSetting.DEFAULT, args[2]);
  });

  test('show confirmation dialog on clear storage', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    flush();

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.shadowRoot!.querySelector<HTMLElement>(
                                 '#usage cr-button')!.click();
      assertTrue(testElement.$.confirmClearStorage.open);
      const actionButtonList =
          testElement.shadowRoot!.querySelectorAll<HTMLElement>(
              `#confirmClearStorage .${buttonType}`);
      assertEquals(1, actionButtonList.length);
      actionButtonList[0]!.click();
      assertFalse(testElement.$.confirmClearStorage.open);
    });
  });

  test('permissions update dynamically', function() {
    browserProxy.setPrefs(prefs);
    const origin = 'https://foo.com:443';
    testElement = createSiteDetails(origin);

    const elems =
        testElement.shadowRoot!.querySelectorAll('site-details-permission');
    const notificationPermission = Array.from(elems).find(
        elem => elem.category === ContentSettingsTypes.NOTIFICATIONS)!;

    // Wait for all the permissions to be populated initially.
    return browserProxy.whenCalled('isOriginValid')
        .then(() => {
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then(() => {
          // Make sure initial state is as expected.
          assertEquals(ContentSetting.ASK, notificationPermission.site.setting);
          assertEquals(
              SiteSettingSource.POLICY, notificationPermission.site.source);
          assertEquals(
              ContentSetting.ASK, notificationPermission.$.permission.value);

          // Set new prefs and make sure only that permission is updated.
          const newException = createRawSiteException(origin, {
            embeddingOrigin: origin,
            origin: origin,
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.DEFAULT,
          });
          browserProxy.resetResolver('getOriginPermissions');
          browserProxy.setSingleException(
              ContentSettingsTypes.NOTIFICATIONS, newException);
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then((args) => {
          // The notification pref was just updated, so make sure the call to
          // getOriginPermissions was to check notifications.
          assertTrue(args[1].includes(ContentSettingsTypes.NOTIFICATIONS));

          // Check |notificationPermission| now shows the new permission value.
          assertEquals(
              ContentSetting.BLOCK, notificationPermission.site.setting);
          assertEquals(
              SiteSettingSource.DEFAULT, notificationPermission.site.source);
          assertEquals(
              ContentSetting.DEFAULT,
              notificationPermission.$.permission.value);
        });
  });

  test('invalid origins navigate back', async function() {
    const invalid_url = 'invalid url';
    browserProxy.setIsOriginValid(false);

    Router.getInstance().navigateTo(routes.SITE_SETTINGS);

    testElement = createSiteDetails(invalid_url);
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);

    const args = await browserProxy.whenCalled('isOriginValid');
    assertEquals(invalid_url, args);
    await flushTasks();
    assertEquals(
        routes.SITE_SETTINGS.path, Router.getInstance().getCurrentRoute().path);
  });

  test('call fetch block autoplay status', async function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);
    await browserProxy.whenCalled('fetchBlockAutoplayStatus');
  });

  test(
      'check related website set membership label empty string',
      async function() {
        const origin = 'https://foo.com:443';
        browserProxy.setPrefs(prefs);
        testElement = createSiteDetails(origin);

        const results = await Promise.all([
          websiteUsageProxy.whenCalled('fetchUsageTotal'),
        ]);

        const hostRequested = results[0];
        assertEquals('https://foo.com:443', hostRequested);
        webUIListenerCallback(
            'usage-total-changed', hostRequested, '1 KB', '10 cookies', '');
        assertTrue(testElement.$.rwsMembership.hidden);
        assertEquals('', testElement.$.rwsMembership.textContent!.trim());
      });

  test(
      'check related website set membership label populated string',
      async function() {
        const origin = 'https://foo.com:443';
        browserProxy.setPrefs(prefs);
        testElement = createSiteDetails(origin);

        const results = await Promise.all([
          websiteUsageProxy.whenCalled('fetchUsageTotal'),
        ]);

        const hostRequested = results[0];
        assertEquals('https://foo.com:443', hostRequested);
        webUIListenerCallback(
            'usage-total-changed', hostRequested, '1 KB', '10 cookies',
            'Allowed for 1 foo.com site', false);
        assertFalse(testElement.$.rwsMembership.hidden);
        assertEquals(
            'Allowed for 1 foo.com site',
            testElement.$.rwsMembership.textContent!.trim());
        flush();
        // Assert related website set policy is null.
        const rwsPolicy =
            testElement.shadowRoot!.querySelector<HTMLElement>('#rwsPolicy');
        assertEquals(null, rwsPolicy);
      });

  test(
      'related website set policy shown when managed key is set to true',
      async function() {
        const origin = 'https://foo.com:443';
        browserProxy.setPrefs(prefs);
        testElement = createSiteDetails(origin);

        const results = await Promise.all([
          websiteUsageProxy.whenCalled('fetchUsageTotal'),
        ]);

        const hostRequested = results[0];
        assertEquals('https://foo.com:443', hostRequested);
        webUIListenerCallback(
            'usage-total-changed', hostRequested, '1 KB', '10 cookies',
            'Allowed for 1 foo.com site', true);
        assertFalse(testElement.$.rwsMembership.hidden);
        assertEquals(
            'Allowed for 1 foo.com site',
            testElement.$.rwsMembership.textContent!.trim());
        flush();
        // Assert related website set policy is shown.
        const rwsPolicy =
            testElement.shadowRoot!.querySelector<HTMLElement>('#rwsPolicy');
        assertFalse(rwsPolicy!.hidden);
      });

  test(
      'clear data dialog warns about ad personalization data removal',
      function() {
        const origin = 'https://foo.com:443';
        browserProxy.setPrefs(prefs);
        testElement = createSiteDetails(origin);

        flush();

        assertTrue(Boolean(testElement.shadowRoot!.querySelector<HTMLElement>(
            '#confirmClearStorage #adPersonalization')));
      });

  test(
      'empty site navigates to parent for invalid site param',
      async function() {
        // Confirm that when attempting to load the page without a provided
        // site, the page is navigated away.
        const invalid_url = '';
        browserProxy.setIsOriginValid(false);
        Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);

        testElement = createSiteDetails(invalid_url);
        assertEquals(
            routes.SITE_SETTINGS_SITE_DETAILS.path,
            Router.getInstance().getCurrentRoute().path);

        const args = await browserProxy.whenCalled('isOriginValid');
        assertEquals(invalid_url, args);
        await flushTasks();
        assertEquals(
            routes.SITE_SETTINGS_ALL.path,
            Router.getInstance().getCurrentRoute().path);
      });
});
