// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {setSetupFn} from 'chrome://usb-internals/app.js';
import {UsbControlTransferParams, UsbControlTransferRecipient, UsbControlTransferType, UsbDeviceCallbackRouter, UsbDeviceRemote, UsbOpenDeviceError, UsbTransferStatus} from 'chrome://usb-internals/usb_device.mojom-webui.js';
import {UsbInternalsPageHandler, UsbInternalsPageHandlerReceiver, UsbInternalsPageHandlerRemote} from 'chrome://usb-internals/usb_internals.mojom-webui.js';
import {UsbDeviceManagerReceiver, UsbDeviceManagerRemote} from 'chrome://usb-internals/usb_manager.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @implements {UsbInternalsPageHandlerRemote} */
class FakePageHandlerRemote extends TestBrowserProxy {
  constructor(handle) {
    super([
      'bindUsbDeviceManagerInterface',
      'bindTestInterface',
    ]);

    this.receiver_ = new UsbInternalsPageHandlerReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  async bindUsbDeviceManagerInterface(deviceManagerPendingReceiver) {
    this.methodCalled(
        'bindUsbDeviceManagerInterface', deviceManagerPendingReceiver);
    this.deviceManager =
        new FakeDeviceManagerRemote(deviceManagerPendingReceiver);
  }

  async bindTestInterface(testDeviceManagerPendingReceiver) {
    this.methodCalled('bindTestInterface', testDeviceManagerPendingReceiver);
  }
}

/** @implements {UsbDeviceManagerRemote} */
class FakeDeviceManagerRemote extends TestBrowserProxy {
  constructor(pendingReceiver) {
    super([
      'enumerateDevicesAndSetClient',
      'getDevice',
      'getSecurityKeyDevice',
      'getDevices',
      'checkAccess',
      'openFileDescriptor',
      'setClient',
    ]);

    this.receiver_ = new UsbDeviceManagerReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);

    this.devices = [];
    this.deviceRemoteMap = new Map();
    this.addFakeDevice(
        fakeDeviceInfo(0), createDeviceWithValidDeviceDescriptor());
    this.addFakeDevice(
        fakeDeviceInfo(1), createDeviceWithShortDeviceDescriptor());
  }

  /**
   * Adds a fake device to this device manager.
   * @param {!Object} device
   * @param {!FakeUsbDeviceRemote} deviceRemote
   */
  addFakeDevice(device, deviceRemote) {
    this.devices.push(device);
    this.deviceRemoteMap.set(device.guid, deviceRemote);
  }

  async enumerateDevicesAndSetClient() {}

  async getDevice(
      guid, blockedInterfaceClasses, devicePendingReceiver, deviceClient) {
    this.methodCalled('getDevice');
    const deviceRemote = this.deviceRemoteMap.get(guid);
    deviceRemote.router.$.bindHandle(devicePendingReceiver.handle);
  }

  async getSecurityKeyDevice(guid, devicePendingReceiver, deviceClient) {}

  async getDevices() {
    this.methodCalled('getDevices');
    return {results: this.devices};
  }

  async checkAccess() {}

  async openFileDescriptor() {}

  async setClient() {}
}

/** @implements {UsbDeviceRemote} */
class FakeUsbDeviceRemote extends TestBrowserProxy {
  constructor() {
    super([
      'open',
      'close',
      'controlTransferIn',
    ]);
    this.responses = new Map();

    // NOTE: We use the generated CallbackRouter here because
    // device.mojom.UsbDevice defines lots of methods we don't care to mock
    // here. UsbDeviceCallbackRouter callback silently discards messages
    // that have no listeners.
    this.router = new UsbDeviceCallbackRouter();
    this.router.open.addListener(async () => {
      return {error: UsbOpenDeviceError.OK};
    });
    this.router.controlTransferIn.addListener(
        (params, length, timeout) =>
            this.controlTransferIn(params, length, timeout));
    this.router.close.addListener(async () => {});
  }

