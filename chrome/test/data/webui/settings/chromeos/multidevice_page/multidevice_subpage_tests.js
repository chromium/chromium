// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessStatus, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

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
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

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
          !!multideviceSubpage.shadowRoot.querySelector('#smartLockItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector(
              '#instantTetheringItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector('#messagesItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector(
              '#phoneHubNotificationsItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector(
              '#phoneHubTaskContinuationItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector('#wifiSyncItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.shadowRoot.querySelector(
              '#phoneHubCameraRollItem'),
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    }
  });

  test(
      'individual features are attached only if they are supported',
      function() {
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#smartLockItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#instantTetheringItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#messagesItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubTaskContinuationItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#wifiSyncItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));

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
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#smartLockItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#instantTetheringItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#messagesItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubTaskContinuationItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#wifiSyncItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));

        setSupportedFeatures([MultiDeviceFeature.INSTANT_TETHERING]);
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#smartLockItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#instantTetheringItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#messagesItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubTaskContinuationItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#wifiSyncItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));

        setSupportedFeatures([]);
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#smartLockItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#instantTetheringItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#messagesItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubTaskContinuationItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#wifiSyncItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
      });

  test(
      'setting isSmartLockSignInRemoved flag removes SmartLock subpage route',
      function() {
        multideviceSubpage.remove();
        loadTimeData.overrideValues({'isSmartLockSignInRemoved': true});
        browserProxy = new TestMultideviceBrowserProxy();
        MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

        PolymerTest.clearBody();
        multideviceSubpage =
            document.createElement('settings-multidevice-subpage');
        multideviceSubpage.pageContentData = {hostDeviceName: 'Pixel XL'};
        setMode(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
        setSupportedFeatures(Object.values(MultiDeviceFeature));

        document.body.appendChild(multideviceSubpage);
        flush();

        assertEquals(
            undefined,
            multideviceSubpage.shadowRoot.querySelector('#smartLockItem')
                .subpageRoute);
        const routeBefore = Router.getInstance().currentRoute;
        multideviceSubpage.shadowRoot.querySelector('#smartLockItem')
            .shadowRoot.querySelector('.link-wrapper')
            .click();
        assertEquals(Router.getInstance().currentRoute, routeBefore);

        loadTimeData.overrideValues({'isSmartLockSignInRemoved': false});
      });

  test('AndroidMessages item shows button when not set up', function() {
    setAndroidSmsPairingComplete(false);
    flush();

    const controllerSelector = '#messagesItem > [slot=feature-controller]';
    assertTrue(
        !!multideviceSubpage.shadowRoot.querySelector(controllerSelector));
    assertTrue(multideviceSubpage.shadowRoot.querySelector(controllerSelector)
                   .tagName.includes('BUTTON'));

    setAndroidSmsPairingComplete(true);
    flush();

    assertFalse(
        !!multideviceSubpage.shadowRoot.querySelector(controllerSelector));
  });

  test(
      'AndroidMessages set up button calls browser proxy function',
      async function() {
        setAndroidSmsPairingComplete(false);
        flush();

        const setUpButton = multideviceSubpage.shadowRoot.querySelector(
            '#messagesItem > [slot=feature-controller]');
        assertTrue(!!setUpButton);

        setUpButton.click();

        await browserProxy.whenCalled('setUpAndroidSms');
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

        let setUpButton = multideviceSubpage.shadowRoot.querySelector(
            '#messagesItem > [slot=feature-controller]');
        assertFalse(!!setUpButton);

        // Verify that setup button is not disabled when feature is enabled.
        setAndroidSmsPairingComplete(false);
        setUpButton = multideviceSubpage.shadowRoot.querySelector(
            '#messagesItem > [slot=feature-controller]');
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

    const deepLinkElement = multideviceSubpage.shadowRoot.querySelector(
        '#messagesItem > [slot=feature-controller]');
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

    const deepLinkElement =
        multideviceSubpage.shadowRoot.querySelector('#messagesItem')
            .shadowRoot.querySelector('settings-multidevice-feature-toggle')
            .shadowRoot.querySelector('cr-toggle');
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

    const deepLinkElement =
        multideviceSubpage.shadowRoot.querySelector('#phoneHubItem')
            .shadowRoot.querySelector('settings-multidevice-feature-toggle')
            .shadowRoot.querySelector('cr-toggle');
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

    const deepLinkElement =
        multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem')
            .shadowRoot.querySelector('settings-multidevice-feature-toggle')
            .shadowRoot.querySelector('cr-toggle');
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
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              isPhoneHubPermissionsDialogSupported: false,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              isPhoneHubPermissionsDialogSupported: false,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              phoneHubAppsState: MultiDeviceFeatureState.ENABLED_BY_USER,
              isPhoneHubPermissionsDialogSupported: true,
              appsAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        const controllerSelector =
            '#phoneHubAppsItem > [slot=feature-controller]';
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector(controllerSelector));
        assertTrue(
            multideviceSubpage.shadowRoot.querySelector(controllerSelector)
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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              phoneHubCameraRollState: MultiDeviceFeatureState.ENABLED_BY_USER,
              cameraRollAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
              phoneHubNotificationsState:
                  MultiDeviceFeatureState.ENABLED_BY_USER,
              notificationAccessStatus:
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
              appsAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));
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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
                  PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertFalse(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
              appsAccessStatus: PhoneHubFeatureAccessStatus.PROHIBITED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));

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
              appsAccessStatus: PhoneHubFeatureAccessStatus.PROHIBITED,
            });

        flush();

        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(
            !!multideviceSubpage.shadowRoot.querySelector('#phoneHubAppsItem'));
        assertFalse(!!multideviceSubpage.shadowRoot.querySelector(
            '#phoneHubCombinedSetupItem'));
      });
});
