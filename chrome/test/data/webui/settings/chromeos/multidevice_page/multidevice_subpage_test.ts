// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsMultideviceFeatureItemElement, SettingsMultideviceSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessStatus, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('<settings-multidevice-subpage>', () => {
  let multideviceSubpage: SettingsMultideviceSubpageElement;
  let browserProxy: TestMultideviceBrowserProxy;
  // Although HOST_SET_MODES is effectively a constant, it cannot reference the
  // enum MultiDeviceSettingsMode from here so its initialization is
  // deferred to the suiteSetup function.
  let HOST_SET_MODES: MultiDeviceSettingsMode[];

  /**
   * Observably sets mode. Everything else remains unchanged.
   */
  function setMode(newMode: MultiDeviceSettingsMode): void {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          mode: newMode,
        });
    flush();
  }

  /**
   * Observably resets feature states so that each feature is supported if and
   * only if it is in the provided array. Everything else remains unchanged.
   */
  function setSupportedFeatures(supportedFeatures: MultiDeviceFeature[]): void {
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

  suiteSetup(() => {
    HOST_SET_MODES = [
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  setup(() => {
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

    multideviceSubpage = document.createElement('settings-multidevice-subpage');
    assertTrue(!!multideviceSubpage);
    multideviceSubpage.pageContentData = {
      ...multideviceSubpage.pageContentData,
      hostDeviceName: 'Pixel XL',
    };

    setMode(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSupportedFeatures(
        Object.values(MultiDeviceFeature) as MultiDeviceFeature[]);

    document.body.appendChild(multideviceSubpage);
    flush();
  });

  teardown(() => {
    multideviceSubpage.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('individual features appear only if host is verified', () => {
    for (const mode of HOST_SET_MODES) {
      setMode(mode);
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector('#smartLockItem'));
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector(
              '#instantTetheringItem'));
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector(
              '#phoneHubNotificationsItem'));
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector(
              '#phoneHubTaskContinuationItem'));
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector('#wifiSyncItem'));
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
      assertEquals(
          mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED,
          !!multideviceSubpage.shadowRoot!.querySelector(
              '#phoneHubCameraRollItem'));
    }
  });

  test('individual features are attached only if they are supported', () => {
    assertTrue(
        !!multideviceSubpage.shadowRoot!.querySelector('#smartLockItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#instantTetheringItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubNotificationsItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubTaskContinuationItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector('#wifiSyncItem'));
    assertTrue(
        !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubCameraRollItem'));

    setSupportedFeatures([
      MultiDeviceFeature.SMART_LOCK,
      MultiDeviceFeature.PHONE_HUB,
      MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
      MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION,
      MultiDeviceFeature.WIFI_SYNC,
      MultiDeviceFeature.ECHE,
      MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL,
    ]);
    assertTrue(
        !!multideviceSubpage.shadowRoot!.querySelector('#smartLockItem'));
    assertNull(
        multideviceSubpage.shadowRoot!.querySelector('#instantTetheringItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubNotificationsItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubTaskContinuationItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector('#wifiSyncItem'));
    assertTrue(
        !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubCameraRollItem'));

    setSupportedFeatures([MultiDeviceFeature.INSTANT_TETHERING]);
    assertNull(multideviceSubpage.shadowRoot!.querySelector('#smartLockItem'));
    assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
        '#instantTetheringItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubNotificationsItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubTaskContinuationItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector('#wifiSyncItem'));
    assertNull(
        multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubCameraRollItem'));

    setSupportedFeatures([]);
    assertNull(multideviceSubpage.shadowRoot!.querySelector('#smartLockItem'));
    assertNull(
        multideviceSubpage.shadowRoot!.querySelector('#instantTetheringItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubNotificationsItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubTaskContinuationItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector('#wifiSyncItem'));
    assertNull(
        multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
    assertNull(multideviceSubpage.shadowRoot!.querySelector(
        '#phoneHubCameraRollItem'));
  });

  test(
      'setting isSmartLockSignInRemoved flag removes SmartLock subpage route',
      () => {
        multideviceSubpage.remove();
        loadTimeData.overrideValues({'isSmartLockSignInRemoved': true});
        browserProxy = new TestMultideviceBrowserProxy();
        MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

        multideviceSubpage =
            document.createElement('settings-multidevice-subpage');
        multideviceSubpage.pageContentData = {
          ...multideviceSubpage.pageContentData,
          hostDeviceName: 'Pixel XL',
        };
        setMode(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
        setSupportedFeatures(
            Object.values(MultiDeviceFeature) as MultiDeviceFeature[]);

        document.body.appendChild(multideviceSubpage);
        flush();

        const smartLockItem =
            multideviceSubpage.shadowRoot!
                .querySelector<SettingsMultideviceFeatureItemElement>(
                    '#smartLockItem');
        assertTrue(!!smartLockItem);
        assertEquals(undefined, smartLockItem.subpageRoute);
        const routeBefore = Router.getInstance().currentRoute;
        const linkWrapper =
            smartLockItem.shadowRoot!.querySelector<HTMLElement>(
                '.link-wrapper');
        assertTrue(!!linkWrapper);
        linkWrapper.click();
        assertEquals(routeBefore, Router.getInstance().currentRoute);

        loadTimeData.overrideValues({'isSmartLockSignInRemoved': false});
      });

  test('Deep link to phone hub on/off', async () => {
    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kPhoneHubOnOff.toString());
    Router.getInstance().navigateTo(routes.MULTIDEVICE_FEATURES, params);

    flush();

    const phoneHubItem =
        multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem');
    assertTrue(!!phoneHubItem);
    const featureToggle = phoneHubItem.shadowRoot!.querySelector(
        'settings-multidevice-feature-toggle');
    assertTrue(!!featureToggle);
    const deepLinkElement =
        featureToggle.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
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
    params.append(
        'settingId', settingMojom.Setting.kPhoneHubAppsOnOff.toString());
    Router.getInstance().navigateTo(routes.MULTIDEVICE_FEATURES, params);

    flush();

    const phoneHubAppsItem =
        multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem');
    assertTrue(!!phoneHubAppsItem);
    const featureToggle = phoneHubAppsItem.shadowRoot!.querySelector(
        'settings-multidevice-feature-toggle');
    assertTrue(!!featureToggle);
    const deepLinkElement =
        featureToggle.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Phone hub apps on/off toggle should be focused for settingId=218.');
  });

  test(
      'Phone Hub Camera Roll, Notifications, Apps and Combined items are shown/hidden correctly',
      () => {
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        const controllerSelector =
            '#phoneHubAppsItem > [slot=feature-controller]';
        const selector =
            multideviceSubpage.shadowRoot!.querySelector(controllerSelector);
        assertTrue(!!selector);
        assertTrue(selector.tagName.includes('BUTTON'));

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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCombinedSetupItem'));
      });

  test(
      'Enterprise policies should properly affect Phone Hub Camera Roll, Notifications, Apps, and Combined items.',
      () => {
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertNull(
            multideviceSubpage.shadowRoot!.querySelector('#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
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
            !!multideviceSubpage.shadowRoot!.querySelector('#phoneHubItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCameraRollItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubNotificationsItem'));
        assertTrue(!!multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubAppsItem'));
        assertNull(multideviceSubpage.shadowRoot!.querySelector(
            '#phoneHubCombinedSetupItem'));
      });
});
