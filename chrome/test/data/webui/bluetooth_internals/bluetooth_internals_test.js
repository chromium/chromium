// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {adapterBroker, devices, initializeViews, pageManager, sidebarObj} from 'chrome://bluetooth-internals/bluetooth_internals.js';
import {connectedDevices} from 'chrome://bluetooth-internals/device_broker.js';
import {Snackbar} from 'chrome://bluetooth-internals/snackbar.js';
import {ValueControl, ValueDataType} from 'chrome://bluetooth-internals/value_control.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {fakeAdapterInfo, fakeCharacteristicInfo1, fakeDeviceInfo1, fakeDeviceInfo2, fakeDeviceInfo3, fakeServiceInfo1, fakeServiceInfo2, TestAdapter, TestBluetoothInternalsHandler, TestDevice} from './test_utils.js';

suite('bluetooth_internals', function() {
  const whenSetupDone = new PromiseResolver();
  let internalsHandler = null;
  let adapterFieldSet = null;
  let deviceTable = null;
  let sidebarNode = null;
  let pageNames = ['adapter', 'devices'];
  const EXPECTED_DEVICES = 2;

  suiteSetup(async function() {
    let internalsHandlerInterceptor = new MojoInterfaceInterceptor(
        mojom.BluetoothInternalsHandler.$interfaceName);
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
    initializeViews();
    await whenSetupDone.promise;
    await Promise.all([
      internalsHandler.whenCalled('getAdapter'),
      internalsHandler.adapter.whenCalled('getInfo'),
      internalsHandler.adapter.whenCalled('getDevices'),
      internalsHandler.adapter.whenCalled('addObserver')
    ]);
  });

  setup(function() {
    adapterFieldSet = document.querySelector('#adapter fieldset');
    deviceTable = document.querySelector('#devices table');
    sidebarNode = document.querySelector('#sidebar');
    devices.splice(0, devices.length);
    adapterBroker.deviceAdded(fakeDeviceInfo1());
    adapterBroker.deviceAdded(fakeDeviceInfo2());
  });

  teardown(function() {
    internalsHandler.reset();
    sidebarObj.close();
    Snackbar.dismiss(true);
    connectedDevices.clear();

    pageManager.registeredPages.get('adapter').setAdapterInfo(
        fakeAdapterInfo());

    for (const pageName of pageManager.registeredPages.keys()) {
      var page = pageManager.registeredPages.get(pageName);

      if (pageNames.indexOf(pageName) < 0) {
        page.pageDiv.parentNode.removeChild(page.pageDiv);
        pageManager.unregister(page);
      }
    }
  });

  /**
   * Updates device info and verifies the contents of the device table.
   * @param {!DeviceInfo} deviceInfo
   */
  function changeDevice(deviceInfo) {
    var deviceRow = deviceTable.querySelector(
        '#' + escapeDeviceAddress(deviceInfo.address));
    var nameForDisplayColumn = deviceRow.children[0];
    var addressColumn = deviceRow.children[1];
    var rssiColumn = deviceRow.children[2];
    var servicesColumn = deviceRow.children[3];

    expectTrue(!!nameForDisplayColumn);
    expectTrue(!!addressColumn);
    expectTrue(!!rssiColumn);
    expectTrue(!!servicesColumn);

    adapterBroker.deviceChanged(deviceInfo);

    expectEquals(deviceInfo.nameForDisplay, nameForDisplayColumn.textContent);
    expectEquals(deviceInfo.address, addressColumn.textContent);

    if (deviceInfo.rssi) {
      expectEquals(String(deviceInfo.rssi.value), rssiColumn.textContent);
    }

    if (deviceInfo.services) {
      expectEquals(
          String(deviceInfo.services.length), servicesColumn.textContent);
    } else {
      expectEquals('Unknown', servicesColumn.textContent);
    }
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
    var removedRow =
        deviceTable.querySelector('#' + escapeDeviceAddress(address));

    expectEquals(expectRemoved, removedRow.classList.contains('removed'));
  }

  /**
   * Tests whether a device is added successfully and not duplicated.
   */
  test('DeviceAdded', function() {
    var devices = deviceTable.querySelectorAll('tbody tr');
    expectEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    var infoCopy = fakeDeviceInfo3();
    adapterBroker.deviceAdded(infoCopy);

    // Same device shouldn't appear twice.
    adapterBroker.deviceAdded(infoCopy);

    devices = deviceTable.querySelectorAll('tbody tr');
    expectEquals(EXPECTED_DEVICES + 1, devices.length);
  });

  /**
   * Tests whether a device is marked properly as removed.
   */
  test('DeviceSetToRemoved', function() {
    var devices = deviceTable.querySelectorAll('tbody tr');
    expectEquals(EXPECTED_DEVICES, devices.length);

    var fakeDevice = fakeDeviceInfo2();
    adapterBroker.deviceRemoved(fakeDevice);

    // The number of rows shouldn't change.
    devices = deviceTable.querySelectorAll('tbody tr');
    expectEquals(EXPECTED_DEVICES, devices.length);

    expectDeviceRemoved(fakeDevice.address, true);
  });

  /**
   * Tests whether a changed device updates the device table properly.
   */
  test('DeviceChanged', function() {
    var devices = deviceTable.querySelectorAll('tbody tr');
    expectEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    var newDeviceInfo = fakeDeviceInfo1();
    newDeviceInfo.nameForDisplay = 'DDDD';
    newDeviceInfo.rssi = {value: -20};
    newDeviceInfo.services = ['service1', 'service2', 'service3'];

    changeDevice(newDeviceInfo);
  });

  /**
   * Tests the entire device cycle, added -> updated -> removed -> re-added.
   */
  test('DeviceUpdateCycle', function() {
    var devices = deviceTable.querySelectorAll('tbody tr');
    expectEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    var originalDeviceInfo = fakeDeviceInfo3();
    adapterBroker.deviceAdded(originalDeviceInfo);

    var newDeviceInfo = fakeDeviceInfo3();
    newDeviceInfo.nameForDisplay = 'DDDD';
    newDeviceInfo.rssi = {value: -20};
    newDeviceInfo.services = ['service1', 'service2', 'service3'];

    changeDevice(newDeviceInfo);
    changeDevice(originalDeviceInfo);

    adapterBroker.deviceRemoved(originalDeviceInfo);
    expectDeviceRemoved(originalDeviceInfo.address, true);

    adapterBroker.deviceAdded(originalDeviceInfo);
    expectDeviceRemoved(originalDeviceInfo.address, false);
  });

  test('DeviceAddedRssiCheck', function() {
    var devices = deviceTable.querySelectorAll('tbody tr');
    expectEquals(EXPECTED_DEVICES, devices.length);

    // Copy device info because device collection will not copy this object.
    var newDeviceInfo = fakeDeviceInfo3();
    adapterBroker.deviceAdded(newDeviceInfo);

    var deviceRow = deviceTable.querySelector(
        '#' + escapeDeviceAddress(newDeviceInfo.address));
    var rssiColumn = deviceRow.children[2];
    expectEquals('Unknown', rssiColumn.textContent);

    var newDeviceInfo1 = fakeDeviceInfo3();
    newDeviceInfo1.rssi = {value: -42};
    adapterBroker.deviceChanged(newDeviceInfo1);
    expectEquals('-42', rssiColumn.textContent);

    // Device table should keep last valid rssi value.
    var newDeviceInfo2 = fakeDeviceInfo3();
    newDeviceInfo2.rssi = null;
    adapterBroker.deviceChanged(newDeviceInfo2);
    expectEquals('-42', rssiColumn.textContent);

    var newDeviceInfo3 = fakeDeviceInfo3();
    newDeviceInfo3.rssi = {value: -17};
    adapterBroker.deviceChanged(newDeviceInfo3);
    expectEquals('-17', rssiColumn.textContent);
  });

  /* Sidebar Tests */
  test('Sidebar_Setup', function() {
    var sidebarItems =
        Array.from(sidebarNode.querySelectorAll('.sidebar-content li'));

    pageNames.forEach(function(pageName) {
      expectTrue(sidebarItems.some(function(item) {
        return item.dataset.pageName === pageName;
      }));
    });
  });

  test('Sidebar_DefaultState', function() {
    // Sidebar should be closed by default.
    expectFalse(sidebarNode.classList.contains('open'));
  });

  test('Sidebar_OpenClose', function() {
    sidebarObj.open();
    expectTrue(sidebarNode.classList.contains('open'));
    sidebarObj.close();
    expectFalse(sidebarNode.classList.contains('open'));
  });

  test('Sidebar_OpenTwice', function() {
    // Multiple calls to open shouldn't change the state.
    sidebarObj.open();
    sidebarObj.open();
    expectTrue(sidebarNode.classList.contains('open'));
  });

  test('Sidebar_CloseTwice', function() {
    // Multiple calls to close shouldn't change the state.
    sidebarObj.close();
    sidebarObj.close();
    expectFalse(sidebarNode.classList.contains('open'));
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
        expectEquals(0, $('snackbar-container').children.length);
        expectFalse(!!Snackbar.current_);
        resolve();
      }, 50);
    });
  }

  test('Snackbar_ShowTimeout', function(done) {
    var snackbar1 = Snackbar.show('Message 1');
    assertEquals(1, $('snackbar-container').children.length);

    snackbar1.addEventListener('dismissed', function() {
      finishSnackbarTest().then(done);
    });
  });

  test('Snackbar_ShowDismiss', function() {
    var snackbar1 = Snackbar.show('Message 1');
    assertEquals(1, $('snackbar-container').children.length);

    return whenSnackbarShows(snackbar1)
        .then(function() {
          return Snackbar.dismiss();
        })
        .then(finishSnackbarTest);
  });

  test('Snackbar_QueueThreeDismiss', function() {
    var expectedCalls = 3;
    var actualCalls = 0;

    var snackbar1 = Snackbar.show('Message 1');
    var snackbar2 = Snackbar.show('Message 2');
    var snackbar3 = Snackbar.show('Message 3');

    assertEquals(1, $('snackbar-container').children.length);
    expectEquals(2, Snackbar.queue_.length);

    function next() {
      actualCalls++;
      return Snackbar.dismiss();
    }

    whenSnackbarShows(snackbar1).then(next);
    whenSnackbarShows(snackbar2).then(next);
    return whenSnackbarShows(snackbar3)
        .then(next)
        .then(function() {
          expectEquals(expectedCalls, actualCalls);
        })
        .then(finishSnackbarTest);
  });

  test('Snackbar_QueueThreeDismissAll', function() {
    var expectedCalls = 1;
    var actualCalls = 0;

    var snackbar1 = Snackbar.show('Message 1');
    var snackbar2 = Snackbar.show('Message 2');
    var snackbar3 = Snackbar.show('Message 3');

    assertEquals(1, $('snackbar-container').children.length);
    expectEquals(2, Snackbar.queue_.length);

    function next() {
      assertTrue(false);
    }

    whenSnackbarShows(snackbar2).then(next);
    snackbar2.addEventListener('dismissed', next);
    whenSnackbarShows(snackbar3).then(next);
    snackbar3.addEventListener('dismissed', next);

    return whenSnackbarShows(snackbar1)
        .then(function() {
          return Snackbar.dismiss(true);
        })
        .then(function() {
          expectEquals(0, Snackbar.queue_.length);
          expectFalse(!!Snackbar.current_);
        })
        .then(finishSnackbarTest);
  });

  /* AdapterPage Tests */
  function checkAdapterFieldSet(adapterInfo) {
    for (var propName in adapterInfo) {
      var valueCell =
          adapterFieldSet.querySelector('[data-field="' + propName + '"]');
      var value = adapterInfo[propName];

      if (typeof (value) === 'boolean') {
        expectEquals(value, valueCell.classList.contains('checked'));
      } else if (typeof (value) === 'string') {
        expectEquals(value, valueCell.textContent);
      } else {
        assert('boolean or string type expected but got ' + typeof (value));
      }
    }
  }

  test('AdapterPage_DefaultState', function() {
    checkAdapterFieldSet(adapterFieldSet.value);
  });

  test('AdapterPage_AdapterChanged', function() {
    var adapterInfo = adapterFieldSet.value;

    adapterInfo.present = !adapterInfo.present;
    adapterBroker.presentChanged(adapterInfo.present);
    checkAdapterFieldSet(adapterInfo);

    adapterInfo.discovering = !adapterInfo.discovering;
    adapterBroker.discoveringChanged(adapterInfo.discovering);
    checkAdapterFieldSet(adapterInfo);
  });

  test('AdapterPage_AdapterChanged_RepeatTwice', function() {
    var adapterInfo = adapterFieldSet.value;

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
     'services.length',
    ].forEach(function(propName) {
      var valueCell =
          detailsPage.querySelector('fieldset [data-field="' + propName + '"]');

      var parts = propName.split('.');
      var value = deviceInfo;

      while (value != null && parts.length > 0) {
        var part = parts.shift();
        value = value[part];
      }

      if (propName === 'isGattConnected') {
        value = value ? 'Connected' : 'Not Connected';
      }

      if (typeof (value) === 'boolean') {
        expectEquals(value, valueCell.classList.contains('checked'));
      } else if (typeof (value) === 'string') {
        expectEquals(value, valueCell.textContent);
      } else {
        assert('boolean or string type expected but got ' + typeof (value));
      }
    });
  }

  test('DeviceDetailsPage_NewDelete', function() {
    var device = devices.item(0);

    var deviceInspectLink =
        $(device.address).querySelector('[is="action-link"]');

    var deviceDetailsPageId = 'devices/' + device.address.toLowerCase();

    deviceInspectLink.click();
    expectEquals('#' + deviceDetailsPageId, window.location.hash);

    var detailsPage = $(deviceDetailsPageId);
    assertTrue(!!detailsPage);

    return internalsHandler.adapter.deviceImplMap.get(device.address)
        .whenCalled('getServices')
        .then(function() {
          // At this point, the device details page should be fully loaded.
          checkDeviceDetailsFieldSet(detailsPage, device);

          detailsPage.querySelector('.forget').click();
          expectEquals('#devices', window.location.hash);
          detailsPage = $(deviceDetailsPageId);
          expectFalse(!!detailsPage);
        });
  });

  test('DeviceDetailsPage_NewDelete_FromDevicesPage', function() {
    var device = devices.item(0);
    var deviceDetailsPageId = 'devices/' + device.address.toLowerCase();

    var deviceLinks = $(device.address).querySelectorAll('[is="action-link"]');

    // First link is 'Inspect'.
    deviceLinks[0].click();
    expectEquals('#' + deviceDetailsPageId, window.location.hash);

    var detailsPage = $(deviceDetailsPageId);
    assertTrue(!!detailsPage);

    return internalsHandler.adapter.deviceImplMap.get(device.address)
        .whenCalled('getServices')
        .then(function() {
          // At this point, the device details page should be fully loaded.
          checkDeviceDetailsFieldSet(detailsPage, device);

          // Second link is 'Forget'.
          deviceLinks[1].click();
          expectEquals('#devices', window.location.hash);
          detailsPage = $(deviceDetailsPageId);
          expectFalse(!!detailsPage);
        });
  });
});

