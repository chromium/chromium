// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {MediaPickerElement, MediaPickerEntry} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

suite('MediaPicker', function() {
  let mediaPicker: MediaPickerElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  const testDevices: MediaPickerEntry[] = [
    {name: 'Device 1', id: 'device_id_1'},
    {name: 'Device 2', id: 'device_id_2'},
    {name: 'Device 3', id: 'device_id_3'},
  ];

  setup(async function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mediaPicker = document.createElement('media-picker');
    mediaPicker.type = 'camera';
    mediaPicker.label = 'Camera Picker';
    document.body.appendChild(mediaPicker);

    const type = await browserProxy.whenCalled('initializeCaptureDevices');
    assertEquals('camera', type);
    await microtasksFinished();
  });

  test('hidden when no devices available', function() {
    assertFalse(isVisible(mediaPicker.$.picker));
  });

  test('ignores updateDevicesMenu for mismatched type', async function() {
    webUIListenerCallback(
        'updateDevicesMenu', 'mic', testDevices, 'device_id_1');
    await microtasksFinished();
    assertFalse(isVisible(mediaPicker.$.picker));
    assertEquals(0, mediaPicker.devices.length);
  });

  test('populates devices and selects default device', async function() {
    webUIListenerCallback(
        'updateDevicesMenu', 'camera', testDevices, 'device_id_2');
    await microtasksFinished();

    assertTrue(isVisible(mediaPicker.$.picker));
    assertEquals(3, mediaPicker.devices.length);

    const select = mediaPicker.$.mediaPicker;
    assertEquals(3, select.options.length);
    assertEquals('Device 1', select.options[0]!.text.trim());
    assertEquals('device_id_1', select.options[0]!.value);
    assertEquals('Device 2', select.options[1]!.text.trim());
    assertEquals('device_id_2', select.options[1]!.value);

    // Verify option 1 (device_id_2) is selected and select value matches.
    assertTrue(select.options[1]!.selected);
    assertEquals('device_id_2', select.value);
  });

  test('updates selection on user change', async function() {
    webUIListenerCallback(
        'updateDevicesMenu', 'camera', testDevices, 'device_id_1');
    await microtasksFinished();

    const select = mediaPicker.$.mediaPicker;
    assertEquals('device_id_1', select.value);

    browserProxy.resetResolver('setPreferredCaptureDevice');
    select.value = 'device_id_3';
    select.dispatchEvent(new CustomEvent('change'));
    await microtasksFinished();

    const [type, defaultDevice] =
        await browserProxy.whenCalled('setPreferredCaptureDevice');
    assertEquals('camera', type);
    assertEquals('device_id_3', defaultDevice);
    assertEquals('device_id_3', select.value);
    assertTrue(select.options[2]!.selected);
  });

  test(
      'updates selection when updateDevicesMenu sends new device',
      async function() {
        webUIListenerCallback(
            'updateDevicesMenu', 'camera', testDevices, 'device_id_1');
        await microtasksFinished();

        const select = mediaPicker.$.mediaPicker;
        assertEquals('device_id_1', select.value);
        assertTrue(select.options[0]!.selected);

        // Update with a different selected device.
        webUIListenerCallback(
            'updateDevicesMenu', 'camera', testDevices, 'device_id_3');
        await microtasksFinished();

        assertEquals('device_id_3', select.value);
        assertTrue(select.options[2]!.selected);
        assertFalse(select.options[0]!.selected);
      });
});
