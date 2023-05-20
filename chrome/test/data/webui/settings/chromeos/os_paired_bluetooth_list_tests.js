// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/strings.m.js';

import {DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('OsPairedBluetoothListTest', function() {
  /** @type {!SettingsPairedBluetoothListElement|undefined} */
  let pairedBluetoothList;

  setup(function() {
    pairedBluetoothList =
        document.createElement('os-settings-paired-bluetooth-list');
    document.body.appendChild(pairedBluetoothList);
    flush();
  });

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Base Test', function() {
    const list = pairedBluetoothList.shadowRoot.querySelector('iron-list');
    assertTrue(!!list);
  });

  test('Device list change renders items correctly', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);

    pairedBluetoothList.devices = [device, device, device];
    await flushAsync();

    const getListItems = () => {
      return pairedBluetoothList.shadowRoot.querySelectorAll(
          'os-settings-paired-bluetooth-list-item');
    };
    assertEquals(getListItems().length, 3);

    const ironResizePromise =
        eventToPromise('iron-resize', pairedBluetoothList);
    pairedBluetoothList.devices = [device, device, device, device, device];

    await ironResizePromise;
    flush();
    assertEquals(getListItems().length, 5);
  });

  test('Tooltip is shown', async function() {
    const getTooltip = () => {
      return pairedBluetoothList.shadowRoot.querySelector('#tooltip');
    };

    assertFalse(getTooltip()._showing);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);

    pairedBluetoothList.devices = [device];
    await flushAsync();

    const listItem = pairedBluetoothList.shadowRoot.querySelector(
        'os-settings-paired-bluetooth-list-item');

    listItem.dispatchEvent(new CustomEvent('managed-tooltip-state-change', {
      bubbles: true,
      composed: true,
      detail: {
        address: 'device-address',
        show: true,
        element: document.createElement('div'),
      },
    }));

    await flushAsync();
    assertTrue(getTooltip()._showing);

    listItem.dispatchEvent(new CustomEvent('managed-tooltip-state-change', {
      bubbles: true,
      composed: true,
      detail: {
        address: 'device-address',
        show: false,
        element: document.createElement('div'),
      },
    }));
    await flushAsync();
    assertFalse(getTooltip()._showing);
  });
});
