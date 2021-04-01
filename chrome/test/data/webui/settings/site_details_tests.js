// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {flush,Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ChooserType,ContentSetting,ContentSettingsTypes,SiteSettingSource,SiteSettingsPrefsBrowserProxyImpl,WebsiteUsageBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, Route,Router,routes} from 'chrome://settings/settings.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createRawChooserException,createRawSiteException,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

class TestWebsiteUsageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['clearUsage', 'fetchUsageTotal']);
  }

  /** @override */
  fetchUsageTotal(host) {
    this.methodCalled('fetchUsageTotal', host);
  }

  /** @override */
  clearUsage(origin) {
    this.methodCalled('clearUsage', origin);
  }
}

/** @fileoverview Suite of tests for site-details. */
suite('SiteDetails', function() {
  /**
   * A site list element created before each test.
   * @type {SiteDetails}
   */
  let testElement;

  /**
   * An example pref with 1 pref in each category.
   * @type {SiteSettingsPref}
   */
  let prefs;

  /**
   * The mock site settings prefs proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /**
   * The mock website usage proxy object to use during test.
   * @type {TestWebsiteUsageBrowserProxy}
   */
  let websiteUsageProxy;

  // Initialize a site-details before each test.
  setup(function() {
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
              ContentSettingsTypes.WINDOW_PLACEMENT,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.FONT_ACCESS,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.IDLE_DETECTION,
              [createRawSiteException('https://foo.com:443')]),
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.FILE_HANDLING,
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
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    websiteUsageProxy = new TestWebsiteUsageBrowserProxy();
    WebsiteUsageBrowserProxyImpl.instance_ = websiteUsageProxy;

    PolymerTest.clearBody();
  });

  function createSiteDetails(origin) {
    const siteDetailsElement = document.createElement('site-details');
    document.body.appendChild(siteDetailsElement);
    siteDetailsElement.origin = origin;
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
    return siteDetailsElement;
  }

  test('all site settings are shown', function() {
    // Add ContentsSettingsTypes which are not supposed to be shown on the Site
    // Details page here.
    const nonSiteDetailsContentSettingsTypes = [
      ContentSettingsTypes.COOKIES,
      ContentSettingsTypes.PROTOCOL_HANDLERS,
      ContentSettingsTypes.ZOOM_LEVELS,
    ];
    if (!isChromeOS) {
      nonSiteDetailsContentSettingsTypes.push(
          ContentSettingsTypes.PROTECTED_CONTENT);
    }

    // A list of optionally shown content settings mapped to their loadTimeData
    // flag string.
    const optionalSiteDetailsContentSettingsTypes =
        /** @type {!ContentSettingsType : string} */ ({});
    optionalSiteDetailsContentSettingsTypes[ContentSettingsTypes
                                                .BLUETOOTH_SCANNING] =
        'enableExperimentalWebPlatformFeatures';
    optionalSiteDetailsContentSettingsTypes[ContentSettingsTypes
                                                .WINDOW_PLACEMENT] =
        'enableExperimentalWebPlatformFeatures';
    optionalSiteDetailsContentSettingsTypes[ContentSettingsTypes
                                                .PAYMENT_HANDLER] =
        'enablePaymentHandlerContentSetting';
    optionalSiteDetailsContentSettingsTypes[ContentSettingsTypes.ADS] =
        'enableSafeBrowsingSubresourceFilter';
    optionalSiteDetailsContentSettingsTypes[ContentSettingsTypes
                                                .BLUETOOTH_DEVICES] =
        'enableWebBluetoothNewPermissionsBackend';
    optionalSiteDetailsContentSettingsTypes[ContentSettingsTypes.FONT_ACCESS] =
        'enableFontAccessContentSetting';
    optionalSiteDetailsContentSettingsTypes[ContentSettingsTypes
                                                .FILE_HANDLING] =
        'enableFileHandlingContentSetting';

    const controlledSettingsCount = /** @type{string : int } */ ({});

    controlledSettingsCount['enableExperimentalWebPlatformFeatures'] = 2;
    controlledSettingsCount['enableFileHandlingContentSetting'] = 1;
    controlledSettingsCount['enableFontAccessContentSetting'] = 1;
    controlledSettingsCount['enablePaymentHandlerContentSetting'] = 1;
    controlledSettingsCount['enableSafeBrowsingSubresourceFilter'] = 1;
    controlledSettingsCount['enableWebBluetoothNewPermissionsBackend'] = 1;

    browserProxy.setPrefs(prefs);

    // First, explicitly set all the optional settings to false.
    for (const contentSetting in optionalSiteDetailsContentSettingsTypes) {
      const loadTimeDataOverride = {};
      loadTimeDataOverride[optionalSiteDetailsContentSettingsTypes
                               [contentSetting]] = false;
      loadTimeData.overrideValues(loadTimeDataOverride);
    }

    // Iterate over each flag in on / off state, assuming that the on state
    // means the content setting will show, and off hides it.
    for (const contentSetting in optionalSiteDetailsContentSettingsTypes) {
      const numContentSettings = Object.keys(ContentSettingsTypes).length -
          nonSiteDetailsContentSettingsTypes.length -
          Object.keys(optionalSiteDetailsContentSettingsTypes).length;

      const loadTimeDataOverride = {};
      loadTimeDataOverride[optionalSiteDetailsContentSettingsTypes
                               [contentSetting]] = true;
      loadTimeData.overrideValues(loadTimeDataOverride);
      testElement = createSiteDetails('https://foo.com:443');
      assertEquals(
          numContentSettings +
              controlledSettingsCount[optionalSiteDetailsContentSettingsTypes[
                  [contentSetting]]],
          testElement.getCategoryList().length);

      // Check for setting = off at the end to ensure that the setting does
      // not carry over for the next iteration.
      loadTimeDataOverride[optionalSiteDetailsContentSettingsTypes
                               [contentSetting]] = false;
      loadTimeData.overrideValues(loadTimeDataOverride);
      testElement = createSiteDetails('https://foo.com:443');
      assertEquals(numContentSettings, testElement.getCategoryList().length);
    }
  });

  test('usage heading shows properly', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    flush();
    assertTrue(!!testElement.$$('#usage'));

    // When there's no usage, there should be a string that says so.
    assertEquals('', testElement.storedData_);
    assertFalse(testElement.$$('#noStorage').hidden);
    assertTrue(testElement.$$('#storage').hidden);
    assertTrue(
        testElement.$$('#usage').innerText.indexOf('No usage data') !== -1);

    // If there is, check the correct amount of usage is specified.
    testElement.storedData_ = '1 KB';
    assertTrue(testElement.$$('#noStorage').hidden);
    assertFalse(testElement.$$('#storage').hidden);
    assertTrue(testElement.$$('#usage').innerText.indexOf('1 KB') !== -1);
  });

  test('storage gets trashed properly', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    flush();

    // Call onOriginChanged_() manually to simulate a new navigation.
    testElement.currentRouteChanged(Route);
    return Promise
        .all([
          browserProxy.whenCalled('getOriginPermissions'),
          websiteUsageProxy.whenCalled('fetchUsageTotal'),
        ])
        .then(results => {
          const hostRequested = results[1];
          assertEquals('foo.com', hostRequested);
          webUIListenerCallback(
              'usage-total-changed', hostRequested, '1 KB', '10 cookies');
          assertEquals('1 KB', testElement.storedData_);
          assertTrue(testElement.$$('#noStorage').hidden);
          assertFalse(testElement.$$('#storage').hidden);

          testElement.$$('#confirmClearStorageNew .action-button').click();
          return websiteUsageProxy.whenCalled('clearUsage');
        })
        .then(originCleared => {
          assertEquals('https://foo.com/', originCleared);
        });
  });

  test('cookies gets deleted properly', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    // Call onOriginChanged_() manually to simulate a new navigation.
    testElement.currentRouteChanged(Route);
    return Promise
        .all([
          browserProxy.whenCalled('getOriginPermissions'),
          websiteUsageProxy.whenCalled('fetchUsageTotal'),
        ])
        .then(results => {
          // Ensure the mock's methods were called and check usage was cleared
          // on clicking the trash button.
          const hostRequested = results[1];
          assertEquals('foo.com', hostRequested);
          webUIListenerCallback(
              'usage-total-changed', hostRequested, '1 KB', '10 cookies');
          assertEquals('10 cookies', testElement.numCookies_);
          assertTrue(testElement.$$('#noStorage').hidden);
          assertFalse(testElement.$$('#storage').hidden);

          testElement.$$('#confirmClearStorageNew .action-button').click();
          return websiteUsageProxy.whenCalled('clearUsage');
        })
        .then(originCleared => {
          assertEquals('https://foo.com/', originCleared);
          return testMetricsBrowserProxy.whenCalled(
              'recordSettingsPageHistogram');
        })
        .then(metric => {
          assertEquals(
              PrivacyElementInteractions.SITE_DETAILS_CLEAR_DATA, metric);
        });
  });

  test('correct pref settings are shown', function() {
    browserProxy.setPrefs(prefs);
    // Make sure all the possible content settings are shown for this test.
    loadTimeData.overrideValues({
      enableExperimentalWebPlatformFeatures: true,
      enableFileSystemWriteContentSetting: true,
      enableFontAccessContentSetting: true,
      enableFileHandlingContentSetting: true,
      enablePaymentHandlerContentSetting: true,
      enableSafeBrowsingSubresourceFilter: true,
      enableWebBluetoothNewPermissionsBackend: true,
    });
    testElement = createSiteDetails('https://foo.com:443');

    return browserProxy.whenCalled('isOriginValid')
        .then(() => {
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then(() => {
          testElement.root.querySelectorAll('site-details-permission')
              .forEach((siteDetailsPermission) => {
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
                    siteDetailsPermission.category ===
                        ContentSettingsTypes.JAVASCRIPT ||
                    siteDetailsPermission.category ===
                        ContentSettingsTypes.IMAGES ||
                    siteDetailsPermission.category ===
                        ContentSettingsTypes.POPUPS ||
                    siteDetailsPermission.category ===
                        ContentSettingsTypes.FILE_SYSTEM_WRITE) {
                  expectedSetting =
                      prefs.exceptions[siteDetailsPermission.category][0]
                          .setting;
                  expectedSource =
                      prefs.exceptions[siteDetailsPermission.category][0]
                          .source;
                  expectedMenuValue =
                      (expectedSource === SiteSettingSource.DEFAULT) ?
                      ContentSetting.DEFAULT :
                      expectedSetting;
                }
                assertEquals(
                    expectedSetting, siteDetailsPermission.site.setting);
                assertEquals(expectedSource, siteDetailsPermission.site.source);
                assertEquals(
                    expectedMenuValue,
                    siteDetailsPermission.$.permission.value);
              });
        });
  });

  test('show confirmation dialog on reset settings', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    flush();

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.$$('#resetSettingsButton').click();
      assertTrue(testElement.$.confirmResetSettings.open);
      const actionButtonList =
          testElement.$.confirmResetSettings.getElementsByClassName(buttonType);
      assertEquals(1, actionButtonList.length);
      actionButtonList[0].click();
      assertFalse(testElement.$.confirmResetSettings.open);
    });

    // Accepting the dialog will make a call to setOriginPermissions.
    return browserProxy.whenCalled('setOriginPermissions').then((args) => {
      assertEquals(testElement.origin, args[0]);
      assertDeepEquals(testElement.getCategoryList(), args[1]);
      assertEquals(ContentSetting.DEFAULT, args[2]);
    });
  });

  test('show confirmation dialog on clear storage', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    flush();

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.$$('#usage cr-button').click();
      assertTrue(testElement.$.confirmClearStorageNew.open);
      const actionButtonList =
          testElement.$.confirmClearStorageNew.getElementsByClassName(
              buttonType);
      assertEquals(1, actionButtonList.length);
      testElement.storedData_ = '';
      actionButtonList[0].click();
      assertFalse(testElement.$.confirmClearStorageNew.open);
    });
  });

  test('permissions update dynamically', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');

    const elems = testElement.root.querySelectorAll('site-details-permission');
    const notificationPermission = Array.from(elems).find(
        elem => elem.category === ContentSettingsTypes.NOTIFICATIONS);

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
          const newException = {
            embeddingOrigin: testElement.origin,
            origin: testElement.origin,
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.DEFAULT,
          };
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

  test('invalid origins navigate back', function() {
    const invalid_url = 'invalid url';
    browserProxy.setIsOriginValid(false);

    Router.getInstance().navigateTo(routes.SITE_SETTINGS);

    testElement = createSiteDetails(invalid_url);
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
    return browserProxy.whenCalled('isOriginValid')
        .then((args) => {
          assertEquals(invalid_url, args);
          return new Promise((resolve) => {
            listenOnce(window, 'popstate', resolve);
          });
        })
        .then(() => {
          assertEquals(
              routes.SITE_SETTINGS.path,
              Router.getInstance().getCurrentRoute().path);
        });
  });

  test('call fetch block autoplay status', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);
    return browserProxy.whenCalled('fetchBlockAutoplayStatus');
  });
});
