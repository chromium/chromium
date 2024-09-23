// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPairedBluetoothListItemElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice} from 'chrome://webui-test/chromeos/bluetooth/fake_bluetooth_config.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('<os-settings-paired-bluetooth-list-item>', () => {
  let pairedBluetoothListItem: SettingsPairedBluetoothListItemElement;

  setup(() => {
    pairedBluetoothListItem =
        document.createElement('os-settings-paired-bluetooth-list-item');
    document.body.appendChild(pairedBluetoothListItem);
    flush();
  });

  teardown(() => {
    pairedBluetoothListItem.remove();
  });

  async function setBatteryPercentage(batteryPercentage: number):
      Promise<void> {
    pairedBluetoothListItem.set('device.deviceProperties.batteryInfo', {
      defaultProperties: {batteryPercentage},
      leftBudInfo: undefined,
      rightBudInfo: undefined,
      caseInfo: undefined,
    });
    await flushTasks();
  }

  test(
      'Device name, type, connection state, battery info and a11y labels',
      async () => {
        // Device with no nickname, battery info, not connected and unknown
        // device type.
        const publicName = 'BeatsX';
        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456789', /*publicName=*/ publicName,
            /*connectionState=*/
            DeviceConnectionState.kNotConnected);
        pairedBluetoothListItem.set('device', device);

        const itemIndex = 3;
        const listSize = 15;
        pairedBluetoothListItem.set('itemIndex', itemIndex);
        pairedBluetoothListItem.set('listSize', listSize);
        await flushTasks();

        const getDeviceName = () => {
          const deviceName =
              pairedBluetoothListItem.shadowRoot!.querySelector<HTMLElement>(
                  '#deviceName');
          assertTrue(!!deviceName);
          return deviceName;
        };
        const isShowingSubtitle = () => {
          const subtitle =
              pairedBluetoothListItem.shadowRoot!.querySelector<HTMLElement>(
                  '#subtitle');
          assertTrue(!!subtitle);
          return isVisible(subtitle);
        };
        const getBatteryInfo = () => {
          return pairedBluetoothListItem.shadowRoot!.querySelector(
              'bluetooth-device-battery-info');
        };
        const getDeviceTypeIcon = () => {
          return pairedBluetoothListItem.shadowRoot!.querySelector(
              'bluetooth-icon');
        };
        const getItemA11yLabel = () => {
          const listItem =
              pairedBluetoothListItem.shadowRoot!.querySelector('.list-item');
          assertTrue(!!listItem);
          return listItem.ariaLabel;
        };

        assertTrue(!!getDeviceName());
        assertEquals(publicName, getDeviceName().innerText);
        assertFalse(isShowingSubtitle());
        assertNull(getBatteryInfo());
        assertTrue(!!getDeviceTypeIcon());

        let expectedA11yLabel = [
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceName', itemIndex + 1, listSize, publicName),
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceConnectionStateNotConnected'),
          pairedBluetoothListItem.i18n('bluetoothA11yDeviceTypeUnknown'),
        ].join(' ');

        assertEquals(expectedA11yLabel, getItemA11yLabel());

        // Set device to connecting.
        device.deviceProperties.connectionState =
            DeviceConnectionState.kConnecting;
        pairedBluetoothListItem.set('device', {...device});
        await flushTasks();

        assertTrue(!!getDeviceName());
        assertEquals(publicName, getDeviceName().innerText);
        assertTrue(isShowingSubtitle());
        assertNull(getBatteryInfo());
        assertTrue(!!getDeviceTypeIcon());

        expectedA11yLabel = [
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceName', itemIndex + 1, listSize, publicName),
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceConnectionStateConnecting'),
          pairedBluetoothListItem.i18n('bluetoothA11yDeviceTypeUnknown'),
        ].join(' ');

        assertEquals(expectedA11yLabel, getItemA11yLabel());

        // Set device nickname, connection state, type and battery info.
        const nickname = 'nickname';
        device.nickname = nickname;
        device.deviceProperties.connectionState =
            DeviceConnectionState.kConnected;
        device.deviceProperties.deviceType = DeviceType.kComputer;
        const batteryPercentage = 60;
        device.deviceProperties.batteryInfo = {
          defaultProperties: {batteryPercentage},
          leftBudInfo: undefined,
          rightBudInfo: undefined,
          caseInfo: undefined,
        };
        pairedBluetoothListItem.set('device', {...device});
        await flushTasks();

        let expectedA11yBatteryLabel = pairedBluetoothListItem.i18n(
            'bluetoothA11yDeviceBatteryInfo', batteryPercentage);

        assertTrue(!!getDeviceName());
        assertEquals(nickname, getDeviceName().innerText);
        assertFalse(isShowingSubtitle());
        assertTrue(!!getBatteryInfo());
        assertEquals(
            pairedBluetoothListItem.get('device.deviceProperties'),
            getBatteryInfo()!.device);

        expectedA11yLabel = [
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceName', itemIndex + 1, listSize, nickname),
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceConnectionStateConnected'),
          pairedBluetoothListItem.i18n('bluetoothA11yDeviceTypeComputer'),
        ].join(' ');

        assertEquals(
            [expectedA11yLabel, expectedA11yBatteryLabel].join(' '),
            getItemA11yLabel());

        // Add True Wireless battery information and make sure that it takes
        // precedence over default battery information.
        const leftBudBatteryPercentage = 19;
        const caseBatteryPercentage = 29;
        const rightBudBatteryPercentage = 39;
        device.deviceProperties.batteryInfo!.leftBudInfo = {
          batteryPercentage: leftBudBatteryPercentage,
        };
        device.deviceProperties.batteryInfo!.caseInfo = {
          batteryPercentage: caseBatteryPercentage,
        };
        device.deviceProperties.batteryInfo!.rightBudInfo = {
          batteryPercentage: rightBudBatteryPercentage,
        };
        pairedBluetoothListItem.set('device', {...device});
        await flushTasks();

        expectedA11yBatteryLabel = [
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceNamedBatteryInfoLeftBud',
              leftBudBatteryPercentage),
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceNamedBatteryInfoCase', caseBatteryPercentage),
          pairedBluetoothListItem.i18n(
              'bluetoothA11yDeviceNamedBatteryInfoRightBud',
              rightBudBatteryPercentage),
        ].join(' ');

        assertEquals(
            [expectedA11yLabel, expectedA11yBatteryLabel].join(' '),
            getItemA11yLabel());
      });

  test('Device name is set correctly', async () => {
    const getDeviceName = () => {
      const deviceName =
          pairedBluetoothListItem.shadowRoot!.querySelector<HTMLElement>(
              '#deviceName');
      assertTrue(!!deviceName);
      return deviceName;
    };

    const publicName = 'BeatsX';
    const nameWithHtml = '<a>testname</a>';
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ publicName,
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    pairedBluetoothListItem.set('device', device);
    await flushTasks();
    assertEquals(publicName, getDeviceName().innerText);

    device.nickname = nameWithHtml;
    pairedBluetoothListItem.set('device', {...device});
    await flushTasks();
    assertTrue(!!getDeviceName());
    assertEquals(nameWithHtml, getDeviceName().innerText);
  });

  test('Battery percentage out of bounds', async () => {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    pairedBluetoothListItem.set('device', device);

    const getBatteryInfo = () => {
      return pairedBluetoothListItem.shadowRoot!.querySelector(
          'bluetooth-device-battery-info');
    };

    await setBatteryPercentage(-10);
    assertNull(getBatteryInfo());

    await setBatteryPercentage(101);
    assertNull(getBatteryInfo());
  });

  test('Selecting item routes to detail subpage', async () => {
    const id = '123456789';
    const device = createDefaultBluetoothDevice(
        id, /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    pairedBluetoothListItem.set('device', device);
    await flushTasks();

    const getItemContainer = () => {
      const listItem =
          pairedBluetoothListItem.shadowRoot!.querySelector<HTMLElement>(
              '.list-item');
      assertTrue(!!listItem);
      return listItem;
    };
    const assertInDetailSubpage = async () => {
      await flushTasks();
      assertEquals(
          routes.BLUETOOTH_DEVICE_DETAIL, Router.getInstance().currentRoute);
      assertEquals(id, Router.getInstance().getQueryParameters().get('id'));

      Router.getInstance().resetRouteForTesting();
      assertEquals(routes.BASIC, Router.getInstance().currentRoute);
    };

    // Simulate clicking item.
    assertEquals(routes.BASIC, Router.getInstance().currentRoute);
    getItemContainer().click();
    await assertInDetailSubpage();

    // Simulate pressing enter on the item.
    getItemContainer().dispatchEvent(
        new KeyboardEvent('keydown', {'key': 'Enter'}));
    await assertInDetailSubpage();

    // Simulate pressing space on the item.
    getItemContainer().dispatchEvent(
        new KeyboardEvent('keydown', {'key': ' '}));
    await assertInDetailSubpage();

    // Simulate clicking the item's subpage button.
    const button =
        pairedBluetoothListItem.shadowRoot!.querySelector<HTMLButtonElement>(
            '#subpageButton');
    assertTrue(!!button);
    button.click();
    await assertInDetailSubpage();
  });

  test('Enterprise-managed icon UI state', async () => {
    const getManagedIcon = () => {
      return pairedBluetoothListItem.shadowRoot!.querySelector('#managedIcon');
    };
    assertNull(getManagedIcon());

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse,
        /*opt_isBlockedByPolicy=*/ true);

    pairedBluetoothListItem.set('device', {...device});
    await flushTasks();

    // The icon should now be showing.
    assertTrue(!!getManagedIcon());

    // Simulate hovering over the icon.
    const showTooltipPromise =
        eventToPromise('managed-tooltip-state-change', pairedBluetoothListItem);
    getManagedIcon()!.dispatchEvent(new Event('mouseenter'));

    // The managed-tooltip-state-changed event should have been fired.
    const showTooltipEvent = await showTooltipPromise;
    assertEquals(true, showTooltipEvent.detail.show);
    assertEquals(getManagedIcon(), showTooltipEvent.detail.element);
    assertEquals(
        device.deviceProperties.address, showTooltipEvent.detail.address);

    // Simulate the device being unblocked by policy.
    const hideTooltipPromise =
        eventToPromise('managed-tooltip-state-change', pairedBluetoothListItem);
    const device1 = {...device};
    device1.deviceProperties.isBlockedByPolicy = false;
    pairedBluetoothListItem.set('device', device1);

    await flushTasks();

    // The icon should now be hidden.
    assertNull(getManagedIcon());

    // The managed-tooltip-state-changed event should have been fired again.
    const hideTooltipEvent = await hideTooltipPromise;
    assertFalse(hideTooltipEvent.detail.show);
    assertEquals(undefined, hideTooltipEvent.detail.element);
    assertEquals(
        device.deviceProperties.address, hideTooltipEvent.detail.address);
  });
});
