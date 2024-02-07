// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsSavedDevicesListElement} from 'chrome://os-settings/lazy_load.js';
import {OsBluetoothDevicesSubpageBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('<os-settings-saved-devices-list>', () => {
  let savedDevicesList: SettingsSavedDevicesListElement;
  let browserProxy: TestOsBluetoothDevicesSubpageBrowserProxy;

  suiteSetup(() => {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);
  });

  setup(() => {
    savedDevicesList = document.createElement('os-settings-saved-devices-list');
    document.body.appendChild(savedDevicesList);
    flush();
  });

  teardown(() => {
    savedDevicesList.remove();
    browserProxy.reset();
  });

  function getListItems(): NodeListOf<Element> {
    return savedDevicesList.shadowRoot!.querySelectorAll(
        'os-settings-saved-devices-list-item');
  }

  test('Base Test', () => {
    const list = savedDevicesList.shadowRoot!.querySelector('iron-list');
    assertTrue(!!list);
  });

  test('Device list change removes items correctly', async () => {
    const device0 = {name: 'dev0', imageUrl: '', accountKey: '0'};
    const device1 = {name: 'dev1', imageUrl: '', accountKey: '1'};
    const device2 = {name: 'dev2', imageUrl: '', accountKey: '2'};
    const device3 = {name: 'dev3', imageUrl: '', accountKey: '3'};

    assertEquals(0, savedDevicesList.get('devices').length);

    savedDevicesList.set('devices', [device0, device1, device2, device3]);
    await flushTasks();

    assertEquals(4, savedDevicesList.get('devices').length);

    const ironResizePromise = eventToPromise('iron-resize', savedDevicesList);

    const listItem = getListItems()[1];
    assertTrue(!!listItem);
    const menuButton =
        listItem.shadowRoot!.querySelector<HTMLButtonElement>('#dotsMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    await flushTasks();

    const listRemoveButton =
        listItem.shadowRoot!.querySelector<HTMLButtonElement>('#removeButton');
    assertTrue(!!listRemoveButton);
    listRemoveButton.click();
    await flushTasks();

    const removeDialog =
        listItem.shadowRoot!.querySelector('#removeDeviceDialog');
    assertTrue(!!removeDialog);
    const dialogRemoveButton =
        removeDialog.shadowRoot!.querySelector<HTMLButtonElement>('#remove');
    assertTrue(!!dialogRemoveButton);
    dialogRemoveButton.click();

    await ironResizePromise;
    await flushTasks();

    assertEquals('0', savedDevicesList.devices[0]!.accountKey);
    assertEquals('2', savedDevicesList.devices[1]!.accountKey);
    assertEquals('3', savedDevicesList.devices[2]!.accountKey);
    assertEquals(3, savedDevicesList.get('devices').length);
  });

  test('Device list change renders items correctly', async () => {
    const device0 = {name: 'dev0', imageUrl: '', accountKey: '0'};
    const device1 = {name: 'dev1', imageUrl: '', accountKey: '1'};
    const device2 = {name: 'dev2', imageUrl: '', accountKey: '2'};

    savedDevicesList.set('devices', [device0, device1, device2]);
    await flushTasks();

    assertEquals(3, getListItems().length);

    const ironResizePromise = eventToPromise('iron-resize', savedDevicesList);
    savedDevicesList.set(
        'devices', [device0, device1, device2, device1, device2]);

    await ironResizePromise;
    await flushTasks();
    assertEquals(5, getListItems().length);
  });

  test('Device list has correct a11y labels', async () => {
    const device0 = {name: 'dev0', imageUrl: '', accountKey: '0'};
    const device1 = {name: 'dev1', imageUrl: '', accountKey: '1'};
    const device2 = {name: 'dev2', imageUrl: '', accountKey: '2'};

    assertEquals(0, savedDevicesList.get('devices').length);

    savedDevicesList.set('devices', [device0, device1, device2]);
    await flushTasks();

    assertEquals(3, savedDevicesList.get('devices').length);

    const listItem0 =
        getListItems()[0]!.shadowRoot!.querySelector('.list-item');
    const listItem1 =
        getListItems()[1]!.shadowRoot!.querySelector('.list-item');
    const listItem2 =
        getListItems()[2]!.shadowRoot!.querySelector('.list-item');
    assertTrue(!!listItem0);
    assertTrue(!!listItem1);
    assertTrue(!!listItem2);

    const iconMoreVert0 =
        getListItems()[0]!.shadowRoot!.querySelector('.icon-more-vert');
    const iconMoreVert1 =
        getListItems()[1]!.shadowRoot!.querySelector('.icon-more-vert');
    const iconMoreVert2 =
        getListItems()[2]!.shadowRoot!.querySelector('.icon-more-vert');
    assertTrue(!!iconMoreVert0);
    assertTrue(!!iconMoreVert1);
    assertTrue(!!iconMoreVert2);

    assertEquals(
        savedDevicesList.i18n('savedDeviceItemA11yLabel', 1, 3, 'dev0'),
        listItem0.ariaLabel);
    assertEquals(
        savedDevicesList.i18n('savedDeviceItemA11yLabel', 2, 3, 'dev1'),
        listItem1.ariaLabel);
    assertEquals(
        savedDevicesList.i18n('savedDeviceItemA11yLabel', 3, 3, 'dev2'),
        listItem2.ariaLabel);
    assertEquals(
        savedDevicesList.i18n('savedDeviceItemButtonA11yLabel', 'dev0'),
        iconMoreVert0.ariaLabel);
    assertEquals(
        savedDevicesList.i18n('savedDeviceItemButtonA11yLabel', 'dev1'),
        iconMoreVert1.ariaLabel);
    assertEquals(
        savedDevicesList.i18n('savedDeviceItemButtonA11yLabel', 'dev2'),
        iconMoreVert2.ariaLabel);
  });

  test('Device images render correctly', async () => {
    const device0 = {name: 'dev0', imageUrl: '', accountKey: '0'};
    const device1 = {name: 'dev1', imageUrl: 'fakeUrl', accountKey: '1'};

    savedDevicesList.set('devices', [device0, device1]);
    await flushTasks();

    assertTrue(isVisible(
        getListItems()[0]!.shadowRoot!.querySelector('#noDeviceImage')));
    assertFalse(isVisible(
        getListItems()[0]!.shadowRoot!.querySelector('#deviceImage')));
    assertFalse(isVisible(
        getListItems()[1]!.shadowRoot!.querySelector('#noDeviceImage')));
    assertTrue(isVisible(
        getListItems()[1]!.shadowRoot!.querySelector('#deviceImage')));
  });

  test('Device names are set properly', async () => {
    const getDeviceName = () => {
      return getListItems()[0]!.shadowRoot!
          .querySelector<HTMLElement>('#deviceName')!.innerText;
    };
    const deviceName = 'deviceName';
    const device = {name: deviceName, imageUrl: 'fakeUrl', accountKey: '1'};

    savedDevicesList.set('devices', [device]);
    await flushTasks();
    assertEquals(deviceName, getDeviceName());

    const nameWithHtml = '<a>test</a>';
    device.name = nameWithHtml;
    savedDevicesList.set('devices', [{...device}]);
    await flushTasks();
    assertEquals(nameWithHtml, getDeviceName());
  });
});
