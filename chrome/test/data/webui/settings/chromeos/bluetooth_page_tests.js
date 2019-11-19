// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getFakePrefs() {
  return {
    ash: {
      user: {
        bluetooth: {
          adapter_enabled: {
            key: 'ash.user.bluetooth.adapter_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          }
        }
      }
    }
  };
}

/**
 * @param {number} numPairedDevices Number of paired devices to generate.
 * @param {number} numUnpairedDevices Number of unparied devices to generate.
 * @return {!Array<!chrome.bluetooth.Device>} An array of fake bluetooth
 *     devices.
 */
function generateFakeDevices(numPairedDevices, numUnpairedDevices) {
  const devices = [];
  for (let i = 0; i < numPairedDevices + numUnpairedDevices; ++i) {
    devices.push({
      address: '00:00:00:00:01:' + i.toString().padStart(2, '0'),
      name: 'FakeDevice' + i,
      paired: i < numPairedDevices,
      connected: false,
    });
  }
  return devices;
}

suite('Bluetooth', function() {
  let bluetoothPage = null;

  /** @type {Bluetooth} */
  let bluetoothApi;

  /** @type {BluetoothPrivate} */
  let bluetoothPrivateApi;

  /** @type {!chrome.bluetooth.Device} */
  const fakeUnpairedDevice1 = {
    address: '00:00:00:00:00:01',
    name: 'FakeUnpairedDevice1',
    paired: false,
    connected: false,
  };

  /** @type {!chrome.bluetooth.Device} */
  const fakeUnpairedDevice2 = {
    address: '00:00:00:00:00:02',
    name: 'FakeUnpairedDevice2',
    paired: false,
    connected: false,
  };

  /** @type {!chrome.bluetooth.Device} */
  const fakeUnpairedDevice3 = {
    address: '00:00:00:00:00:03',
    name: 'FakeUnpairedDevice3',
    paired: false,
    connected: false,
  };

  /** @type {!chrome.bluetooth.Device} */
  const fakePairedDevice1 = {
    address: '10:00:00:00:00:01',
    name: 'FakePairedDevice1',
    paired: true,
    connected: true,
  };

  /** @type {!chrome.bluetooth.Device} */
  const fakePairedDevice2 = {
    address: '10:00:00:00:00:02',
    name: 'FakePairedDevice2',
    paired: true,
    connected: false,
  };

  /** @type {!chrome.bluetooth.Device} */
  const fakePairedDevice3 = {
    address: '10:00:00:00:00:03',
    name: 'FakePairedDevice3',
    paired: true,
    connected: false,
  };

  suiteSetup(function() {
    loadTimeData.overrideValues({
      deviceOff: 'deviceOff',
      deviceOn: 'deviceOn',
      bluetoothConnected: 'bluetoothConnected',
      bluetoothDisconnect: 'bluetoothDisconnect',
      bluetoothPair: 'bluetoothPair',
      bluetoothStartConnecting: 'bluetoothStartConnecting',
    });

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    bluetoothApi = new settings.FakeBluetooth();
    bluetoothPrivateApi = new settings.FakeBluetoothPrivate(bluetoothApi);

    // Set globals to override Settings Bluetooth Page apis.
    bluetoothApis.bluetoothApiForTest = bluetoothApi;
    bluetoothApis.bluetoothPrivateApiForTest = bluetoothPrivateApi;

    PolymerTest.clearBody();
    bluetoothPage = document.createElement('settings-bluetooth-page');
    bluetoothPage.prefs = getFakePrefs();
    assertTrue(!!bluetoothPage);

    bluetoothApi.clearDevicesForTest();
    document.body.appendChild(bluetoothPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    bluetoothPage.remove();
  });

  test('MainPage', function() {
    assertFalse(bluetoothApi.getAdapterStateForTest().powered);
    assertFalse(bluetoothPage.bluetoothToggleState_);
    assertTrue(bluetoothPage.isToggleEnabled_());

    // Test that tapping the single settings-box div enables bluetooth.
    const div = bluetoothPage.$$('.link-wrapper');
    assertTrue(!!div);
    div.click();

    assertTrue(
        bluetoothPrivateApi.getLastSetAdapterStateValueForTest().powered);
    assertFalse(bluetoothPage.isToggleEnabled_());

    bluetoothPrivateApi.simulateSuccessfulSetAdapterStateCallForTest();
    assertTrue(bluetoothPage.isToggleEnabled_());
    assertTrue(bluetoothPage.bluetoothToggleState_);
  });

  // Tests that the adapter changing states affects the toggle.
  test('Main Page: State changed', function() {
    // The default adapter starts available and powered off.
    assertFalse(bluetoothApi.getAdapterStateForTest().powered);
    assertTrue(bluetoothApi.getAdapterStateForTest().available);

    assertFalse(bluetoothPage.bluetoothToggleState_);
    assertTrue(bluetoothPage.isToggleEnabled_());

    // Make the adapter unavailable.
    bluetoothApi.simulateAdapterStateChangedForTest({
      available: false,
      powered: false,
    });
    assertFalse(bluetoothPage.bluetoothToggleState_);
    assertFalse(bluetoothPage.isToggleEnabled_());

    // Make the adapter available again.
    bluetoothApi.simulateAdapterStateChangedForTest({
      available: true,
      powered: false,
    });
    assertFalse(bluetoothPage.bluetoothToggleState_);
    assertTrue(bluetoothPage.isToggleEnabled_());

    // Powered on the adapter.
    bluetoothApi.simulateAdapterStateChangedForTest({
      available: true,
      powered: true,
    });
    assertTrue(bluetoothPage.bluetoothToggleState_);
    assertTrue(bluetoothPage.isToggleEnabled_());
  });

  suite('SubPage', function() {
    let subpage;

    function flushAsync() {
      Polymer.dom.flush();
      return new Promise(resolve => {
        bluetoothPage.async(resolve);
      });
    }

    setup(async function() {
      bluetoothApi.simulateAdapterStateChangedForTest({
        available: true,
        powered: true,
      });

      Polymer.dom.flush();
      const div = bluetoothPage.$$('.link-wrapper');
      div.click();

      await flushAsync();

      subpage = bluetoothPage.$$('settings-bluetooth-subpage');
      subpage.listUpdateFrequencyMs = 0;
      assertTrue(!!subpage);
      assertTrue(subpage.bluetoothToggleState);
      assertFalse(subpage.stateChangeInProgress);
      assertEquals(0, subpage.listUpdateFrequencyMs);
    });

    test('toggle', function() {
      assertTrue(subpage.bluetoothToggleState);
      assertTrue(subpage.isToggleEnabled_());

      const enableButton = subpage.$.enableBluetooth;
      assertTrue(!!enableButton);
      assertTrue(enableButton.checked);

      // Changing the toggle should power off the adapter.
      subpage.bluetoothToggleState = false;
      assertFalse(enableButton.checked);
      assertFalse(
          bluetoothPrivateApi.getLastSetAdapterStateValueForTest().powered);
      assertFalse(subpage.isToggleEnabled_());

      bluetoothPrivateApi.simulateSuccessfulSetAdapterStateCallForTest();
      assertFalse(bluetoothPage.bluetoothToggleState_);
      assertTrue(subpage.isToggleEnabled_());
    });

    async function waitForListUpdateTimeout() {
      // listUpdateFrequencyMs is set to 0 for tests, but we still need to wait
      // for the callback of setTimeout(0) to be processed in the message queue.
      await new Promise(function(resolve) {
        setTimeout(resolve, 0);
      });

      // Adding two flushTasks ensures that all events are fully handled after
      // being fired.
      await test_util.flushTasks();
      await test_util.flushTasks();
    }

    test('pair device', async function() {
      bluetoothApi.simulateDevicesAddedForTest([
        fakeUnpairedDevice1, fakeUnpairedDevice2, fakePairedDevice1,
        fakePairedDevice2
      ]);

      await waitForListUpdateTimeout();

      // TODO(jlklein): Stop referencing private state in these tests. Only use
      // public observable state.
      assertEquals(4, subpage.deviceList_.length);
      assertEquals(2, subpage.pairedDeviceList_.length);
      assertEquals(2, subpage.unpairedDeviceList_.length);

      const address = subpage.unpairedDeviceList_[0].address;
      await new Promise(
          resolve => bluetoothPrivateApi.connect(address, resolve));

      await waitForListUpdateTimeout();

      assertEquals(3, subpage.pairedDeviceList_.length);
      assertEquals(1, subpage.unpairedDeviceList_.length);
    });

    test('pair dialog', async function() {
      bluetoothApi.simulateDevicesAddedForTest([
        fakeUnpairedDevice1, fakeUnpairedDevice2, fakePairedDevice1,
        fakePairedDevice2
      ]);
      await waitForListUpdateTimeout();

      const dialog = subpage.$.deviceDialog;
      assertTrue(!!dialog);
      assertFalse(dialog.$.dialog.open);

      // Simulate selecting an unpaired device; should show the pair dialog.
      subpage.connectDevice_(subpage.unpairedDeviceList_[0]);
      Polymer.dom.flush();
      assertTrue(dialog.$.dialog.open);
    });

    suite('Device List', function() {
      function deviceList() {
        return subpage.deviceList_;
      }

      function unpairedDeviceList() {
        return subpage.unpairedDeviceList_;
      }

      function pairedDeviceList() {
        return subpage.pairedDeviceList_;
      }

      let unpairedContainer;
      let unpairedDeviceIronList;

      let pairedContainer;
      let pairedDeviceIronList;

      setup(function() {
        unpairedContainer = subpage.$.unpairedContainer;
        assertTrue(!!unpairedContainer);
        assertTrue(unpairedContainer.hidden);
        unpairedDeviceIronList = subpage.$.unpairedDevices;
        assertTrue(!!unpairedDeviceIronList);

        pairedContainer = subpage.$.pairedContainer;
        assertTrue(!!pairedContainer);
        assertTrue(pairedContainer.hidden);
        pairedDeviceIronList = subpage.$.pairedDevices;
        assertTrue(!!pairedDeviceIronList);
      });

      test('Unpaired devices: added and removed', async function() {
        // Add two unpaired devices.
        bluetoothApi.simulateDevicesAddedForTest(
            [fakeUnpairedDevice1, fakeUnpairedDevice2]);
        await waitForListUpdateTimeout();

        assertEquals(2, deviceList().length);
        assertEquals(2, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        assertEquals(
            unpairedDeviceList()[0].address, fakeUnpairedDevice1.address);
        assertEquals(
            unpairedDeviceList()[1].address, fakeUnpairedDevice2.address);

        Polymer.dom.flush();

        const devices = unpairedDeviceIronList.querySelectorAll(
            'bluetooth-device-list-item');
        assertEquals(2, devices.length);
        assertFalse(devices[0].device.paired);
        assertFalse(devices[1].device.paired);

        // Remove the first device.
        bluetoothApi.simulateDevicesRemovedForTest(
            [fakeUnpairedDevice1.address]);

        await waitForListUpdateTimeout();
        Polymer.dom.flush();

        assertEquals(1, deviceList().length);
        assertEquals(1, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        assertEquals(
            unpairedDeviceList()[0].address, fakeUnpairedDevice2.address);

        // Add the first device again; it should be added at the end of the
        // list.
        bluetoothApi.simulateDevicesAddedForTest([fakeUnpairedDevice1]);

        await waitForListUpdateTimeout();

        assertEquals(2, deviceList().length);
        assertEquals(2, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        assertEquals(
            unpairedDeviceList()[0].address, fakeUnpairedDevice2.address);
        assertEquals(
            unpairedDeviceList()[1].address, fakeUnpairedDevice1.address);

        // Remove both devices.
        bluetoothApi.simulateDevicesRemovedForTest(
            [fakeUnpairedDevice1.address, fakeUnpairedDevice2.address]);

        await waitForListUpdateTimeout();

        assertEquals(0, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);
      });

      test('Unpaired devices: device updated', async function() {
        // Add three unpaired devices.
        bluetoothApi.simulateDevicesAddedForTest(
            [fakeUnpairedDevice1, fakeUnpairedDevice2, fakeUnpairedDevice3]);

        await waitForListUpdateTimeout();

        assertEquals(3, deviceList().length);
        assertEquals(3, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        // Update the one in the middle.
        const updatedDevice = Object.assign({}, fakeUnpairedDevice2);
        updatedDevice.name = 'Updated Name';
        bluetoothApi.simulateDeviceUpdatedForTest(updatedDevice);

        await waitForListUpdateTimeout();

        assertEquals(3, deviceList().length);
        assertEquals(3, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        for (const prop in updatedDevice) {
          assertEquals(updatedDevice[prop], deviceList()[1][prop]);
          assertEquals(updatedDevice[prop], unpairedDeviceList()[1][prop]);
        }
      });

      test('Paired devices: devices added and removed', async function() {
        // Add two paired devices.
        bluetoothApi.simulateDevicesAddedForTest(
            [fakePairedDevice1, fakePairedDevice2]);
        await waitForListUpdateTimeout();

        assertEquals(2, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(2, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        Polymer.dom.flush();

        const devices =
            pairedDeviceIronList.querySelectorAll('bluetooth-device-list-item');
        assertEquals(2, devices.length);
        assertTrue(devices[0].device.paired);
        assertTrue(devices[0].device.connected);
        assertTrue(devices[1].device.paired);
        assertFalse(devices[1].device.connected);

        // Remove the first device.
        bluetoothApi.simulateDevicesRemovedForTest([fakePairedDevice1.address]);

        await waitForListUpdateTimeout();

        assertEquals(1, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(1, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        assertEquals(pairedDeviceList()[0].address, fakePairedDevice2.address);

        // Add the first device again; it should be added at the end of the
        // list.
        bluetoothApi.simulateDevicesAddedForTest([fakePairedDevice1]);

        await waitForListUpdateTimeout();

        assertEquals(2, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(2, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        assertEquals(pairedDeviceList()[0].address, fakePairedDevice2.address);
        assertEquals(pairedDeviceList()[1].address, fakePairedDevice1.address);

        // Remove both devices.
        bluetoothApi.simulateDevicesRemovedForTest(
            [fakePairedDevice1.address, fakePairedDevice2.address]);

        await waitForListUpdateTimeout();

        assertEquals(0, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);
      });

      test('Paired devices: device updated', async function() {
        // Add three paired devices.
        bluetoothApi.simulateDevicesAddedForTest(
            [fakePairedDevice1, fakePairedDevice2, fakePairedDevice3]);

        await waitForListUpdateTimeout();

        assertEquals(3, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(3, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        // Update the one in the middle.
        const updatedDevice = Object.assign({}, fakePairedDevice2);
        updatedDevice.name = 'Updated Name';
        bluetoothApi.simulateDeviceUpdatedForTest(updatedDevice);

        await waitForListUpdateTimeout();

        assertEquals(3, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(3, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        for (const prop in updatedDevice) {
          assertEquals(updatedDevice[prop], deviceList()[1][prop]);
          assertEquals(updatedDevice[prop], pairedDeviceList()[1][prop]);
        }
      });

      test('Unpaired device becomes paired', async function() {
        // Add unpaired device.
        bluetoothApi.simulateDevicesAddedForTest([fakeUnpairedDevice1]);

        await waitForListUpdateTimeout();

        assertEquals(1, deviceList().length);
        assertEquals(1, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);

        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        // Mark the device as paired.
        const nowPairedDevice = Object.assign({}, fakeUnpairedDevice1);
        nowPairedDevice.paired = true;
        bluetoothApi.simulateDeviceUpdatedForTest(nowPairedDevice);

        await waitForListUpdateTimeout();

        assertEquals(1, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(1, pairedDeviceList().length);

        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        assertTrue(deviceList()[0].paired);
        assertTrue(pairedDeviceList()[0].paired);
      });

      test('Paired device becomes unpaired', async function() {
        // Add paired device.
        bluetoothApi.simulateDevicesAddedForTest([fakePairedDevice1]);

        await waitForListUpdateTimeout();

        assertEquals(1, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(1, pairedDeviceList().length);

        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        // Mark the device as not paired.
        const nowUnpairedDevice = Object.assign({}, fakePairedDevice1);
        nowUnpairedDevice.paired = false;
        bluetoothApi.simulateDeviceUpdatedForTest(nowUnpairedDevice);

        await waitForListUpdateTimeout();

        assertEquals(1, deviceList().length);
        assertEquals(1, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);

        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        assertFalse(deviceList()[0].paired);
        assertFalse(unpairedDeviceList()[0].paired);
      });

      test('Unpaired and paired devices: devices added', async function() {
        bluetoothApi.simulateDevicesAddedForTest([
          fakeUnpairedDevice1, fakeUnpairedDevice2, fakePairedDevice1,
          fakePairedDevice2
        ]);

        await waitForListUpdateTimeout();

        assertEquals(4, deviceList().length);
        assertEquals(2, unpairedDeviceList().length);
        assertEquals(2, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        Polymer.dom.flush();

        const unpairedDevices = unpairedDeviceIronList.querySelectorAll(
            'bluetooth-device-list-item');
        assertEquals(2, unpairedDevices.length);
        assertFalse(unpairedDevices[0].device.paired);
        assertFalse(unpairedDevices[1].device.paired);

        const pairedDevices =
            pairedDeviceIronList.querySelectorAll('bluetooth-device-list-item');
        assertEquals(2, pairedDevices.length);
        assertTrue(pairedDevices[0].device.paired);
        assertTrue(pairedDevices[0].device.connected);
        assertTrue(pairedDevices[1].device.paired);
        assertFalse(pairedDevices[1].device.connected);
      });

      test('Unpaired and paired devices: many devices added', async function() {
        bluetoothApi.simulateDevicesAddedForTest(generateFakeDevices(5, 15));

        await waitForListUpdateTimeout();

        assertEquals(20, deviceList().length);
        assertEquals(15, unpairedDeviceList().length);
        assertEquals(5, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        const unpairedDevices = unpairedDeviceIronList.querySelectorAll(
            'bluetooth-device-list-item');
        assertEquals(15, unpairedDevices.length);

        const pairedDevices =
            pairedDeviceIronList.querySelectorAll('bluetooth-device-list-item');
        assertEquals(5, pairedDevices.length);
      });
    });
  });
});
