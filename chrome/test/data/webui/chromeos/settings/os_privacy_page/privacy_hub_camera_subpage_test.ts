// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {MediaDevicesProxy, PrivacyHubBrowserProxyImpl, SettingsPrivacyHubCameraSubpage} from 'chrome://os-settings/lazy_load.js';
import {appPermissionHandlerMojom, CrLinkRowElement, CrToggleElement, PrivacyHubSensorSubpageUserAction, Router, setAppPermissionProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeMediaDevices} from '../fake_media_devices.js';
import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {FakeAppPermissionHandler} from './fake_app_permission_handler.js';
import {createApp, createFakeMetricsPrivate, getSystemServicePermissionText, getSystemServicesFromSubpage} from './privacy_hub_app_permission_test_util.js';
import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

type App = appPermissionHandlerMojom.App;

suite('<settings-privacy-hub-camera-subpage>', () => {
  let fakeHandler: FakeAppPermissionHandler;
  let metrics: FakeMetricsPrivate;
  let privacyHubCameraSubpage: SettingsPrivacyHubCameraSubpage;
  let privacyHubBrowserProxy: TestPrivacyHubBrowserProxy;
  let mediaDevices: FakeMediaDevices;

  setup(() => {
    fakeHandler = new FakeAppPermissionHandler();
    setAppPermissionProviderForTesting(fakeHandler);

    metrics = createFakeMetricsPrivate();

    privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);

    mediaDevices = new FakeMediaDevices();
    MediaDevicesProxy.setMediaDevicesForTesting(mediaDevices);

    privacyHubCameraSubpage =
        document.createElement('settings-privacy-hub-camera-subpage');
    const prefs = {
      'ash': {
        'user': {
          'camera_allowed': {
            value: true,
          },
        },
      },
    };
    privacyHubCameraSubpage.prefs = prefs;
    document.body.appendChild(privacyHubCameraSubpage);
    flush();
  });

  teardown(() => {
    privacyHubCameraSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function getCameraCrToggle(): CrToggleElement {
    const crToggle =
        privacyHubCameraSubpage.shadowRoot!.querySelector<CrToggleElement>(
            '#cameraToggle');
    assertTrue(!!crToggle);
    return crToggle;
  }

  function getOnOffText(): string {
    return privacyHubCameraSubpage.shadowRoot!.querySelector('#onOffText')!
        .textContent!.trim();
  }

  function getOnOffSubtext(): string {
    return privacyHubCameraSubpage.shadowRoot!.querySelector('#onOffSubtext')!
        .textContent!.trim();
  }

  function isCameraListSectionVisible(): boolean {
    return isVisible(privacyHubCameraSubpage.shadowRoot!.querySelector(
        '#cameraListSection'));
  }

  function getNoCameraTextElement(): HTMLDivElement|null {
    return privacyHubCameraSubpage.shadowRoot!.querySelector('#noCameraText');
  }

  function getCameraList(): DomRepeat|null {
    return privacyHubCameraSubpage.shadowRoot!.querySelector<DomRepeat>(
        '#cameraList');
  }

  function isBlockedSuffixDisplayedAfterCameraName(): boolean {
    return isVisible(privacyHubCameraSubpage.shadowRoot!.querySelector(
        '#cameraNameWithBlockedSuffix'));
  }

  test('Camera section view when access is enabled', () => {
    const cameraToggle = getCameraCrToggle();

    assertTrue(cameraToggle.checked);
    assertEquals(privacyHubCameraSubpage.i18n('deviceOn'), getOnOffText());
    assertEquals(
        privacyHubCameraSubpage.i18n(
            'privacyHubCameraSubpageCameraToggleSubtext'),
        getOnOffSubtext());
    assertTrue(isCameraListSectionVisible());
    assertFalse(isBlockedSuffixDisplayedAfterCameraName());
  });

  test('Camera section view when access is disabled', async () => {
    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();

    const cameraToggle = getCameraCrToggle();

    // Disable camera access.
    cameraToggle.click();
    flush();

    assertFalse(cameraToggle.checked);
    assertEquals(privacyHubCameraSubpage.i18n('deviceOff'), getOnOffText());
    assertEquals(
        privacyHubCameraSubpage.i18n('privacyHubCameraAccessBlockedText'),
        getOnOffSubtext());
    assertTrue(isCameraListSectionVisible());
    assertTrue(isBlockedSuffixDisplayedAfterCameraName());
    assertEquals(
        privacyHubCameraSubpage.i18n(
            'privacyHubSensorNameWithBlockedSuffix', 'Fake Camera'),
        privacyHubCameraSubpage.shadowRoot!
            .querySelector<HTMLDivElement>(
                '#cameraNameWithBlockedSuffix')!.innerText.trim());
  });

  test('Repeatedly toggle camera access', async () => {
    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();

    const cameraToggle = getCameraCrToggle();

    for (let i = 0; i < 3; i++) {
      cameraToggle.click();
      flush();

      assertEquals(
          cameraToggle.checked,
          privacyHubCameraSubpage.prefs.ash.user.camera_allowed.value);
      assertEquals(
          i + 1,
          metrics.countMetricValue(
              'ChromeOS.PrivacyHub.CameraSubpage.UserAction',
              PrivacyHubSensorSubpageUserAction.SYSTEM_ACCESS_CHANGED));
    }
  });

  function isCameraRowActionable(): boolean {
    const actionableAttribute =
        privacyHubCameraSubpage.shadowRoot!.querySelector('#accessStatusRow')!
            .getAttribute('actionable');
    return actionableAttribute === '';
  }

  test(
      'Clicking toggle is no-op when accessStatusRow is not actionable',
      async () => {
        const cameraToggle = getCameraCrToggle();
        assertTrue(cameraToggle.checked);

        assertFalse(isCameraRowActionable());
        cameraToggle.click();
        assertTrue(cameraToggle.checked);

        // Add a camera to make accessStatusRow actionable.
        mediaDevices.addDevice('videoinput', 'Fake Camera');
        await flushTasks();
        assertTrue(isCameraRowActionable());
        cameraToggle.click();
        assertFalse(cameraToggle.checked);
      });

  test('No camera connected and toggle disabled by default', () => {
    assertTrue(getCameraCrToggle().disabled);
    assertNull(getCameraList());
    assertTrue(!!getNoCameraTextElement());
    assertEquals(
        privacyHubCameraSubpage.i18n('noCameraConnectedText'),
        getNoCameraTextElement()!.textContent!.trim());
  });

  test('Change force-disable-camera-switch', async () => {
    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();

    assertFalse(getCameraCrToggle().disabled);

    webUIListenerCallback('force-disable-camera-switch', true);
    await flushTasks();

    assertTrue(getCameraCrToggle().disabled);

    webUIListenerCallback('force-disable-camera-switch', false);
    await flushTasks();

    assertFalse(getCameraCrToggle().disabled);
  });

  test('Toggle enabled when at least one camera connected', async () => {
    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();

    assertFalse(getCameraCrToggle().disabled);
    assertNull(getNoCameraTextElement());
  });

  test('Camera list updated when a camera is added or removed', async () => {
    const testDevices = [
      {
        device: {
          kind: 'audiooutput',
          label: 'Fake Speaker 1',
        },
      },
      {
        device: {
          kind: 'videoinput',
          label: 'Fake Camera 1',
        },
      },
      {
        device: {
          kind: 'audioinput',
          label: 'Fake Microphone 1',
        },
      },
      {
        device: {
          kind: 'videoinput',
          label: 'Fake Camera 2',
        },
      },
      {
        device: {
          kind: 'audiooutput',
          label: 'Fake Speaker 2',
        },
      },
      {
        device: {
          kind: 'audioinput',
          label: 'Fake Microphone 2',
        },
      },
    ];

    let cameraCount = 0;

    // Adding a media device in each iteration.
    for (const test of testDevices) {
      mediaDevices.addDevice(test.device.kind, test.device.label);
      await flushTasks();

      if (test.device.kind === 'videoinput') {
        cameraCount++;
      }

      const cameraList = getCameraList();
      if (cameraCount) {
        assertTrue(!!cameraList);
        assertEquals(cameraCount, cameraList.items!.length);
      } else {
        assertNull(cameraList);
      }
    }

    // Removing the most recently added media device in each iteration.
    for (const test of testDevices.reverse()) {
      mediaDevices.popDevice();
      await flushTasks();

      if (test.device.kind === 'videoinput') {
        cameraCount--;
      }

      const cameraList = getCameraList();
      if (cameraCount) {
        assertTrue(!!cameraList);
        assertEquals(cameraCount, cameraList.items!.length);
      } else {
        assertNull(cameraList);
      }
    }
  });

  function getNoAppHasAccessTextSection(): HTMLDivElement|null {
    return privacyHubCameraSubpage.shadowRoot!.querySelector(
        '#noAppHasAccessText');
  }

  function getAppList(): DomRepeat|null {
    return privacyHubCameraSubpage.shadowRoot!.querySelector('#appList');
  }

  test('App list displayed when camera allowed', () => {
    assertEquals(
        privacyHubCameraSubpage.i18n('privacyHubAppsSectionTitle'),
        privacyHubCameraSubpage.shadowRoot!.querySelector('#appsSectionTitle')!
            .textContent!.trim());
    assertTrue(!!getAppList());
    assertNull(getNoAppHasAccessTextSection());
  });

  test('App list not displayed when camera not allowed', async () => {
    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();

    // Disable camera access.
    getCameraCrToggle().click();
    flush();

    assertNull(getAppList());
    assertTrue(!!getNoAppHasAccessTextSection());
    assertEquals(
        privacyHubCameraSubpage.i18n('noAppCanUseCameraText'),
        getNoAppHasAccessTextSection()!.textContent!.trim());
  });

  function initializeObserver(): Promise<void> {
    return fakeHandler.whenCalled('addObserver');
  }

  function simulateAppUpdate(app: App): void {
    fakeHandler.getObserverRemote().onAppUpdated(app);
  }

  function simulateAppRemoval(id: string): void {
    fakeHandler.getObserverRemote().onAppRemoved(id);
  }

  test('AppList displays all apps with camera permission', async () => {
    const app1 = createApp(
        'app1_id', 'app1_name', PermissionType.kCamera, TriState.kAllow);
    const app2 = createApp(
        'app2_id', 'app2_name', PermissionType.kMicrophone, TriState.kAllow);
    const app3 = createApp(
        'app3_id', 'app3_name', PermissionType.kCamera, TriState.kAsk);

    await initializeObserver();
    simulateAppUpdate(app1);
    simulateAppUpdate(app2);
    simulateAppUpdate(app3);
    await flushTasks();

    assertEquals(2, getAppList()!.items!.length);
  });

  test('Removed app are removed from appList', async () => {
    const app1 = createApp(
        'app1_id', 'app1_name', PermissionType.kCamera, TriState.kAllow);
    const app2 = createApp(
        'app2_id', 'app2_name', PermissionType.kMicrophone, TriState.kAllow);

    await initializeObserver();
    simulateAppUpdate(app1);
    simulateAppUpdate(app2);
    await flushTasks();

    assertEquals(1, getAppList()!.items!.length);

    simulateAppRemoval(app2.id);
    await flushTasks();

    assertEquals(1, getAppList()!.items!.length);

    simulateAppRemoval(app1.id);
    await flushTasks();

    assertEquals(0, getAppList()!.items!.length);
  });

  function getManagePermissionsInChromeRow(): CrLinkRowElement|null {
    return privacyHubCameraSubpage.shadowRoot!.querySelector<CrLinkRowElement>(
        '#managePermissionsInChromeRow');
  }

  function getNoWebsiteHasAccessTextRow(): HTMLDivElement|null {
    return privacyHubCameraSubpage.shadowRoot!.querySelector<HTMLDivElement>(
        '#noWebsiteHasAccessText');
  }

  test('Websites section texts', async () => {
    assertEquals(
        privacyHubCameraSubpage.i18n('websitesSectionTitle'),
        privacyHubCameraSubpage.shadowRoot!
            .querySelector('#websitesSectionTitle')!.textContent!.trim());

    assertEquals(
        privacyHubCameraSubpage.i18n('manageCameraPermissionsInChromeText'),
        getManagePermissionsInChromeRow()!.label);

    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();
    // Disable camera access.
    getCameraCrToggle().click();
    flush();

    assertEquals(
        privacyHubCameraSubpage.i18n('noWebsiteCanUseCameraText'),
        getNoWebsiteHasAccessTextRow()!.textContent!.trim());
  });

  test('Websites section when camera allowed', () => {
    assertTrue(!!getManagePermissionsInChromeRow());
    assertNull(getNoWebsiteHasAccessTextRow());
  });

  test('Websites section when camera not allowed', async () => {
    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();
    // Disable camera access.
    getCameraCrToggle().click();
    flush();

    assertNull(getManagePermissionsInChromeRow());
    assertTrue(!!getNoWebsiteHasAccessTextRow());
  });

  test('Website section metric recorded when clicked', () => {
    assertEquals(
        0,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.CameraSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.WEBSITE_PERMISSION_LINK_CLICKED));

    getManagePermissionsInChromeRow()!.click();

    assertEquals(
        1,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.CameraSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.WEBSITE_PERMISSION_LINK_CLICKED));
  });

  test(
      'Clicking Chrome row opens Chrome browser camera permission settings',
      async () => {
        assertEquals(
            PermissionType.kUnknown,
            fakeHandler.getLastOpenedBrowserPermissionSettingsType());

        getManagePermissionsInChromeRow()!.click();
        await fakeHandler.whenCalled('openBrowserPermissionSettings');

        assertEquals(
            PermissionType.kCamera,
            fakeHandler.getLastOpenedBrowserPermissionSettingsType());
      });

  test('System services section when camera is allowed', async () => {
    assertEquals(
        privacyHubCameraSubpage.i18n('privacyHubSystemServicesSectionTitle'),
        privacyHubCameraSubpage.shadowRoot!
            .querySelector('#systemServicesSectionTitle')!.textContent!.trim());

    await flushTasks();
    const systemServices =
        getSystemServicesFromSubpage(privacyHubCameraSubpage);
    assertEquals(1, systemServices.length);
    assertEquals(
        privacyHubCameraSubpage.i18n('privacyHubSystemServicesAllowedText'),
        getSystemServicePermissionText(systemServices[0]!));
  });

  test('System services section when camera is not allowed', async () => {
    mediaDevices.addDevice('videoinput', 'Fake Camera');
    await flushTasks();
    // Toggle camera access.
    getCameraCrToggle().click();
    flush();

    const systemServices =
        getSystemServicesFromSubpage(privacyHubCameraSubpage);
    assertEquals(1, systemServices.length);
    assertEquals(
        privacyHubCameraSubpage.i18n('privacyHubSystemServicesBlockedText'),
        getSystemServicePermissionText(systemServices[0]!));
  });
});
