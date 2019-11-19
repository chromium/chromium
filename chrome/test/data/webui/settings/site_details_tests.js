// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  // Initialize a site-details before each test.
  setup(function() {
    prefs = test_util.createSiteSettingsPrefs([], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.COOKIES,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.IMAGES,
          [test_util.createRawSiteException('https://foo.com:443', {
            source: settings.SiteSettingSource.DEFAULT,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.JAVASCRIPT,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.SOUND,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.PLUGINS,
          [test_util.createRawSiteException('https://foo.com:443', {
            source: settings.SiteSettingSource.EXTENSION,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.POPUPS,
          [test_util.createRawSiteException('https://foo.com:443', {
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.DEFAULT,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.GEOLOCATION,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.NOTIFICATIONS,
          [test_util.createRawSiteException('https://foo.com:443', {
            setting: settings.ContentSetting.ASK,
            source: settings.SiteSettingSource.POLICY,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.MIC,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.CAMERA,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.BACKGROUND_SYNC,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.MIDI_DEVICES,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.PROTECTED_CONTENT,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.ADS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.CLIPBOARD,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.SENSORS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.PAYMENT_HANDLER,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.SERIAL_PORTS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.BLUETOOTH_SCANNING,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE,
          [test_util.createRawSiteException('https://foo.com:443', {
            setting: settings.ContentSetting.BLOCK,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.MIXEDSCRIPT,
          [test_util.createRawSiteException('https://foo.com:443')]),
    ], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.USB_DEVICES,
          [test_util.createRawChooserException(
              settings.ChooserType.USB_DEVICES,
              [test_util.createRawSiteException('https://foo.com:443')])]),
    ]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
  });

  function createSiteDetails(origin) {
    const siteDetailsElement = document.createElement('site-details');
    document.body.appendChild(siteDetailsElement);
    siteDetailsElement.origin = origin;
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
    return siteDetailsElement;
  }

  test('all site settings are shown', function() {
    // Add ContentsSettingsTypes which are not supposed to be shown on the Site
    // Details page here.
    const nonSiteDetailsContentSettingsTypes = [
      settings.ContentSettingsTypes.COOKIES,
      settings.ContentSettingsTypes.PROTOCOL_HANDLERS,
      settings.ContentSettingsTypes.ZOOM_LEVELS,
    ];
    if (!cr.isChromeOS) {
      nonSiteDetailsContentSettingsTypes.push(
          settings.ContentSettingsTypes.PROTECTED_CONTENT);
    }
    const experimentalSiteDetailsContentSettingsTypes = [
      settings.ContentSettingsTypes.BLUETOOTH_SCANNING,
    ];

    // A list of optionally shown content settings mapped to their loadTimeData
    // flag string.
    const optionalSiteDetailsContentSettingsTypes =
        /** @type {!settings.ContentSettingsType : string} */ ({});
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes.ADS] =
        'enableSafeBrowsingSubresourceFilter';

    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .PAYMENT_HANDLER] =
        'enablePaymentHandlerContentSetting';

    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .NATIVE_FILE_SYSTEM_WRITE] =
        'enableNativeFileSystemWriteContentSetting';
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .MIXEDSCRIPT] =
        'enableInsecureContentContentSetting';

    browserProxy.setPrefs(prefs);

    // First, explicitly set all the optional settings to false.
    for (contentSetting in optionalSiteDetailsContentSettingsTypes) {
      const loadTimeDataOverride = {};
      loadTimeDataOverride
          [optionalSiteDetailsContentSettingsTypes[contentSetting]] = false;
      loadTimeData.overrideValues(loadTimeDataOverride);
    }

    // Iterate over each flag in on / off state, assuming that the on state
    // means the content setting will show, and off hides it.
    for (contentSetting in optionalSiteDetailsContentSettingsTypes) {
      const numContentSettings =
          Object.keys(settings.ContentSettingsTypes).length -
          nonSiteDetailsContentSettingsTypes.length -
          experimentalSiteDetailsContentSettingsTypes.length -
          Object.keys(optionalSiteDetailsContentSettingsTypes).length;

      const loadTimeDataOverride = {};
      loadTimeDataOverride
          [optionalSiteDetailsContentSettingsTypes[contentSetting]] = true;
      loadTimeData.overrideValues(loadTimeDataOverride);
      testElement = createSiteDetails('https://foo.com:443');
      assertEquals(
          numContentSettings + 1, testElement.getCategoryList().length);

      // Check for setting = off at the end to ensure that the setting does
      // not carry over for the next iteration.
      loadTimeDataOverride
          [optionalSiteDetailsContentSettingsTypes[contentSetting]] = false;
      loadTimeData.overrideValues(loadTimeDataOverride);
      testElement = createSiteDetails('https://foo.com:443');
      assertEquals(numContentSettings, testElement.getCategoryList().length);
    }

    const numContentSettings =
        Object.keys(settings.ContentSettingsTypes).length -
        nonSiteDetailsContentSettingsTypes.length -
        Object.keys(optionalSiteDetailsContentSettingsTypes).length;

    // Explicitly set all the optional settings to true.
    const loadTimeDataOverride = {};
    loadTimeDataOverride['enableExperimentalWebPlatformFeatures'] = true;
    loadTimeData.overrideValues(loadTimeDataOverride);
    testElement = createSiteDetails('https://foo.com:443');
    assertEquals(numContentSettings, testElement.getCategoryList().length);

    // Check for setting = off at the end to ensure that the setting does
    // not carry over for the next iteration.
    loadTimeDataOverride['enableExperimentalWebPlatformFeatures'] = false;
    loadTimeData.overrideValues(loadTimeDataOverride);
    testElement = createSiteDetails('https://foo.com:443');
    assertEquals(
        numContentSettings - experimentalSiteDetailsContentSettingsTypes.length,
        testElement.getCategoryList().length);
  });

  test('usage heading shows properly', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    Polymer.dom.flush();
    assert(!!testElement.$$('#usage'));

    // When there's no usage, there should be a string that says so.
    assertEquals('', testElement.storedData_);
    assertFalse(testElement.$$('#noStorage').hidden);
    assertTrue(testElement.$$('#storage').hidden);
    assertTrue(
        testElement.$$('#usage').innerText.indexOf('No usage data') != -1);

    // If there is, check the correct amount of usage is specified.
    testElement.storedData_ = '1 KB';
    assertTrue(testElement.$$('#noStorage').hidden);
    assertFalse(testElement.$$('#storage').hidden);
    assertTrue(testElement.$$('#usage').innerText.indexOf('1 KB') != -1);
  });

  test('storage gets trashed properly', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    // Remove the current website-usage-private-api element.
    const parent = testElement.$.usageApi.parentNode;
    assertTrue(parent != undefined);
    testElement.$.usageApi.remove();

    // Replace it with a mock version.
    let usageCleared = false;
    Polymer({
      is: 'mock-website-usage-private-api-storage',

      fetchUsageTotal: function(host) {
        testElement.storedData_ = '1 KB';
      },

      clearUsage: function(origin, task) {
        usageCleared = true;
      },
    });
    const api =
        document.createElement('mock-website-usage-private-api-storage');
    testElement.$.usageApi = api;
    parent.appendChild(api);
    Polymer.dom.flush();

    // Call onOriginChanged_() manually to simulate a new navigation.
    testElement.currentRouteChanged(settings.Route);
    return browserProxy.whenCalled('getOriginPermissions').then(() => {
      // Ensure the mock's methods were called and check usage was cleared on
      // clicking the trash button.
      assertEquals('1 KB', testElement.storedData_);
      assertTrue(testElement.$$('#noStorage').hidden);
      assertFalse(testElement.$$('#storage').hidden);

      testElement.$$('#confirmClearStorage .action-button').click();
      assertTrue(usageCleared);
    });
  });

  test('cookies gets deleted properly', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    // Remove the current website-usage-private-api element.
    const parent = testElement.$.usageApi.parentNode;
    assertTrue(parent != undefined);
    testElement.$.usageApi.remove();

    // Replace it with a mock version.
    let usageCleared = false;
    Polymer({
      is: 'mock-website-usage-private-api-cookies',

      fetchUsageTotal: function(host) {
        testElement.numCookies_ = '10 cookies';
      },

      clearUsage: function(origin, task) {
        usageCleared = true;
      },
    });
    const api =
        document.createElement('mock-website-usage-private-api-cookies');
    testElement.$.usageApi = api;
    parent.appendChild(api);
    Polymer.dom.flush();

    // Call onOriginChanged_() manually to simulate a new navigation.
    testElement.currentRouteChanged(settings.Route);
    return browserProxy.whenCalled('getOriginPermissions').then(() => {
      // Ensure the mock's methods were called and check usage was cleared on
      // clicking the trash button.
      assertEquals('10 cookies', testElement.numCookies_);
      assertTrue(testElement.$$('#noStorage').hidden);
      assertFalse(testElement.$$('#storage').hidden);

      testElement.$$('#confirmClearStorage .action-button').click();
      assertTrue(usageCleared);
    });
  });

  test('correct pref settings are shown', function() {
    browserProxy.setPrefs(prefs);
    // Make sure all the possible content settings are shown for this test.
    loadTimeData.overrideValues({enableSafeBrowsingSubresourceFilter: true});
    loadTimeData.overrideValues({enablePaymentHandlerContentSetting: true});
    loadTimeData.overrideValues(
        {enableNativeFileSystemWriteContentSetting: true});
    testElement = createSiteDetails('https://foo.com:443');

    return browserProxy.whenCalled('isOriginValid')
        .then(() => {
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then(() => {
          testElement.root.querySelectorAll('site-details-permission')
              .forEach((siteDetailsPermission) => {
                if (!cr.isChromeOS &&
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.PROTECTED_CONTENT) {
                  return;
                }

                // Verify settings match the values specified in |prefs|.
                let expectedSetting = settings.ContentSetting.ALLOW;
                let expectedSource = settings.SiteSettingSource.PREFERENCE;
                let expectedMenuValue = settings.ContentSetting.ALLOW;

                // For all the categories with non-user-set 'Allow' preferences,
                // update expected values.
                if (siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.NOTIFICATIONS ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.PLUGINS ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.JAVASCRIPT ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.IMAGES ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.POPUPS ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes
                            .NATIVE_FILE_SYSTEM_WRITE) {
                  expectedSetting =
                      prefs.exceptions[siteDetailsPermission.category][0]
                          .setting;
                  expectedSource =
                      prefs.exceptions[siteDetailsPermission.category][0]
                          .source;
                  expectedMenuValue =
                      (expectedSource == settings.SiteSettingSource.DEFAULT) ?
                      settings.ContentSetting.DEFAULT :
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
    Polymer.dom.flush();

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
      assertEquals(settings.ContentSetting.DEFAULT, args[2]);
    });
  });

  test('show confirmation dialog on clear storage', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');

    // Give |testElement.storedData_| a non-empty value to make the clear
    // storage button appear. Also replace the website-usage-private-api element
    // to prevent a call going back to the C++ upon confirming the dialog.
    const parent = testElement.$.usageApi.parentNode;
    assertTrue(parent != undefined);
    testElement.$.usageApi.remove();
    Polymer({
      // Use a different mock name here to avoid an error when all tests are run
      // together as there is no way to unregister a Polymer custom element.
      is: 'mock1-website-usage-private-api',
      fetchUsageTotal: function() {
        testElement.storedData_ = '1 KB';
      },
      clearUsage: function(origin) {},
    });
    const api = document.createElement('mock1-website-usage-private-api');
    testElement.$.usageApi = api;
    parent.appendChild(api);
    Polymer.dom.flush();

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.$$('#usage cr-button').click();
      assertTrue(testElement.$.confirmClearStorage.open);
      const actionButtonList =
          testElement.$.confirmClearStorage.getElementsByClassName(buttonType);
      assertEquals(1, actionButtonList.length);
      testElement.storedData_ = '';
      actionButtonList[0].click();
      assertFalse(testElement.$.confirmClearStorage.open);
    });
  });

  test('permissions update dynamically', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');

    const siteDetailsPermission =
        testElement.root.querySelector('#notifications');

    // Wait for all the permissions to be populated initially.
    return browserProxy.whenCalled('isOriginValid')
        .then(() => {
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then(() => {
          // Make sure initial state is as expected.
          assertEquals(
              settings.ContentSetting.ASK, siteDetailsPermission.site.setting);
          assertEquals(
              settings.SiteSettingSource.POLICY,
              siteDetailsPermission.site.source);
          assertEquals(
              settings.ContentSetting.ASK,
              siteDetailsPermission.$.permission.value);

          // Set new prefs and make sure only that permission is updated.
          const newException = {
            embeddingOrigin: testElement.origin,
            origin: testElement.origin,
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.DEFAULT,
          };
          browserProxy.resetResolver('getOriginPermissions');
          browserProxy.setSingleException(
              settings.ContentSettingsTypes.NOTIFICATIONS, newException);
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then((args) => {
          // The notification pref was just updated, so make sure the call to
          // getOriginPermissions was to check notifications.
          assertTrue(
              args[1].includes(settings.ContentSettingsTypes.NOTIFICATIONS));

          // Check |siteDetailsPermission| now shows the new permission value.
          assertEquals(
              settings.ContentSetting.BLOCK,
              siteDetailsPermission.site.setting);
          assertEquals(
              settings.SiteSettingSource.DEFAULT,
              siteDetailsPermission.site.source);
          assertEquals(
              settings.ContentSetting.DEFAULT,
              siteDetailsPermission.$.permission.value);
        });
  });

  test('invalid origins navigate back', function() {
    const invalid_url = 'invalid url';
    browserProxy.setIsOriginValid(false);

    settings.navigateTo(settings.routes.SITE_SETTINGS);

    testElement = createSiteDetails(invalid_url);
    assertEquals(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.getCurrentRoute().path);
    return browserProxy.whenCalled('isOriginValid')
        .then((args) => {
          assertEquals(invalid_url, args);
          return new Promise((resolve) => {
            listenOnce(window, 'popstate', resolve);
          });
        })
        .then(() => {
          assertEquals(
              settings.routes.SITE_SETTINGS.path,
              settings.getCurrentRoute().path);
        });
  });

  test('call fetch block autoplay status', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);
    return browserProxy.whenCalled('fetchBlockAutoplayStatus');
  });

});