  async controlTransferIn(params, length, timeout) {
    const response =
        this.responses.get(usbControlTransferParamsToString(params));
    if (!response) {
      return {
        status: UsbTransferStatus.TRANSFER_ERROR,
        data: {buffer: []},
      };
    }
    response.data = {buffer: response.data.slice(0, length)};
    return response;
  }

  /**
   * Set a response for a given request.
   * @param {!UsbControlTransferParams} params
   * @param {!Object} response
   */
  setResponse(params, response) {
    this.responses.set(usbControlTransferParamsToString(params), response);
  }

  /**
   * Set the device descriptor the device will respond to queries with.
   * @param {!Object} response
   */
  setDeviceDescriptor(response) {
    const params = {};
    params.type = UsbControlTransferType.STANDARD;
    params.recipient = UsbControlTransferRecipient.DEVICE;
    params.request = 6;
    params.index = 0;
    params.value = (1 << 8);
    this.setResponse(params, response);
  }
}

/**
 * Creates a fake device using the given number.
 * @param {number} num
 * @return {!Object}
 */
function fakeDeviceInfo(num) {
  return {
    guid: `abcdefgh-ijkl-mnop-qrst-uvwxyz12345${num}`,
    usbVersionMajor: 2,
    usbVersionMinor: 0,
    usbVersionSubminor: num,
    classCode: 0,
    subclassCode: 0,
    protocolCode: 0,
    busNumber: num,
    portNumber: num,
    vendorId: 0x1050 + num,
    productId: 0x17EF + num,
    deviceVersionMajor: 3,
    deviceVersionMinor: 2,
    deviceVersionSubminor: 1,
    manufacturerName: stringToMojoString16('test'),
    productName: undefined,
    serialNumber: undefined,
    webusbLandingPage: {url: 'http://google.com'},
    activeConfiguration: 1,
    configurations: [],
  };
}

/**
 * Creates a device with correct descriptors.
 */
function createDeviceWithValidDeviceDescriptor() {
  const deviceRemote = new FakeUsbDeviceRemote();
  deviceRemote.setDeviceDescriptor({
    status: UsbTransferStatus.COMPLETED,
    data: [
      0x12,
      0x01,
      0x00,
      0x02,
      0x00,
      0x00,
      0x00,
      0x40,
      0x50,
      0x10,
      0xEF,
      0x17,
      0x21,
      0x03,
      0x01,
      0x02,
      0x00,
      0x01,
    ],
  });
  return deviceRemote;
}

/**
 * Creates a device with too short descriptors.
 */
function createDeviceWithShortDeviceDescriptor() {
  const deviceRemote = new FakeUsbDeviceRemote();
  deviceRemote.setDeviceDescriptor({
    status: UsbTransferStatus.SHORT_PACKET,
    data: [0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x50],
  });
  return deviceRemote;
}

/**
 * Converts an ECMAScript string to an instance of mojo_base.mojom.String16.
 * @param {string} string
 * @return {!String16}
 */
function stringToMojoString16(string) {
  return {data: Array.from(string, c => c.charCodeAt(0))};
}

/**
 * Stringify a UsbControlTransferParams type object to be the key of
 * response map.
 * @param {!UsbControlTransferParams} params
 * @return {string}
 */
function usbControlTransferParamsToString(params) {
  return `${params.type}-${params.recipient}-${params.request}-${
      params.value}-${params.index}`;
}

const setupResolver = new PromiseResolver();
let deviceDescriptorRenderPromise =
    eventToPromise('device-descriptor-complete-for-test', document.body);
let pageHandler = null;

const deviceTabInitializedPromise =
    eventToPromise('device-tab-initialized-for-test', document.body);

const deviceManagerGetDevicesPromise =
    eventToPromise('device-list-complete-for-test', document.body);

