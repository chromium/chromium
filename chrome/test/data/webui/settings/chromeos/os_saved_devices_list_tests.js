// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';
import 'chrome://os-settings/strings.m.js';

import {OsBluetoothDevicesSubpageBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createDefaultBluetoothDevice} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';


suite('OsSavedDevicesListTest', function() {
  /** @type {!SettingsSavedDevicesListElement|undefined} */
  let savedDevicesList;

  /** @type {?OsBluetoothDevicesSubpageBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);
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

    assertEquals(savedDevicesList.devices.length, 0);

    savedDevicesList.devices = [device0, device1, device2, device3];
    await flushAsync();

    assertEquals(savedDevicesList.devices.length, 4);

    const ironResizePromise = eventToPromise('iron-resize', savedDevicesList);

    const listItem = getListItems()[1];
    listItem.shadowRoot.querySelector('#dotsMenu').click();
    await flushAsync();
    listItem.shadowRoot.querySelector('#removeButton').click();
    await flushAsync();
    const removeDialog =
        listItem.shadowRoot.querySelector('#removeDeviceDialog');
    removeDialog.shadowRoot.querySelector('#remove').click();

    await ironResizePromise;
    await flushAsync();

    assertEquals(savedDevicesList.devices[0].accountKey, '0');
    assertEquals(savedDevicesList.devices[1].accountKey, '2');
    assertEquals(savedDevicesList.devices[2].accountKey, '3');
    assertEquals(savedDevicesList.devices.length, 3);
  });

  test('Device list change renders items correctly', async function() {
    const device0 = {name: 'dev0'};
    const device1 = {name: 'dev1'};
    const device2 = {name: 'dev2'};

    savedDevicesList.devices = [device0, device1, device2];
    await flushAsync();

    assertEquals(getListItems().length, 3);

    const ironResizePromise = eventToPromise('iron-resize', savedDevicesList);
    savedDevicesList.devices = [device0, device1, device2, device1, device2];

    await ironResizePromise;
    await flushAsync();
    assertEquals(getListItems().length, 5);
  });

  test('Device list has correct a11y labels', async function() {
    const device0 = {name: 'dev0', imageUrl: '', accountKey: '0'};
    const device1 = {name: 'dev1', imageUrl: '', accountKey: '1'};
    const device2 = {name: 'dev2', imageUrl: '', accountKey: '2'};

    const getListItems = () => {
      return savedDevicesList.shadowRoot.querySelectorAll(
          'os-settings-saved-devices-list-item');
    };
    assertEquals(savedDevicesList.devices.length, 0);

    savedDevicesList.devices = [device0, device1, device2];
    await flushAsync();

    assertEquals(savedDevicesList.devices.length, 3);

    const ironResizePromise = eventToPromise('iron-resize', savedDevicesList);

    assertEquals(
        getListItems()[0].shadowRoot.querySelector('.list-item').ariaLabel,
        savedDevicesList.i18n('savedDeviceItemA11yLabel', 1, 3, 'dev0'));
    assertEquals(
        getListItems()[1].shadowRoot.querySelector('.list-item').ariaLabel,
        savedDevicesList.i18n('savedDeviceItemA11yLabel', 2, 3, 'dev1'));
    assertEquals(
        getListItems()[2].shadowRoot.querySelector('.list-item').ariaLabel,
        savedDevicesList.i18n('savedDeviceItemA11yLabel', 3, 3, 'dev2'));
    assertEquals(
        getListItems()[0].shadowRoot.querySelector('.icon-more-vert').ariaLabel,
        savedDevicesList.i18n('savedDeviceItemButtonA11yLabel', 'dev0'));
    assertEquals(
        getListItems()[1].shadowRoot.querySelector('.icon-more-vert').ariaLabel,
        savedDevicesList.i18n('savedDeviceItemButtonA11yLabel', 'dev1'));
    assertEquals(
        getListItems()[2].shadowRoot.querySelector('.icon-more-vert').ariaLabel,
        savedDevicesList.i18n('savedDeviceItemButtonA11yLabel', 'dev2'));
  });

  test('Device images render correctly', async function() {
    const device0 = {name: 'dev0', imageUrl: '', accountKey: '0'};
    const device1 = {name: 'dev1', imageUrl: 'fakeUrl', accountKey: '1'};

    savedDevicesList.devices = [device0, device1];
    await flushAsync();

    assertTrue(isVisible(
        getListItems()[0].shadowRoot.querySelector('#noDeviceImage')));
    assertFalse(
        isVisible(getListItems()[0].shadowRoot.querySelector('#deviceImage')));
    assertFalse(isVisible(
        getListItems()[1].shadowRoot.querySelector('#noDeviceImage')));
    assertTrue(
        isVisible(getListItems()[1].shadowRoot.querySelector('#deviceImage')));
  });
});
