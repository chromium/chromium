// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';
import 'chrome://os-settings/strings.m.js';

import {DeviceConnectionState} from 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createDefaultBluetoothDevice} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {eventToPromise} from 'chrome://test/test_util.js';

import {assertEquals, assertTrue} from '../../../chai_assert.js';

suite('OsSavedDevicesListTest', function() {
  /** @type {!SettingsSavedDevicesListElement|undefined} */
  let savedDevicesList;

  setup(function() {
    savedDevicesList = document.createElement('os-settings-saved-devices-list');
    document.body.appendChild(savedDevicesList);
    flush();
  });

  function getListItems() {
    return savedDevicesList.shadowRoot.querySelectorAll(
        'os-settings-saved-devices-list-item');
  }

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Base Test', function() {
    const list = savedDevicesList.shadowRoot.querySelector('iron-list');
    assertTrue(!!list);
  });

  test('Device list change removes items correctly', async function() {
    const device0 = {name: 'dev0', imageUrl: '', accountKey: '0'};
    const device1 = {name: 'dev1', imageUrl: '', accountKey: '1'};
    const device2 = {name: 'dev2', imageUrl: '', accountKey: '2'};
    const device3 = {name: 'dev3', imageUrl: '', accountKey: '3'};

    assertEquals(savedDevicesList.devices_.length, 0);

    savedDevicesList.devices_ = [device0, device1, device2, device3];
    await flushAsync();

    assertEquals(savedDevicesList.devices_.length, 4);

    const ironResizePromise = eventToPromise('iron-resize', savedDevicesList);

    const listItem = getListItems()[1];
    listItem.$$('#dotsMenu').click();
    listItem.$$('#removeButton').click();

    await ironResizePromise;
    await flushAsync();

    assertEquals(savedDevicesList.devices_[0].accountKey, '0');
    assertEquals(savedDevicesList.devices_[1].accountKey, '2');
    assertEquals(savedDevicesList.devices_[2].accountKey, '3');
    assertEquals(savedDevicesList.devices_.length, 3);
  });

  test('Device list change renders items correctly', async function() {
    const device0 = {name: 'dev0'};
    const device1 = {name: 'dev1'};
    const device2 = {name: 'dev2'};

    savedDevicesList.devices_ = [device0, device1, device2];
    await flushAsync();

    assertEquals(getListItems().length, 3);

    const ironResizePromise = eventToPromise('iron-resize', savedDevicesList);
    savedDevicesList.devices_ = [device0, device1, device2, device1, device2];

    await ironResizePromise;
    flush();
    assertEquals(getListItems().length, 5);
  });
});
