// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {adapterBroker, checkSystemPermissions, devices, initializeViews, pageManager, sidebarObj} from 'chrome://bluetooth-internals/bluetooth_internals.js';
import {BluetoothInternalsHandler} from 'chrome://bluetooth-internals/bluetooth_internals.mojom-webui.js';
import {connectedDevices} from 'chrome://bluetooth-internals/device_broker.js';
import {dismissSnackbar, getSnackbarStateForTest, showSnackbar} from 'chrome://bluetooth-internals/snackbar.js';
import {ValueDataType} from 'chrome://bluetooth-internals/value_control.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {$} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
// <if expr="chromeos_ash">
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// </if>

import {fakeAdapterInfo, fakeCharacteristicInfo1, fakeDeviceInfo1, fakeDeviceInfo2, fakeDeviceInfo3, fakeServiceInfo1, fakeServiceInfo2, TestAdapter, TestBluetoothInternalsHandler, TestDevice} from './test_utils.js';

suite('bluetooth_internals', function() {
  const whenSetupDone = new PromiseResolver();
  let internalsHandler = null;
  let adapterFieldSet = null;
  let deviceTable = null;
  let sidebarNode = null;
  let bluetoothInternalsHandlerRemote = null;
  const pageNames = ['adapter', 'devices'];
  const EXPECTED_DEVICES = 2;

  suiteSetup(async function() {
    const internalsHandlerInterceptor =
        new MojoInterfaceInterceptor(BluetoothInternalsHandler.$interfaceName);
    internalsHandlerInterceptor.oninterfacerequest = (e) => {
      internalsHandler = new TestBluetoothInternalsHandler(e.handle);

      const testAdapter = new TestAdapter(fakeAdapterInfo());
      testAdapter.setTestDevices([
        fakeDeviceInfo1(),
        fakeDeviceInfo2(),
      ]);

      const testServices = [fakeServiceInfo1(), fakeServiceInfo2()];

      testAdapter.setTestServicesForTestDevice(
          fakeDeviceInfo1(), Object.assign({}, testServices));
      testAdapter.setTestServicesForTestDevice(
          fakeDeviceInfo2(), Object.assign({}, testServices));

      internalsHandler.setAdapterForTesting(testAdapter);
      whenSetupDone.resolve();
    };
    internalsHandlerInterceptor.start();
    bluetoothInternalsHandlerRemote = BluetoothInternalsHandler.getRemote();
    await checkSystemPermissions(
        bluetoothInternalsHandlerRemote, initializeViews);
    await whenSetupDone.promise;
    await Promise.all([
      internalsHandler.whenCalled('checkSystemPermissions'),
      internalsHandler.whenCalled('getAdapter'),
      internalsHandler.adapter.whenCalled('getInfo'),
      internalsHandler.adapter.whenCalled('getDevices'),
      internalsHandler.adapter.whenCalled('addObserver'),
    ]);
  });

  setup(function() {
    adapterFieldSet = document.querySelector('#adapter object-field-set');
    deviceTable = document.querySelector('#devices device-table')
                      .shadowRoot.querySelector('table');
    sidebarNode = document.querySelector('#sidebar');
    devices.resetForTest();
    adapterBroker.deviceAdded(fakeDeviceInfo1());
    adapterBroker.deviceAdded(fakeDeviceInfo2());
  });

  teardown(function() {
    internalsHandler.reset();
    sidebarObj.close();
    dismissSnackbar(true);
    connectedDevices.clear();

    pageManager.registeredPages.get('adapter').setAdapterInfo(
        fakeAdapterInfo());

    for (const pageName of pageManager.registeredPages.keys()) {
      const page = pageManager.registeredPages.get(pageName);

      if (pageNames.indexOf(pageName) < 0) {
        page.pageDiv.parentNode.removeChild(page.pageDiv);
        pageManager.unregister(page);
      }
    }

    // Close all of the dialogs.
    document.getElementById('need-location-services-on').close();
    document.getElementById('need-location-permission-and-services-on').close();
    document.getElementById('need-nearby-devices-permission').close();
    document.getElementById('need-location-permission').close();
    document.getElementById('can-not-request-permissions').close();
    document.getElementById('refresh-page').close();
  });

  /**
   * Updates device info and verifies the contents of the device table.
   * @param {!DeviceInfo} deviceInfo
   */
  function changeDevice(deviceInfo) {
    const deviceRow = deviceTable.querySelector(
        '#' + escapeDeviceAddress(deviceInfo.address));
    const nameForDisplayColumn = deviceRow.children[0];
    const addressColumn = deviceRow.children[1];
    const rssiColumn = deviceRow.children[2];
    const serviceUuidsColumn = deviceRow.children[3];
    const manufacturerDataColumn = deviceRow.children[4];

    assertTrue(!!nameForDisplayColumn);
    assertTrue(!!addressColumn);
    assertTrue(!!rssiColumn);
    assertTrue(!!serviceUuidsColumn);
    assertTrue(!!manufacturerDataColumn);

    adapterBroker.deviceChanged(deviceInfo);

    assertEquals(deviceInfo.nameForDisplay, nameForDisplayColumn.textContent);
    assertEquals(deviceInfo.address, addressColumn.textContent);

    if (deviceInfo.rssi) {
      assertEquals(String(deviceInfo.rssi.value), rssiColumn.textContent);
    }

    if (deviceInfo.serviceUuids) {
      assertEquals(
          formatServiceUuids(deviceInfo.serviceUuids),
          serviceUuidsColumn.textContent);
    }

    if (deviceInfo.manufacturerDataMap) {
      assertEquals(
          formatManufacturerDataMap(deviceInfo.manufacturerDataMap),
          manufacturerDataColumn.textContent);
    }
  }

  /**
   * Format in a user readable way service UUIDs.
   * @param ?Array<UUID> uuids
   * @return {string}
   */
  function formatServiceUuids(serviceUuids) {
    if (!serviceUuids) {
      return '';
    }
    return serviceUuids.map(service => service.uuid).join(', ');
  }

  /**
   * Format in a user readable way device manufacturer data map. Keys are
   * Bluetooth company identifiers (unsigned short), values are bytes.
   * @param {Map<string, array<number>>} manufacturerDataMap
   * @return {string}
   */
  function formatManufacturerDataMap(manufacturerDataMap) {
    return Object.entries(manufacturerDataMap)
        .map(([key, value]) => {
          const companyIdentifier = parseInt(key).toString(16).padStart(4, '0');
          const data = value.map(v => v.toString(16).padStart(2, '0')).join('');
          return `0x${companyIdentifier} 0x${data}`;
        })
        .join(' | ');
  }

  /**
   * Escapes colons in a device address for CSS formatting.
   * @param {string} address
   */
  function escapeDeviceAddress(address) {
    return address.replace(/:/g, '\\:');
  }

  /**
   * Expects whether device with |address| is removed.
   * @param {string} address
   * @param {boolean} expectRemoved
   */
  function expectDeviceRemoved(address, expectRemoved) {
    const removedRow =
        deviceTable.querySelector('#' + escapeDeviceAddress(address));

    assertEquals(expectRemoved, removedRow.classList.contains('removed'));
  }

  /**
   * Tests whether a device is added successfully and not duplicated.
   */
  test('DeviceAdded', function() {
    let devices = deviceTable.querySelectorAll('tbody tr');
    assertEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    const infoCopy = fakeDeviceInfo3();
    adapterBroker.deviceAdded(infoCopy);

    // Same device shouldn't appear twice.
    adapterBroker.deviceAdded(infoCopy);

    devices = deviceTable.querySelectorAll('tbody tr');
    assertEquals(EXPECTED_DEVICES + 1, devices.length);
  });

  /**
   * Tests whether a device is marked properly as removed.
   */
  test('DeviceSetToRemoved', function() {
    let devices = deviceTable.querySelectorAll('tbody tr');
    assertEquals(EXPECTED_DEVICES, devices.length);

    const fakeDevice = fakeDeviceInfo2();
    adapterBroker.deviceRemoved(fakeDevice);

    // The number of rows shouldn't change.
    devices = deviceTable.querySelectorAll('tbody tr');
    assertEquals(EXPECTED_DEVICES, devices.length);

    expectDeviceRemoved(fakeDevice.address, true);
  });

  /**
   * Tests whether a changed device updates the device table properly.
   */
  test('DeviceChanged', function() {
    const devices = deviceTable.querySelectorAll('tbody tr');
    assertEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    const newDeviceInfo = fakeDeviceInfo1();
    newDeviceInfo.nameForDisplay = 'DDDD';
    newDeviceInfo.rssi = {value: -20};
    newDeviceInfo.serviceUuids = [
      {uuid: '00002a05-0000-1000-8000-00805f9b34fb'},
      {uuid: '0000180d-0000-1000-8000-00805f9b34fb'},
    ];

    changeDevice(newDeviceInfo);
  });

  /**
   * Tests the entire device cycle, added -> updated -> removed -> re-added.
   */
  test('DeviceUpdateCycle', function() {
    const devices = deviceTable.querySelectorAll('tbody tr');
    assertEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    const originalDeviceInfo = fakeDeviceInfo3();
    adapterBroker.deviceAdded(originalDeviceInfo);

    const newDeviceInfo = fakeDeviceInfo3();
    newDeviceInfo.nameForDisplay = 'DDDD';
    newDeviceInfo.rssi = {value: -20};
    newDeviceInfo.serviceUuids = [
      {uuid: '00002a05-0000-1000-8000-00805f9b34fb'},
      {uuid: '0000180d-0000-1000-8000-00805f9b34fb'},
    ];

    changeDevice(newDeviceInfo);
    changeDevice(originalDeviceInfo);

    adapterBroker.deviceRemoved(originalDeviceInfo);
    expectDeviceRemoved(originalDeviceInfo.address, true);

    adapterBroker.deviceAdded(originalDeviceInfo);
    expectDeviceRemoved(originalDeviceInfo.address, false);
  });

  test('DeviceAddedRssiCheck', function() {
    const devices = deviceTable.querySelectorAll('tbody tr');
    assertEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    const newDeviceInfo = fakeDeviceInfo3();
    adapterBroker.deviceAdded(newDeviceInfo);

    const deviceRow = deviceTable.querySelector(
        '#' + escapeDeviceAddress(newDeviceInfo.address));
    const rssiColumn = deviceRow.children[2];
    assertEquals('Unknown', rssiColumn.textContent);

    const newDeviceInfo1 = fakeDeviceInfo3();
    newDeviceInfo1.rssi = {value: -42};
    adapterBroker.deviceChanged(newDeviceInfo1);
    assertEquals('-42', rssiColumn.textContent);

    // Device table should keep last valid rssi value.
    const newDeviceInfo2 = fakeDeviceInfo3();
    newDeviceInfo2.rssi = null;
    adapterBroker.deviceChanged(newDeviceInfo2);
    assertEquals('-42', rssiColumn.textContent);

    const newDeviceInfo3 = fakeDeviceInfo3();
    newDeviceInfo3.rssi = {value: -17};
    adapterBroker.deviceChanged(newDeviceInfo3);
    assertEquals('-17', rssiColumn.textContent);
  });

  /* Sidebar Tests */
  test('Sidebar_Setup', function() {
    const sidebarItems =
        Array.from(sidebarNode.querySelectorAll('.sidebar-content li'));

    pageNames.forEach(function(pageName) {
      assertTrue(sidebarItems.some(function(item) {
        return item.dataset.pageName === pageName;
      }));
    });
  });

  test('Sidebar_DefaultState', function() {
    // Sidebar should be closed by default.
    assertFalse(sidebarNode.classList.contains('open'));
  });

  test('Sidebar_OpenClose', function() {
    sidebarObj.open();
    assertTrue(sidebarNode.classList.contains('open'));
    sidebarObj.close();
    assertFalse(sidebarNode.classList.contains('open'));
  });

  test('Sidebar_OpenTwice', function() {
    // Multiple calls to open shouldn't change the state.
    sidebarObj.open();
    sidebarObj.open();
    assertTrue(sidebarNode.classList.contains('open'));
  });

  test('Sidebar_CloseTwice', function() {
    // Multiple calls to close shouldn't change the state.
    sidebarObj.close();
    sidebarObj.close();
    assertFalse(sidebarNode.classList.contains('open'));
  });

  /* Snackbar Tests */

  /**
   * Checks snackbar showing status and returns a Promise that resolves when
   * |pendingSnackbar| is shown. If the snackbar is already showing, the
   * Promise resolves immediately.
   * @param {!Snackbar} pendingSnackbar
   * @return {!Promise}
   */
  function whenSnackbarShows(pendingSnackbar) {
    return new Promise(function(resolve) {
      if (pendingSnackbar.classList.contains('open')) {
        resolve();
      } else {
        pendingSnackbar.addEventListener('showed', resolve);
      }
    });
  }

  /**
   * Performs final checks for snackbar tests.
   * @return {!Promise} Promise is fulfilled when the checks finish.
   */
  function finishSnackbarTest() {
    return new Promise(function(resolve) {
      // Let event queue finish.
      setTimeout(function() {
        assertEquals(0, $('snackbar-container').children.length);
        assertFalse(!!getSnackbarStateForTest().current);
        resolve();
      }, 50);
    });
  }

  test('Snackbar_ShowTimeout', function(done) {
    const snackbar1 = showSnackbar('Message 1');
    assertEquals(1, $('snackbar-container').children.length);

    snackbar1.addEventListener('dismissed', function() {
      finishSnackbarTest().then(done);
    });
  });

  test('Snackbar_ShowDismiss', function() {
    const snackbar1 = showSnackbar('Message 1');
    assertEquals(1, $('snackbar-container').children.length);

    return whenSnackbarShows(snackbar1)
        .then(function() {
          return dismissSnackbar();
        })
        .then(finishSnackbarTest);
  });

  test('Snackbar_QueueThreeDismiss', function() {
    const expectedCalls = 3;
    let actualCalls = 0;

    const snackbar1 = showSnackbar('Message 1');
    const snackbar2 = showSnackbar('Message 2');
    const snackbar3 = showSnackbar('Message 3');

    assertEquals(1, $('snackbar-container').children.length);
    assertEquals(2, getSnackbarStateForTest().numPending);

    function next() {
      actualCalls++;
      return dismissSnackbar();
    }

    whenSnackbarShows(snackbar1).then(next);
    whenSnackbarShows(snackbar2).then(next);
    return whenSnackbarShows(snackbar3)
        .then(next)
        .then(function() {
          assertEquals(expectedCalls, actualCalls);
        })
        .then(finishSnackbarTest);
  });

  test('Snackbar_QueueThreeDismissAll', function() {
    const expectedCalls = 1;
    const actualCalls = 0;

    const snackbar1 = showSnackbar('Message 1');
    const snackbar2 = showSnackbar('Message 2');
    const snackbar3 = showSnackbar('Message 3');

    assertEquals(1, $('snackbar-container').children.length);
    assertEquals(2, getSnackbarStateForTest().numPending);

    function next() {
      assertTrue(false);
    }

    whenSnackbarShows(snackbar2).then(next);
    snackbar2.addEventListener('dismissed', next);
    whenSnackbarShows(snackbar3).then(next);
    snackbar3.addEventListener('dismissed', next);

    return whenSnackbarShows(snackbar1)
        .then(function() {
          return dismissSnackbar(true);
        })
        .then(function() {
          assertEquals(0, getSnackbarStateForTest().numPending);
          assertFalse(!!getSnackbarStateForTest().current);
        })
        .then(finishSnackbarTest);
  });

  /* AdapterPage Tests */
  function checkAdapterFieldSet(adapterInfo) {
    for (const propName in adapterInfo) {
      const valueCell = adapterFieldSet.shadowRoot.querySelector(
          `fieldset [data-field="${propName}"]`);
      const value = adapterInfo[propName];

      if (typeof (value) === 'boolean') {
        assertEquals(value, valueCell.classList.contains('checked'));
      } else if (typeof (value) === 'string') {
        assertEquals(value, valueCell.textContent);
      } else {
        assertNotReached(
            'boolean or string type expected but got ' + typeof (value));
      }
    }
  }

  test('AdapterPage_DefaultState', function() {
    checkAdapterFieldSet(adapterFieldSet.value);
  });

  test('AdapterPage_AdapterChanged', function() {
    const adapterInfo = adapterFieldSet.value;

    adapterInfo.present = !adapterInfo.present;
    adapterBroker.presentChanged(adapterInfo.present);
    checkAdapterFieldSet(adapterInfo);

    adapterInfo.discovering = !adapterInfo.discovering;
    adapterBroker.discoveringChanged(adapterInfo.discovering);
    checkAdapterFieldSet(adapterInfo);
  });

  test('AdapterPage_AdapterChanged_RepeatTwice', function() {
    const adapterInfo = adapterFieldSet.value;

    adapterInfo.present = !adapterInfo.present;
    adapterBroker.presentChanged(adapterInfo.present);
    checkAdapterFieldSet(adapterInfo);
    adapterBroker.presentChanged(adapterInfo.present);
    checkAdapterFieldSet(adapterInfo);

    adapterInfo.discovering = !adapterInfo.discovering;
    adapterBroker.discoveringChanged(adapterInfo.discovering);
    checkAdapterFieldSet(adapterInfo);
    adapterBroker.discoveringChanged(adapterInfo.discovering);
    checkAdapterFieldSet(adapterInfo);
  });

  /** Device Details Page Tests */

  /**
   * Checks DeviceDetailsPage status fieldset.
   * @param {!HTMLElement} detailsPage
   * @param {!Object} deviceInfo
   */
  function checkDeviceDetailsFieldSet(detailsPage, deviceInfo) {
    ['name',
     'address',
     'isGattConnected',
     'rssi.value',
     'serviceUuids',
     'manufacturerDataMap',
    ].forEach(function(propName) {
      const valueCell =
          detailsPage.querySelector('object-field-set')
              .shadowRoot.querySelector(`fieldset [data-field="${propName}"]`);

      const parts = propName.split('.');
      let value = deviceInfo;

      while (value != null && parts.length > 0) {
        const part = parts.shift();
        value = value[part];
      }

      if (propName === 'isGattConnected') {
        value = value ? 'Connected' : 'Not Connected';
      } else if (propName === 'serviceUuids') {
        value = formatServiceUuids(value);
      } else if (propName === 'manufacturerDataMap') {
        value = formatManufacturerDataMap(value);
      }

      if (typeof (value) === 'boolean') {
        assertEquals(value, valueCell.classList.contains('checked'));
      } else if (typeof (value) === 'string') {
        assertEquals(value, valueCell.textContent);
      } else if (typeof (value) === 'number') {
        assertEquals(value.toString(), valueCell.textContent);
      } else {
        assertNotReached(
            'boolean, number or string type expected but got ' +
            typeof (value));
      }
    });
  }

  test('DeviceDetailsPage_NewDelete', function() {
    const device = devices.item(0);

    // Have to search manually since the device row IDs aren't valid selectors.
    const deviceRow = Array.from(deviceTable.querySelectorAll('tr'))
                          .find(row => row.id === device.address);
    const deviceInspectLink = deviceRow.querySelector('[is="action-link"]');

    const deviceDetailsPageId = 'devices/' + device.address.toLowerCase();

    deviceInspectLink.click();
    assertEquals('#' + deviceDetailsPageId, window.location.hash);

    let detailsPage = document.getElementById(deviceDetailsPageId);
    assertTrue(!!detailsPage);

    return internalsHandler.adapter.deviceImplMap.get(device.address)
        .whenCalled('getServices')
        .then(function() {
          // At this point, the device details page should be fully loaded.
          checkDeviceDetailsFieldSet(detailsPage, device);

          detailsPage.querySelector('.forget').click();
          assertEquals('#devices', window.location.hash);
          detailsPage = document.getElementById(deviceDetailsPageId);
          assertFalse(!!detailsPage);
        });
  });

  test('DeviceDetailsPage_NewDelete_FromDevicesPage', function() {
    const device = devices.item(0);
    const deviceDetailsPageId = 'devices/' + device.address.toLowerCase();

    const deviceRow = Array.from(deviceTable.querySelectorAll('tr'))
                          .find(row => row.id === device.address);
    const deviceLinks = deviceRow.querySelectorAll('[is="action-link"]');

    // First link is 'Inspect'.
    deviceLinks[0].click();
    assertEquals('#' + deviceDetailsPageId, window.location.hash);

    let detailsPage = document.getElementById(deviceDetailsPageId);
    assertTrue(!!detailsPage);

    return internalsHandler.adapter.deviceImplMap.get(device.address)
        .whenCalled('getServices')
        .then(function() {
          // At this point, the device details page should be fully loaded.
          checkDeviceDetailsFieldSet(detailsPage, device);

          // Second link is 'Forget'.
          deviceLinks[1].click();
          assertEquals('#devices', window.location.hash);
          detailsPage = document.getElementById(deviceDetailsPageId);
          assertFalse(!!detailsPage);
        });
  });

  test('CheckSystemPermissions_need_location_permission', async function() {
    internalsHandler.setSystemPermission(
        /*needLocationPermission=*/ true,
        /*needNearbyDevicesPermission=*/ false,
        /*needLocationServices=*/ false,
        /*canRequestPermissions=*/ true,
    );
    await checkSystemPermissions(bluetoothInternalsHandlerRemote, () => {
      assert(false);
    });
    await internalsHandler.whenCalled('checkSystemPermissions');
    assertTrue(document.getElementById('need-location-permission').open);
    document.getElementById('need-location-permission-permission-link').click();
    await internalsHandler.whenCalled('requestSystemPermissions');
    assertFalse(document.getElementById('need-location-permission').open);
    assertTrue(document.getElementById('refresh-page').open);
  });

  test(
      'CheckSystemPermissions_need_location_permission_and_services_on_' +
          'click_permission_link',
      async function() {
        internalsHandler.setSystemPermission(
            /*needLocationPermission=*/ true,
            /*needNearbyDevicesPermission=*/ false,
            /*needLocationServices=*/ true,
            /*canRequestPermissions=*/ true,
        );
        await checkSystemPermissions(bluetoothInternalsHandlerRemote, () => {
          assert(false);
        });
        await internalsHandler.whenCalled('checkSystemPermissions');
        assertTrue(
            document.getElementById('need-location-permission-and-services-on')
                .open);
        document
            .getElementById(
                'need-location-permission-and-services-on-permission-link')
            .click();
        await internalsHandler.whenCalled('requestSystemPermissions');
        assertFalse(
            document.getElementById('need-location-permission-and-services-on')
                .open);
        assertTrue(document.getElementById('refresh-page').open);
      });

  test(
      'CheckSystemPermissions_need_location_permission_and_services_on_' +
          'click_services_link',
      async function() {
        internalsHandler.setSystemPermission(
            /*needLocationPermission=*/ true,
            /*needNearbyDevicesPermission=*/ false,
            /*needLocationServices=*/ true,
            /*canRequestPermissions=*/ true,
        );
        await checkSystemPermissions(bluetoothInternalsHandlerRemote, () => {
          assert(false);
        });
        await internalsHandler.whenCalled('checkSystemPermissions');
        assertTrue(
            document.getElementById('need-location-permission-and-services-on')
                .open);
        document
            .getElementById(
                'need-location-permission-and-services-on-services-link')
            .click();
        await internalsHandler.whenCalled('requestLocationServices');
        assertFalse(
            document.getElementById('need-location-permission-and-services-on')
                .open);
        assertTrue(document.getElementById('refresh-page').open);
      });

  test(
      'CheckSystemPermissions_need_nearby_devices_permission',
      async function() {
        internalsHandler.setSystemPermission(
            /*needLocationPermission=*/ false,
            /*needNearbyDevicesPermission=*/ true,
            /*needLocationServices=*/ false,
            /*canRequestPermissions=*/ true,
        );
        await checkSystemPermissions(bluetoothInternalsHandlerRemote, () => {
          assert(false);
        });
        await internalsHandler.whenCalled('checkSystemPermissions');
        assertTrue(
            document.getElementById('need-nearby-devices-permission').open);
        document
            .getElementById('need-nearby-devices-permission-permission-link')
            .click();
        await internalsHandler.whenCalled('requestSystemPermissions');
        assertFalse(
            document.getElementById('need-nearby-devices-permission').open);
        assertTrue(document.getElementById('refresh-page').open);
      });

  test('CheckSystemPermissions_can_not_request_permission', async function() {
    internalsHandler.setSystemPermission(
        /*needLocationPermission=*/ false,
        /*needNearbyDevicesPermission=*/ true,
        /*needLocationServices=*/ false,
        /*canRequestPermissions=*/ false,
    );
    await checkSystemPermissions(bluetoothInternalsHandlerRemote, () => {
      assert(false);
    });
    await internalsHandler.whenCalled('checkSystemPermissions');
    assertTrue(document.getElementById('can-not-request-permissions').open);
  });

  // <if expr="chromeos_ash">
  test('Restart system Bluetooth', async function() {
    const getReStartBluetoothBtn = () => {
      return document.querySelector('#restart-bluetooth-btn');
    };

    assert(getReStartBluetoothBtn());
    assertEquals(
        getReStartBluetoothBtn().textContent, 'Restart system Bluetooth');

    getReStartBluetoothBtn().click();
    await flushTasks();
    await internalsHandler.whenCalled('restartSystemBluetooth');

    assertEquals(
        getReStartBluetoothBtn().textContent, 'Restarting system Bluetooth..');

    internalsHandler.completeRestartSystemBluetooth();
    await flushTasks();
    await internalsHandler.whenCalled('completeRestartSystemBluetooth');
    assertEquals(
        getReStartBluetoothBtn().textContent, 'Restart system Bluetooth');
  });
  // </if>
});