suite('BluetoothInternalsUnitTests', function() {
  /* Value Control Unit Tests */
  var aCode = 'a'.charCodeAt(0);
  var bCode = 'b'.charCodeAt(0);
  var cCode = 'c'.charCodeAt(0);

  var device1 = fakeDeviceInfo1();
  var service1 = fakeServiceInfo1();
  var characteristic1 = fakeCharacteristicInfo1();
  var valueControl = null;

  setup(function() {
    valueControl = new ValueControl();
    valueControl.load(device1.address, service1.id, characteristic1);
    valueControl.typeSelect_.value = ValueDataType.HEXADECIMAL;
  });

  test('ValueControl_SetValue_Hexadecimal_EmptyArray', function() {
    valueControl.setValue([]);
    expectEquals('', valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_Hexadecimal_OneValue', function() {
    valueControl.setValue([aCode]);
    expectEquals('0x61', valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_Hexadecimal_ThreeValues', function() {
    valueControl.setValue([aCode, bCode, cCode]);
    expectEquals('0x616263', valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_UTF8_EmptyArray', function() {
    valueControl.typeSelect_.value = ValueDataType.UTF8;
    valueControl.setValue([]);
    expectEquals('', valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_UTF8_OneValue', function() {
    valueControl.typeSelect_.value = ValueDataType.UTF8;
    valueControl.setValue([aCode]);
    expectEquals('a', valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_UTF8_ThreeValues', function() {
    valueControl.typeSelect_.value = ValueDataType.UTF8;
    valueControl.setValue([aCode, bCode, cCode]);
    expectEquals('abc', valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_Decimal_EmptyArray', function() {
    valueControl.typeSelect_.value = ValueDataType.DECIMAL;
    valueControl.setValue([]);
    expectEquals('', valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_Decimal_OneValue', function() {
    valueControl.typeSelect_.value = ValueDataType.DECIMAL;
    valueControl.setValue([aCode]);
    expectEquals(String(aCode), valueControl.valueInput_.value);
  });

  test('ValueControl_SetValue_Decimal_ThreeValues', function() {
    valueControl.typeSelect_.value = ValueDataType.DECIMAL;
    valueControl.setValue([aCode, bCode, cCode]);
    expectEquals('97-98-99', valueControl.valueInput_.value);
  });

  test('ValueControl_ConvertValue_Hexadecimal_EmptyString', function() {
    valueControl.value_.setAs(ValueDataType.HEXADECIMAL, '');
    expectEquals(0, valueControl.value_.getArray().length);
  });

  test('ValueControl_ConvertValue_Hexadecimal_BadHexPrefix', function() {
    expectThrows(function() {
      valueControl.value_.setAs(ValueDataType.HEXADECIMAL, 'd0x');
    }, 'Expected new value to start with "0x"');
  });

  test('ValueControl_ConvertValue_Hexadecimal_ThreeValues', function() {
    valueControl.value_.setAs(ValueDataType.HEXADECIMAL, '0x616263');
    expectDeepEquals([aCode, bCode, cCode], valueControl.value_.getArray());
  });

  test('ValueControl_ConvertValue_UTF8_EmptyString', function() {
    valueControl.typeSelect_.value = ValueDataType.UTF8;
    valueControl.value_.setAs(ValueDataType.UTF8, '');
    expectEquals(0, valueControl.value_.getArray().length);
  });

  test('ValueControl_ConvertValue_UTF8_ThreeValues', function() {
    valueControl.typeSelect_.value = ValueDataType.UTF8;
    valueControl.value_.setAs(ValueDataType.UTF8, 'abc');
    expectDeepEquals([aCode, bCode, cCode], valueControl.value_.getArray());
  });

  test('ValueControl_ConvertValue_Decimal_EmptyString', function() {
    valueControl.typeSelect_.value = ValueDataType.DECIMAL;
    valueControl.value_.setAs(ValueDataType.DECIMAL, '');
    expectEquals(0, valueControl.value_.getArray().length);
  });

  test('ValueControl_ConvertValue_Decimal_ThreeValues_Fail', function() {
    valueControl.typeSelect_.value = ValueDataType.DECIMAL;

    expectThrows(function() {
      valueControl.value_.setAs(ValueDataType.DECIMAL, '97-+-99' /* a-+-c */);
    }, 'New value can only contain numbers and hyphens');
  });

  test('ValueControl_ConvertValue_Decimal_ThreeValues', function() {
    valueControl.typeSelect_.value = ValueDataType.DECIMAL;
    valueControl.value_.setAs(ValueDataType.DECIMAL, '97-98-99' /* abc */);
    expectDeepEquals([aCode, bCode, cCode], valueControl.value_.getArray());
  });
});