setSetupFn(() => {
  const pageHandlerInterceptor =
      new MojoInterfaceInterceptor(UsbInternalsPageHandler.$interfaceName);
  pageHandlerInterceptor.oninterfacerequest = (e) => {
    pageHandler = new FakePageHandlerRemote(e.handle);
    setupResolver.resolve();
  };
  pageHandlerInterceptor.start();

  return Promise.resolve();
});


suite('UsbInternalsUITest', function() {
  let app = null;

  suiteSetup(async function() {
    document.body.innerHTML = trustedTypes.emptyHTML;
    app = document.createElement('usb-internals-app');
    document.body.appendChild(app);

    // Before tests are run, make sure setup completes.
    await setupResolver.promise;
    await pageHandler.whenCalled('bindUsbDeviceManagerInterface');
    await pageHandler.deviceManager.whenCalled('getDevices');
  });

  teardown(function() {
    pageHandler.reset();
  });

  test('PageLoaded', async function() {
    const EXPECT_DEVICES_NUM = 2;

    // Totally 2 tables: 'TestDevice' table and 'Device' table.
    const tables = app.shadowRoot.querySelectorAll('table');
    assertEquals(2, tables.length);

    // Only 2 tabs after loading page.
    const tabs = app.shadowRoot.querySelectorAll('div[slot=\'tab\']');
    assertEquals(2, tabs.length);
    const tabPanels = app.shadowRoot.querySelectorAll('div[slot=\'panel\']');
    assertEquals(2, tabPanels.length);

    // The second is the devices table, which has 8 columns.
    const devicesTable = app.shadowRoot.querySelectorAll('table')[1];
    const columns = devicesTable.querySelector('thead')
                        .querySelector('tr')
                        .querySelectorAll('th');
    assertEquals(8, columns.length);

    await deviceManagerGetDevicesPromise;
    const devices = devicesTable.querySelectorAll('tbody tr');
    assertEquals(EXPECT_DEVICES_NUM, devices.length);
  });

  test('DeviceTabAdded', function() {
    const devicesTable = app.$('#device-list');
    // Click the inspect button to open information about the first device.
    // The device info is opened as a third tab panel.
    devicesTable.querySelectorAll('button')[0].click();
    assertEquals(
        3, app.shadowRoot.querySelectorAll('div[slot=\'tab\']').length);
    let panels = app.shadowRoot.querySelectorAll('div[slot=\'panel\']');
    assertEquals(3, panels.length);
    assertTrue(panels[2].hasAttribute('selected'));

    // Check that clicking the inspect button for another device will open a
    // new tabpanel.
    devicesTable.querySelectorAll('button')[1].click();
    assertEquals(
        4, app.shadowRoot.querySelectorAll('div[slot=\'tab\']').length);
    panels = app.shadowRoot.querySelectorAll('div[slot=\'panel\']');
    assertEquals(4, panels.length);
    assertTrue(panels[3].hasAttribute('selected'));
    assertFalse(panels[2].hasAttribute('selected'));

    // Check that clicking the inspect button for the same device a second
    // time will open the same tabpanel.
    devicesTable.querySelectorAll('button')[0].click();
    assertEquals(
        4, app.shadowRoot.querySelectorAll('div[slot=\'tab\']').length);
    panels = app.shadowRoot.querySelectorAll('div[slot=\'panel\']');
    assertEquals(4, panels.length);
    assertTrue(panels[2].hasAttribute('selected'));
    assertFalse(panels[3].hasAttribute('selected'));
  });

  test('RenderDeviceInfoTree', function() {
    // Test the tab opened by clicking inspect button contains a tree view
    // showing WebUSB information. Check the tree displays correct data.
    // The tab panel of the first device is opened in previous test as the
    // third tab panel.
    const deviceTab = app.shadowRoot.querySelectorAll('div[slot=\'panel\']')[2];
    const tree = deviceTab.querySelector('cr-tree');
    const treeItems = tree.items;
    assertEquals(11, treeItems.length);

    const labels = [
      'USB Version: 2.0.0',
      'Class Code: 0 (Device)',
      'Subclass Code: 0',
      'Protocol Code: 0',
      'Port Number: 0',
      'Vendor Id: 0x1050',
      'Product Id: 0x17EF',
      'Device Version: 3.2.1',
      'Manufacturer Name: test',
      'WebUSB Landing Page: http://google.com',
      'Active Configuration: 1',
    ];
    labels.forEach((label, i) => {
      assertEquals(label, treeItems[i].labelElement.textContent);
    });
  });

  test('RenderDeviceDescriptor', async function() {
    // Test the tab opened by clicking inspect button contains a panel that
    // can manually retrieve device descriptor from device. Check the response
    // can be rendered correctly.
    await deviceTabInitializedPromise;
    // The tab panel of the first device is opened in previous test as the
    // third tab panel. This device has correct device descriptor.
    const deviceTab = app.shadowRoot.querySelectorAll('div[slot=\'panel\']')[2];
    deviceTab.querySelector('.device-descriptor-button').click();

    await deviceDescriptorRenderPromise;
    const panel = deviceTab.querySelector('.device-descriptor-panel');
    assertEquals(1, panel.querySelectorAll('descriptorpanel').length);
    assertEquals(0, panel.querySelectorAll('error').length);
    const treeItems = panel.querySelector('cr-tree').items;
    assertEquals(14, treeItems.length);

    const labels = [
      'Length (should be 18): 18',
      'Descriptor Type (should be 0x01): 0x01',
      'USB Version: 2.0.0',
      'Class Code: 0 (Device)',
      'Subclass Code: 0',
      'Protocol Code: 0',
      'Control Pipe Maximum Packet Size: 64',
      'Vendor ID: 0x1050',
      'Product ID: 0x17EF',
      'Device Version: 3.2.1',
      'Manufacturer String Index: 1GET',
      'Product String Index: 2GET',
      'Serial Number Index: 0',
      'Number of Configurations: 1',
    ];
    labels.forEach((label, i) => {
      assertEquals(label, treeItems[i].labelElement.textContent);
    });

    const byteElements = panel.querySelectorAll('.raw-data-byte-view span');
    assertEquals(18, byteElements.length);
    assertEquals(
        '12010002000000405010EF17210301020001',
        panel.querySelector('.raw-data-byte-view').textContent);

    // Click a single byte tree item (Length) and check that both the item
    // and the related byte are highlighted.
    treeItems[0].rowElement.click();
    assertTrue(treeItems[0].hasAttribute('selected'));
    assertTrue(byteElements[0].classList.contains('selected-field'));
    // Click a multi-byte tree item (Vendor ID) and check that both the
    // item and the related bytes are highlighted, and other items and bytes
    // are not highlighted.
    treeItems[7].rowElement.click();
    assertFalse(treeItems[0].hasAttribute('selected'));
    assertTrue(treeItems[7].hasAttribute('selected'));
    assertFalse(byteElements[0].classList.contains('selected-field'));
    assertTrue(byteElements[8].classList.contains('selected-field'));
    assertTrue(byteElements[9].classList.contains('selected-field'));
    // Click a single byte element (Descriptor Type) and check that both the
    // byte and the related item are highlighted, and other items and bytes
    // are not highlighted.
    byteElements[1].click();
    assertFalse(treeItems[7].hasAttribute('selected'));
    assertTrue(treeItems[1].hasAttribute('selected'));
    assertTrue(byteElements[1].classList.contains('selected-field'));
    // Click any byte element of a multi-byte element (Product ID) and check
    // that both the bytes and the related item are highlighted, and other
    // items and bytes are not highlighted.
    byteElements[11].click();
    assertFalse(treeItems[1].hasAttribute('selected'));
    assertTrue(treeItems[8].hasAttribute('selected'));
    assertTrue(byteElements[10].classList.contains('selected-field'));
    assertTrue(byteElements[11].classList.contains('selected-field'));
  });

  test('RenderShortDeviceDescriptor', async function() {
    await deviceManagerGetDevicesPromise;
    const devicesTable = app.$('#device-list');
    // Inspect the second device, which has short device descriptor.
    devicesTable.querySelectorAll('button')[1].click();
    // The fourth is the device tab (a third tab was opened in a previous test).
    const deviceTab = app.shadowRoot.querySelectorAll('div[slot=\'panel\']')[3];

    await deviceTabInitializedPromise;
    deviceDescriptorRenderPromise =
        eventToPromise('device-descriptor-complete-for-test', document.body);
    deviceTab.querySelector('.device-descriptor-button').click();

    await deviceDescriptorRenderPromise;
    const panel = deviceTab.querySelector('.device-descriptor-panel');

    assertEquals(1, panel.querySelectorAll('descriptorpanel').length);
    const errors = panel.querySelectorAll('error');
    assertEquals(2, errors.length);
    assertEquals('Field at offset 8 is invalid.', errors[0].textContent);
    assertEquals('Descriptor is too short.', errors[1].textContent);
    // For the short response, the returned data should still be rendered.
    const treeItems = panel.querySelector('cr-tree').items;
    assertEquals(7, treeItems.length);

    const labels = [
      'Length (should be 18): 18',
      'Descriptor Type (should be 0x01): 0x01',
      'USB Version: 2.0.0',
      'Class Code: 0 (Device)',
      'Subclass Code: 0',
      'Protocol Code: 0',
      'Control Pipe Maximum Packet Size: 64',
    ];
    labels.forEach((label, i) => {
      assertEquals(label, treeItems[i].labelElement.textContent);
    });

    const byteElements = panel.querySelectorAll('.raw-data-byte-view span');
    assertEquals(9, byteElements.length);
    assertEquals(
        '120100020000004050',
        panel.querySelector('.raw-data-byte-view').textContent);


    // Click a single byte tree item (Length) and check that both the item
    // and the related byte are highlighted.
    treeItems[0].rowElement.click();
    assertTrue(treeItems[0].hasAttribute('selected'));
    assertTrue(byteElements[0].classList.contains('selected-field'));
    // Click a multi-byte tree item (USB Version) and check that both the
    // item and the related bytes are highlighted, and other items and bytes
    // are not highlighted.
    treeItems[2].rowElement.click();
    assertFalse(treeItems[0].hasAttribute('selected'));
    assertTrue(treeItems[2].hasAttribute('selected'));
    assertFalse(byteElements[0].classList.contains('selected-field'));
    assertTrue(byteElements[2].classList.contains('selected-field'));
    assertTrue(byteElements[3].classList.contains('selected-field'));
    // Click a single byte element (Descriptor Type) and check that both the
    // byte and the related item are highlighted, and other items and bytes
    // are not highlighted.
    byteElements[1].click();
    assertFalse(treeItems[2].hasAttribute('selected'));
    assertTrue(treeItems[1].hasAttribute('selected'));
    assertTrue(byteElements[1].classList.contains('selected-field'));
    // Click any byte element of a multi-byte element (USB Version) and
    // check that both the bytes and the related item are highlighted, and
    // other items and bytes are not highlighted.
    byteElements[3].click();
    assertFalse(treeItems[1].hasAttribute('selected'));
    assertTrue(treeItems[2].hasAttribute('selected'));
    assertTrue(byteElements[2].classList.contains('selected-field'));
    assertTrue(byteElements[3].classList.contains('selected-field'));
    // Click the invalid field's byte (Vendor ID) will do nothing, check the
    // highlighted item and bytes are not changed.
    byteElements[8].click();
    assertTrue(treeItems[2].hasAttribute('selected'));
    assertTrue(byteElements[2].classList.contains('selected-field'));
    assertTrue(byteElements[3].classList.contains('selected-field'));
    assertFalse(byteElements[8].classList.contains('selected-field'));
  });
});