suite('BluetoothInternalsUnitTests', function() {
  /* Value Control Unit Tests */
  const aCode = 'a'.charCodeAt(0);
  const bCode = 'b'.charCodeAt(0);
  const cCode = 'c'.charCodeAt(0);

  const device1 = fakeDeviceInfo1();
  const service1 = fakeServiceInfo1();
  const characteristic1 = fakeCharacteristicInfo1();
  let valueControl = null;

  setup(function() {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    valueControl = document.createElement('value-control');
    document.body.appendChild(valueControl);
    valueControl.dataset.options = JSON.stringify({
      deviceAddress: device1.address,
      serviceId: service1.id,
      characteristicId: characteristic1,
    });
    valueControl.shadowRoot.querySelector('select').value =
        ValueDataType.HEXADECIMAL;
  });

  test('ValueControl_SetValue_Hexadecimal_EmptyArray', function() {
    valueControl.dataset.value = JSON.stringify([]);
    assertEquals('', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_Hexadecimal_OneValue', function() {
    valueControl.dataset.value = JSON.stringify([aCode]);
    assertEquals('0x61', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_Hexadecimal_ThreeValues', function() {
    valueControl.dataset.value = JSON.stringify([aCode, bCode, cCode]);
    assertEquals(
        '0x616263', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_UTF8_EmptyArray', function() {
    valueControl.shadowRoot.querySelector('select').value = ValueDataType.UTF8;
    valueControl.dataset.value = JSON.stringify([]);
    assertEquals('', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_UTF8_OneValue', function() {
    valueControl.shadowRoot.querySelector('select').value = ValueDataType.UTF8;
    valueControl.dataset.value = JSON.stringify([aCode]);
    assertEquals('a', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_UTF8_ThreeValues', function() {
    valueControl.shadowRoot.querySelector('select').value = ValueDataType.UTF8;
    valueControl.dataset.value = JSON.stringify([aCode, bCode, cCode]);
    assertEquals('abc', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_Decimal_EmptyArray', function() {
    valueControl.shadowRoot.querySelector('select').value =
        ValueDataType.DECIMAL;
    valueControl.dataset.value = JSON.stringify([]);
    assertEquals('', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_Decimal_OneValue', function() {
    valueControl.shadowRoot.querySelector('select').value =
        ValueDataType.DECIMAL;
    valueControl.dataset.value = JSON.stringify([aCode]);
    assertEquals(
        String(aCode), valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_SetValue_Decimal_ThreeValues', function() {
    valueControl.shadowRoot.querySelector('select').value =
        ValueDataType.DECIMAL;
    valueControl.dataset.value = JSON.stringify([aCode, bCode, cCode]);
    assertEquals(
        '97-98-99', valueControl.shadowRoot.querySelector('input').value);
  });

  test('ValueControl_ConvertValue_Hexadecimal_EmptyString', function() {
    valueControl.value_.setAs(ValueDataType.HEXADECIMAL, '');
    assertEquals(0, valueControl.value_.getArray().length);
  });

  test('ValueControl_ConvertValue_Hexadecimal_BadHexPrefix', function() {
    assertThrows(function() {
      valueControl.value_.setAs(ValueDataType.HEXADECIMAL, 'd0x');
    }, 'Expected new value to start with "0x"');
  });

  test('ValueControl_ConvertValue_Hexadecimal_ThreeValues', function() {
    valueControl.value_.setAs(ValueDataType.HEXADECIMAL, '0x616263');
    assertDeepEquals([aCode, bCode, cCode], valueControl.value_.getArray());
  });

  test('ValueControl_ConvertValue_UTF8_EmptyString', function() {
    valueControl.shadowRoot.querySelector('select').value = ValueDataType.UTF8;
    valueControl.value_.setAs(ValueDataType.UTF8, '');
    assertEquals(0, valueControl.value_.getArray().length);
  });

  test('ValueControl_ConvertValue_UTF8_ThreeValues', function() {
    valueControl.shadowRoot.querySelector('select').value = ValueDataType.UTF8;
    valueControl.value_.setAs(ValueDataType.UTF8, 'abc');
    assertDeepEquals([aCode, bCode, cCode], valueControl.value_.getArray());
  });

  test('ValueControl_ConvertValue_Decimal_EmptyString', function() {
    valueControl.shadowRoot.querySelector('select').value =
        ValueDataType.DECIMAL;
    valueControl.value_.setAs(ValueDataType.DECIMAL, '');
    assertEquals(0, valueControl.value_.getArray().length);
  });

  test('ValueControl_ConvertValue_Decimal_ThreeValues_Fail', function() {
    valueControl.shadowRoot.querySelector('select').value =
        ValueDataType.DECIMAL;

    assertThrows(function() {
      valueControl.value_.setAs(ValueDataType.DECIMAL, '97-+-99' /* a-+-c */);
    }, 'New value can only contain numbers and hyphens');
  });

  test('ValueControl_ConvertValue_Decimal_ThreeValues', function() {
    valueControl.shadowRoot.querySelector('select').value =
        ValueDataType.DECIMAL;
    valueControl.value_.setAs(ValueDataType.DECIMAL, '97-98-99' /* abc */);
    assertDeepEquals([aCode, bCode, cCode], valueControl.value_.getArray());
  });
});
