// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessStatus, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('Multidevice', function() {
  let multideviceSubpage = null;
  let browserProxy = null;
  // Although HOST_SET_MODES is effectively a constant, it cannot reference the
  // enum MultiDeviceSettingsMode from here so its initialization is
  // deferred to the suiteSetup function.
  let HOST_SET_MODES;

  /**
   * Observably sets mode. Everything else remains unchanged.
   * @param {MultiDeviceSettingsMode} newMode
   */
  function setMode(newMode) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          mode: newMode,
        });
    flush();
  }

  /**
   * Observably resets feature states so that each feature is supported if and
   * only if it is in the provided array. Everything else remains unchanged.
   * @param {Array<MultiDeviceFeature>} supportedFeatures
   */
  function setSupportedFeatures(supportedFeatures) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          betterTogetherState: supportedFeatures.includes(
                                   MultiDeviceFeature.BETTER_TOGETHER_SUITE) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          instantTetheringState:
              supportedFeatures.includes(MultiDeviceFeature.INSTANT_TETHERING) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          messagesState:
              supportedFeatures.includes(MultiDeviceFeature.MESSAGES) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          smartLockState:
              supportedFeatures.includes(MultiDeviceFeature.SMART_LOCK) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubState:
              supportedFeatures.includes(MultiDeviceFeature.PHONE_HUB) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubNotificationsState:
              supportedFeatures.includes(
                  MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubTaskContinuationState:
              supportedFeatures.includes(
                  MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          wifiSyncState:
              supportedFeatures.includes(MultiDeviceFeature.WIFI_SYNC) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubAppsState:
              supportedFeatures.includes(MultiDeviceFeature.ECHE) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          phoneHubCameraRollState:
              supportedFeatures.includes(
                  MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL) ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
        });
    flush();
  }

  /**
   * @param {boolean} pairingComplete
   */
  function setAndroidSmsPairingComplete(pairingComplete) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          messagesState: pairingComplete ?
              MultiDeviceFeatureState.ENABLED_BY_USER :
              MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED,
          isAndroidSmsPairingComplete: pairingComplete,
        });
    flush();
  }

  suiteSetup(function() {
    HOST_SET_MODES = [
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  setup(function() {
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multideviceSubpage = document.createElement('settings-multidevice-subpage');
    multideviceSubpage.pageContentData = {hostDeviceName: 'Pixel XL'};
    setMode(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSupportedFeatures(Object.values(MultiDeviceFeature));

    document.body.appendChild(multideviceSubpage);
    flush();
  });

  teardown(function() {
    multideviceSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('individual features appear only if host is verified', function() {
    for (const mode of HOST_SET_MODES) {
      setMode(mode);
      assertEquals(
          !!multideviceSubpage.$$('#smartLockItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#instantTetheringItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#messagesItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubNotificationsItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubTaskContinuationItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#wifiSyncItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubAppsItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#phoneHubCameraRollItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
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
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));

        setSupportedFeatures([
          MultiDeviceFeature.SMART_LOCK,
          MultiDeviceFeature.MESSAGES,
          MultiDeviceFeature.PHONE_HUB,
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
          MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION,
          MultiDeviceFeature.WIFI_SYNC,
          MultiDeviceFeature.ECHE,
          MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL,
        ]);
        assertTrue(!!multideviceSubpage.$$('#smartLockItem'));
        assertFalse(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertTrue(!!multideviceSubpage.$$('#messagesItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubTaskContinuationItem'));
        assertTrue(!!multideviceSubpage.$$('#wifiSyncItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));

        setSupportedFeatures([MultiDeviceFeature.INSTANT_TETHERING]);
        assertFalse(!!multideviceSubpage.$$('#smartLockItem'));
        assertTrue(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertFalse(!!multideviceSubpage.$$('#messagesItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubTaskContinuationItem'));
        assertFalse(!!multideviceSubpage.$$('#wifiSyncItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));

        setSupportedFeatures([]);
        assertFalse(!!multideviceSubpage.$$('#smartLockItem'));
        assertFalse(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertFalse(!!multideviceSubpage.$$('#messagesItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubTaskContinuationItem'));
        assertFalse(!!multideviceSubpage.$$('#wifiSyncItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
      });

  test(
      'SmartLock item routes to subpage with isSmartLockSignInRemoved disabled',
      function() {
        multideviceSubpage.remove();
        loadTimeData.overrideValues({'isSmartLockSignInRemoved': false});
        browserProxy = new TestMultideviceBrowserProxy();
        MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

        PolymerTest.clearBody();
        multideviceSubpage =
            document.createElement('settings-multidevice-subpage');
        multideviceSubpage.pageContentData = {hostDeviceName: 'Pixel XL'};
        setMode(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
        setSupportedFeatures(Object.values(MultiDeviceFeature));

        document.body.appendChild(multideviceSubpage);
        flush();

        multideviceSubpage.$$('#smartLockItem').$$('.link-wrapper').click();
        assertEquals(Router.getInstance().getCurrentRoute(), routes.SMART_LOCK);
      });

  test(
      'setting isSmartLockSignInRemoved flag removes SmartLock subpage route',
      function() {
        multideviceSubpage.remove();
        loadTimeData.overrideValues({'isSmartLockSignInRemoved': true});
        browserProxy = new TestMultideviceBrowserProxy();
        MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

        PolymerTest.clearBody();
        multideviceSubpage =
            document.createElement('settings-multidevice-subpage');
        multideviceSubpage.pageContentData = {hostDeviceName: 'Pixel XL'};
        setMode(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
        setSupportedFeatures(Object.values(MultiDeviceFeature));

        document.body.appendChild(multideviceSubpage);
        flush();

        assertEquals(
            undefined, multideviceSubpage.$$('#smartLockItem').subpageRoute);
        const routeBefore = Router.getInstance().getCurrentRoute();
        multideviceSubpage.$$('#smartLockItem').$$('.link-wrapper').click();
        assertEquals(Router.getInstance().getCurrentRoute(), routeBefore);

        loadTimeData.overrideValues({'isSmartLockSignInRemoved': false});
      });

  test('AndroidMessages item shows button when not set up', function() {
    setAndroidSmsPairingComplete(false);
    flush();

    const controllerSelector = '#messagesItem > [slot=feature-controller]';
    assertTrue(!!multideviceSubpage.$$(controllerSelector));
    assertTrue(
        multideviceSubpage.$$(controllerSelector).tagName.includes('BUTTON'));

    setAndroidSmsPairingComplete(true);
    flush();

    assertFalse(!!multideviceSubpage.$$(controllerSelector));
  });

  test(
      'AndroidMessages set up button calls browser proxy function', function() {
        setAndroidSmsPairingComplete(false);
        flush();

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
              messagesState: MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              isAndroidSmsPairingComplete: false,
            });
        flush();

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
    setAndroidSmsPairingComplete(false);
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '205');
    Router.getInstance().navigateTo(routes.MULTIDEVICE_FEATURES, params);

    flush();

    const deepLinkElement =
        multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Setup messages button should be focused for settingId=205.');
  });

  test('Deep link to messages on/off', async () => {
    setAndroidSmsPairingComplete(true);
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '206');
    Router.getInstance().navigateTo(routes.MULTIDEVICE_FEATURES, params);

    flush();

    const deepLinkElement = multideviceSubpage.$$('#messagesItem')
                                .$$('settings-multidevice-feature-toggle')
                                .$$('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Messages on/off toggle should be focused for settingId=206.');
  });

  test('Deep link to phone hub on/off', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '209');
    Router.getInstance().navigateTo(routes.MULTIDEVICE_FEATURES, params);

    flush();

    const deepLinkElement = multideviceSubpage.$$('#phoneHubItem')
                                .$$('settings-multidevice-feature-toggle')
                                .$$('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Phone hub on/off toggle should be focused for settingId=209.');
  });

  test('Deep link to phone hub apps on/off', async () => {
    multideviceSubpage.pageContentData = Object.assign(
        {}, multideviceSubpage.pageContentData,
        {isPhoneHubAppsAccessGranted: true});
    const params = new URLSearchParams();
    params.append('settingId', '218');
    Router.getInstance().navigateTo(routes.MULTIDEVICE_FEATURES, params);

    flush();

    const deepLinkElement = multideviceSubpage.$$('#phoneHubAppsItem')
                                .$$('settings-multidevice-feature-toggle')
                                .$$('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Phone hub apps on/off toggle should be focused for settingId=218.');
  });

  test(
      'Phone Hub Camera Roll, Notifications, Apps and Combined items are shown/hidden correctly',
      function() {
        setSupportedFeatures([
          MultiDeviceFeature.PHONE_HUB,
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        ]);

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        setSupportedFeatures([
          MultiDeviceFeature.PHONE_HUB,
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
          MultiDeviceFeature.ECHE,
        ]);

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              isPhoneHubPermissionsDialogSupported: false,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();
        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              isPhoneHubPermissionsDialogSupported: false,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();
        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              isPhoneHubPermissionsDialogSupported: true,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        const controllerSelector =
            '#phoneHubAppsItem > [slot=feature-controller]';
        assertTrue(!!multideviceSubpage.$$(controllerSelector));
        assertTrue(multideviceSubpage.$$(controllerSelector)
                       .tagName.includes('BUTTON'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              isPhoneHubPermissionsDialogSupported: true,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        setSupportedFeatures([
          MultiDeviceFeature.PHONE_HUB,
          MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL,
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        ]);

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        setSupportedFeatures([
          MultiDeviceFeature.PHONE_HUB,
          MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL,
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
          MultiDeviceFeature.ECHE,
        ]);

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));
      });

  test(
      'Enterprise policies should properly affect Phone Hub Camera Roll, Notifications, Apps, and Combined items.',
      function() {
        setSupportedFeatures([
          MultiDeviceFeature.PHONE_HUB,
          MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL,
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
          MultiDeviceFeature.ECHE,
        ]);
        // Test CrOS enterprise policy:
        // Notifications, CameraRoll and Apps features are not grant, but
        // Notifications is prohibited. Should show Notifications and combined
        // settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubCameraRollState: MultiDeviceFeatureState.DISABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        // Notifications, CameraRoll and Apps features are not grant, but
        // cameraRoll is prohibited. Should show cameraRoll and combined
        // settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubCameraRollState:
                  MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.DISABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        // Notifications, CameraRoll and Apps features are not grant, but Apps
        // is prohibited. Should show Apps and combined settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubCameraRollState: MultiDeviceFeatureState.DISABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.DISABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        // Notifications, CameraRoll and Apps features are not grant, but
        // Notifications and CameraRoll are prohibited. Should show
        // Notifications, CameraRoll and Apps and hide combined settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubCameraRollState:
                  MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        // Notifications, CameraRoll and Apps features are not grant, but Phone
        // Hub (top feature) is prohibited. Should show Notifications,
        // CameraRoll and Apps and hide combined settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubState: MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              phoneHubCameraRollState:
                  MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        // Test Phone's enterprise policy:
        // Notifications, CameraRoll and Apps features are not grant, but
        // phone's notifications policy is disabled. Should show Notifications
        // and combined settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubState: MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubCameraRollState: MultiDeviceFeatureState.DISABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.DISABLED_BY_USER,
              notificationAccessStatus: PhoneHubFeatureAccessStatus.PROHIBITED,
              phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        // Notifications, CameraRoll and Apps features are not grant, but
        // phone's apps streaming policy is disabled. Should show Apps and
        // combined settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubState: MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubCameraRollState: MultiDeviceFeatureState.DISABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.DISABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.PROHIBITED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));

        // Notifications, CameraRoll and Apps features are not grant, but
        // phone's notification and apps streaming policy are disabled. Should
        // show Notifications, CameraRoll and Apps and hide combined settings.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              isPhoneHubPermissionsDialogSupported: true,
              phoneHubState: MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubCameraRollState: MultiDeviceFeatureState.DISABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.DISABLED_BY_USER,
              notificationAccessStatus: PhoneHubFeatureAccessStatus.PROHIBITED,
              phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.PROHIBITED
            });

        flush();

        assertTrue(!!multideviceSubpage.$$('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.$$('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.$$('#phoneHubCombinedSetupItem'));
      });
});
