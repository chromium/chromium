// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {MediaDevicesProxy, PrivacyHubBrowserProxyImpl, SettingsPrivacyHubCameraSubpage} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, Router} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeMediaDevices} from '../fake_media_devices.js';

import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

suite('<settings-privacy-hub-camera-subpage>', () => {
  let privacyHubCameraSubpage: SettingsPrivacyHubCameraSubpage;
  let privacyHubBrowserProxy: TestPrivacyHubBrowserProxy;
  let mediaDevices: FakeMediaDevices;

  setup(() => {
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

  test('Camera section view when access is enabled', () => {
    const cameraToggle = getCameraCrToggle();

    assertTrue(cameraToggle.checked);
    assertEquals(privacyHubCameraSubpage.i18n('deviceOn'), getOnOffText());
    assertEquals(
        privacyHubCameraSubpage.i18n('cameraToggleSubtext'), getOnOffSubtext());
    assertTrue(isCameraListSectionVisible());
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
        privacyHubCameraSubpage.i18n('blockedForAllText'), getOnOffSubtext());
    assertFalse(isCameraListSectionVisible());
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
    }
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
});
