// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {MultiDeviceSettingsMode, MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// clang-format on

suite('Multidevice', function() {
  let multideviceSubpage = null;
  let browserProxy = null;
  // Although HOST_SET_MODES is effectively a constant, it cannot reference the
  // enum settings.MultiDeviceSettingsMode from here so its initialization is
  // deferred to the suiteSetup function.
  let HOST_SET_MODES;

  /**
   * Observably sets mode. Everything else remains unchanged.
   * @param {settings.MultiDeviceSettingsMode} newMode
   */
  function setMode(newMode) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          mode: newMode,
        });
    Polymer.dom.flush();
  }

  /**
   * Observably resets feature states so that each feature is supported if and
   * only if it is in the provided array. Everything else remains unchanged.
   * @param {Array<settings.MultiDeviceFeature>} supportedFeatures
   */
  function setSupportedFeatures(supportedFeatures) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          betterTogetherState:
              supportedFeatures.includes(
                  settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          instantTetheringState:
              supportedFeatures.includes(
                  settings.MultiDeviceFeature.INSTANT_TETHERING) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          messagesState:
              supportedFeatures.includes(settings.MultiDeviceFeature.MESSAGES) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          smartLockState: supportedFeatures.includes(
                              settings.MultiDeviceFeature.SMART_LOCK) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubState: supportedFeatures.includes(
                        settings.MultiDeviceFeature.PHONE_HUB) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubNotificationsState: supportedFeatures.includes(
                  settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubTaskContinuationState: supportedFeatures.includes(
                  settings.MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          wifiSyncState: supportedFeatures.includes(
                  settings.MultiDeviceFeature.WIFI_SYNC) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
        });
    Polymer.dom.flush();
  }

  /**
   * @param {boolean} pairingComplete
   */
  function setAndroidSmsPairingComplete(pairingComplete) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          messagesState: pairingComplete ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED,
          isAndroidSmsPairingComplete: pairingComplete,
        });
    Polymer.dom.flush();
  }

  suiteSetup(function() {
    HOST_SET_MODES = [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  setup(function() {
    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multideviceSubpage = document.createElement('settings-multidevice-subpage');
    multideviceSubpage.pageContentData = {hostDeviceName: 'Pixel XL'};
    setMode(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSupportedFeatures(Object.values(settings.MultiDeviceFeature));

    document.body.appendChild(multideviceSubpage);
    Polymer.dom.flush();
  });

  teardown(function() {
    multideviceSubpage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('individual features appear only if host is verified', function() {
    for (const mode of HOST_SET_MODES) {
      setMode(mode);
      assertEquals(
          !!multideviceSubpage.$$('#smartLockItem'),
          mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#instantTetheringItem'),
          mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#messagesItem'),
          mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubItem'),
          mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubNotificationsItem'),
          mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubTaskContinuationItem'),
          mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#wifiSyncItem'),
          mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    }
  });

  test(
      'individual features are attached only if they are supported',
      function() {
        assertTrue(!!multideviceSubpage.$$('#smartLockItem'));
        assertTrue(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertTrue(!!multideviceSubpage.$$('#messagesItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubTaskContinuationItem'));
        assertTrue(!!multideviceSubpage.$$('#wifiSyncItem'));

        setSupportedFeatures([
          settings.MultiDeviceFeature.SMART_LOCK,
          settings.MultiDeviceFeature.MESSAGES,
          settings.MultiDeviceFeature.PHONE_HUB,
          settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
          settings.MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION,
          settings.MultiDeviceFeature.WIFI_SYNC,
        ]);
        assertTrue(!!multideviceSubpage.$$('#smartLockItem'));
        assertFalse(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertTrue(!!multideviceSubpage.$$('#messagesItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubTaskContinuationItem'));
        assertTrue(!!multideviceSubpage.$$('#wifiSyncItem'));

        setSupportedFeatures([settings.MultiDeviceFeature.INSTANT_TETHERING]);
        assertFalse(!!multideviceSubpage.$$('#smartLockItem'));
        assertTrue(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertFalse(!!multideviceSubpage.$$('#messagesItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubTaskContinuationItem'));
        assertFalse(!!multideviceSubpage.$$('#wifiSyncItem'));

        setSupportedFeatures([]);
        assertFalse(!!multideviceSubpage.$$('#smartLockItem'));
        assertFalse(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertFalse(!!multideviceSubpage.$$('#messagesItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubTaskContinuationItem'));
        assertFalse(!!multideviceSubpage.$$('#wifiSyncItem'));
      });

  test('clicking SmartLock item routes to SmartLock subpage', function() {
    multideviceSubpage.$$('#smartLockItem').$$('.link-wrapper').click();
    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.SMART_LOCK);
  });

  test('AndroidMessages item shows button when not set up', function() {
    setAndroidSmsPairingComplete(false);
    Polymer.dom.flush();

    const controllerSelector = '#messagesItem > [slot=feature-controller]';
    assertTrue(!!multideviceSubpage.$$(controllerSelector));
    assertTrue(
        multideviceSubpage.$$(controllerSelector).tagName.includes('BUTTON'));

    setAndroidSmsPairingComplete(true);
    Polymer.dom.flush();

    assertFalse(!!multideviceSubpage.$$(controllerSelector));
  });

  test(
      'AndroidMessages set up button calls browser proxy function', function() {
        setAndroidSmsPairingComplete(false);
        Polymer.dom.flush();

        const setUpButton =
            multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
        assertTrue(!!setUpButton);

        setUpButton.click();

        return browserProxy.whenCalled('setUpAndroidSms');
      });

  test(
      'AndroidMessages toggle is disabled when prohibited by policy',
      function() {
        // Verify that setup button is disabled when prohibited by policy.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              messagesState:
                  settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              isAndroidSmsPairingComplete: false,
            });
        Polymer.dom.flush();

        let setUpButton =
            multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
        assertFalse(!!setUpButton);

        // Verify that setup button is not disabled when feature is enabled.
        setAndroidSmsPairingComplete(false);
        setUpButton =
            multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
        assertTrue(!!setUpButton);
        assertTrue(setUpButton.tagName.includes('BUTTON'));
        assertFalse(setUpButton.disabled);
      });

  test('Deep link to setup messages', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    setAndroidSmsPairingComplete(false);
    Polymer.dom.flush();

    const params = new URLSearchParams;
    params.append('settingId', '205');
    settings.Router.getInstance().navigateTo(
        settings.routes.MULTIDEVICE_FEATURES, params);

    Polymer.dom.flush();

    const deepLinkElement =
        multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Setup messages button should be focused for settingId=205.');
  });

  test('Deep link to messages on/off', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    setAndroidSmsPairingComplete(true);
    Polymer.dom.flush();

    const params = new URLSearchParams;
    params.append('settingId', '206');
    settings.Router.getInstance().navigateTo(
        settings.routes.MULTIDEVICE_FEATURES, params);

    Polymer.dom.flush();

    const deepLinkElement = multideviceSubpage.$$('#messagesItem')
                                .$$('settings-multidevice-feature-toggle')
                                .$$('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Messages on/off toggle should be focused for settingId=206.');
  });

  test('Deep link to phone hub on/off', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '209');
    settings.Router.getInstance().navigateTo(
        settings.routes.MULTIDEVICE_FEATURES, params);

    Polymer.dom.flush();

    const deepLinkElement = multideviceSubpage.$$('#phoneHubItem')
                                .$$('settings-multidevice-feature-toggle')
                                .$$('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Phone hub on/off toggle should be focused for settingId=209.');
  });
});
