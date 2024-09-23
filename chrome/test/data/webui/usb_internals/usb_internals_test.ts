// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';

import type {CrTreeItemElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {File} from 'chrome://resources/mojo/mojo/public/mojom/base/file.mojom-webui.js';
import type {ReadOnlyBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/read_only_buffer.mojom-webui.js';
import type {UsbInternalsAppElement} from 'chrome://usb-internals/app.js';
import {setSetupFn} from 'chrome://usb-internals/app.js';
import type {UsbClaimInterfaceResult, UsbControlTransferParams, UsbDeviceClientRemote, UsbDeviceInfo, UsbDeviceInterface, UsbDevicePendingReceiver, UsbIsochronousPacket, UsbOpenDeviceResult, UsbTransferDirection} from 'chrome://usb-internals/usb_device.mojom-webui.js';
import {UsbControlTransferRecipient, UsbControlTransferType, UsbDeviceReceiver, UsbOpenDeviceSuccess, UsbTransferStatus} from 'chrome://usb-internals/usb_device.mojom-webui.js';
import type {UsbInternalsPageHandlerInterface} from 'chrome://usb-internals/usb_internals.mojom-webui.js';
import {UsbInternalsPageHandler, UsbInternalsPageHandlerReceiver} from 'chrome://usb-internals/usb_internals.mojom-webui.js';
import type {UsbDeviceManagerInterface, UsbDeviceManagerPendingReceiver} from 'chrome://usb-internals/usb_manager.mojom-webui.js';
import {UsbDeviceManagerReceiver} from 'chrome://usb-internals/usb_manager.mojom-webui.js';
import type {UsbDeviceManagerTestPendingReceiver} from 'chrome://usb-internals/usb_manager_test.mojom-webui.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

class FakePageHandlerRemote extends TestBrowserProxy implements
    UsbInternalsPageHandlerInterface {
  private receiver_: UsbInternalsPageHandlerReceiver;
  deviceManager: FakeDeviceManagerRemote|null = null;

  constructor(handle: MojoHandle) {
    super([
      'bindUsbDeviceManagerInterface',
      'bindTestInterface',
    ]);

    this.receiver_ = new UsbInternalsPageHandlerReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  bindUsbDeviceManagerInterface(deviceManagerPendingReceiver:
                                    UsbDeviceManagerPendingReceiver) {
    this.methodCalled(
        'bindUsbDeviceManagerInterface', deviceManagerPendingReceiver);
    this.deviceManager =
        new FakeDeviceManagerRemote(deviceManagerPendingReceiver);
  }

  bindTestInterface(testDeviceManagerPendingReceiver:
                        UsbDeviceManagerTestPendingReceiver) {
    this.methodCalled('bindTestInterface', testDeviceManagerPendingReceiver);
  }
}

class FakeDeviceManagerRemote extends TestBrowserProxy implements
    UsbDeviceManagerInterface {
  private receiver_: UsbDeviceManagerReceiver;
  devices: UsbDeviceInfo[];
  deviceRemoteMap: Map<string, FakeUsbDeviceRemote>;

  constructor(pendingReceiver: UsbDeviceManagerPendingReceiver) {
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
   */
  addFakeDevice(device: UsbDeviceInfo, deviceRemote: FakeUsbDeviceRemote) {
    this.devices.push(device);
    this.deviceRemoteMap.set(device.guid, deviceRemote);
  }

  enumerateDevicesAndSetClient(_client: object):
      Promise<{results: UsbDeviceInfo[]}> {
    assertNotReached();
  }

  getDevice(
      guid: string, _blockedInterfaceClasses: number[],
      devicePendingReceiver: UsbDevicePendingReceiver,
      _deviceClient: UsbDeviceClientRemote|null) {
    this.methodCalled('getDevice');
    const deviceRemote = this.deviceRemoteMap.get(guid);
    assertTrue(!!deviceRemote);
    deviceRemote.receiver.$.bindHandle(devicePendingReceiver.handle);
  }

  getSecurityKeyDevice(
      _guid: string, _devicePendingReceiver: UsbDevicePendingReceiver,
      _deviceClient: UsbDeviceClientRemote|null) {}

  getDevices(): Promise<{results: UsbDeviceInfo[]}> {
    this.methodCalled('getDevices');
    return Promise.resolve({results: this.devices});
  }

  checkAccess(_guid: string): Promise<{success: boolean}> {
    assertNotReached();
  }

  openFileDescriptor(
      _guid: string, _allowedInterfacesMask: number,
      _lifelineFd: MojoHandle): Promise<{fd: File | null}> {
    assertNotReached();
  }

  async setClient() {}
}

class FakeUsbDeviceRemote extends TestBrowserProxy implements
    UsbDeviceInterface {
  responses = new Map<string, any>();
  receiver: UsbDeviceReceiver;

  constructor() {
    super([
      'open',
      'close',
      'controlTransferIn',
    ]);

    this.receiver = new UsbDeviceReceiver(this);
  }

  async controlTransferIn(
      params: UsbControlTransferParams, length: number, _timeout: number):
      Promise<{status: UsbTransferStatus, data: ReadOnlyBuffer}> {
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
   */
  setResponse(params: UsbControlTransferParams, response: any) {
    this.responses.set(usbControlTransferParamsToString(params), response);
  }

  /**
   * Set the device descriptor the device will respond to queries with.
   */
  setDeviceDescriptor(response: any) {
    const params: UsbControlTransferParams = {
      type: UsbControlTransferType.STANDARD,
      recipient: UsbControlTransferRecipient.DEVICE,
      request: 6,
      index: 0,
      value: (1 << 8),
    };
    this.setResponse(params, response);
  }

  setInterfaceAlternateSetting(
      _interfaceNumber: number,
      _alternateSetting: number): Promise<{success: boolean}> {
    assertNotReached();
  }

  clearHalt(_direction: UsbTransferDirection, _endpointNumber: number):
      Promise<{success: boolean}> {
    assertNotReached();
  }

  controlTransferOut(
      _params: UsbControlTransferParams, _data: ReadOnlyBuffer,
      _timeout: number): Promise<{status: UsbTransferStatus}> {
    assertNotReached();
  }

  setConfiguration(_value: number): Promise<{success: boolean}> {
    assertNotReached();
  }

  claimInterface(_interfaceNumber: number):
      Promise<{result: UsbClaimInterfaceResult}> {
    assertNotReached();
  }

  releaseInterface(_interfaceNumber: number): Promise<{success: boolean}> {
    assertNotReached();
  }

  open(): Promise<{result: UsbOpenDeviceResult}> {
    return Promise.resolve({result: {success: UsbOpenDeviceSuccess.OK}});
  }

  genericTransferIn(_endpointNumber: number, _length: number, _timeout: number):
      Promise<{status: UsbTransferStatus, data: ReadOnlyBuffer}> {
    assertNotReached();
  }

  genericTransferOut(
      _endpointNumber: number, _data: ReadOnlyBuffer,
      _timeout: number): Promise<{status: UsbTransferStatus}> {
    assertNotReached();
  }

  isochronousTransferIn(
      _endpointNumber: number, _packetLengths: number[], _timeout: number):
      Promise<{data: ReadOnlyBuffer, packets: UsbIsochronousPacket[]}> {
    assertNotReached();
  }

  isochronousTransferOut(
      _endpointNumber: number, _data: ReadOnlyBuffer, _packetLengths: number[],
      _timeout: number): Promise<{packets: UsbIsochronousPacket[]}> {
    assertNotReached();
  }

  async close() {}

  override reset(): Promise<{success: boolean}> {
    assertNotReached();
  }
}

/**
 * Creates a fake device using the given number.
 */
function fakeDeviceInfo(num: number): UsbDeviceInfo {
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
    productName: null,
    serialNumber: null,
    webusbLandingPage: {url: 'http://google.com'},
    activeConfiguration: 1,
    configurations: [],
  };
}

/**
 * Creates a device with correct descriptors.
 */
function createDeviceWithValidDeviceDescriptor(): FakeUsbDeviceRemote {
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
function createDeviceWithShortDeviceDescriptor(): FakeUsbDeviceRemote {
  const deviceRemote = new FakeUsbDeviceRemote();
  deviceRemote.setDeviceDescriptor({
    status: UsbTransferStatus.SHORT_PACKET,
    data: [0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x50],
  });
  return deviceRemote;
}

/**
 * Stringify a UsbControlTransferParams type object to be the key of
 * response map.
 */
function usbControlTransferParamsToString(params: UsbControlTransferParams):
    string {
  return `${params.type}-${params.recipient}-${params.request}-${
      params.value}-${params.index}`;
}

const setupResolver = new PromiseResolver<void>();
let deviceDescriptorRenderPromise =
    eventToPromise('device-descriptor-complete-for-test', document.body);
let pageHandler: FakePageHandlerRemote|null = null;

const deviceTabInitializedPromise =
    eventToPromise('device-tab-initialized-for-test', document.body);

const deviceManagerGetDevicesPromise =
    eventToPromise('device-list-complete-for-test', document.body);

setSetupFn(() => {
  const pageHandlerInterceptor =
      new MojoInterfaceInterceptor(UsbInternalsPageHandler.$interfaceName);
  pageHandlerInterceptor.oninterfacerequest =
      (e: MojoInterfaceRequestEvent) => {
        pageHandler = new FakePageHandlerRemote(e.handle);
        setupResolver.resolve();
      };
  pageHandlerInterceptor.start();

  return Promise.resolve();
});


suite('UsbInternalsUITest', function() {
  let app: UsbInternalsAppElement;

  suiteSetup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('usb-internals-app');
    document.body.appendChild(app);

    // Before tests are run, make sure setup completes.
    await setupResolver.promise;
    assertTrue(!!pageHandler);
    await pageHandler.whenCalled('bindUsbDeviceManagerInterface');
    assertTrue(!!pageHandler.deviceManager);
    await pageHandler.deviceManager.whenCalled('getDevices');
  });

  teardown(function() {
    assertTrue(!!pageHandler);
    pageHandler.reset();
  });

  test('PageLoaded', async function() {
    const EXPECT_DEVICES_NUM = 2;

    // Totally 2 tables: 'TestDevice' table and 'Device' table.
    const tables = app.shadowRoot!.querySelectorAll('table');
    assertEquals(2, tables.length);

    // Only 2 tabs after loading page.
    const tabs = app.shadowRoot!.querySelectorAll('div[slot=\'tab\']');
    assertEquals(2, tabs.length);
    const tabPanels = app.shadowRoot!.querySelectorAll('div[slot=\'panel\']');
    assertEquals(2, tabPanels.length);

    // The second is the devices table, which has 8 columns.
    const devicesTable = app.shadowRoot!.querySelectorAll('table')[1];
    assertTrue(!!devicesTable);
    const columns =
        devicesTable.querySelector('thead')!.querySelector(
                                                'tr')!.querySelectorAll('th');
    assertEquals(8, columns.length);

    await deviceManagerGetDevicesPromise;
    const devices = devicesTable.querySelectorAll('tbody tr');
    assertEquals(EXPECT_DEVICES_NUM, devices.length);
  });

  test('DeviceTabAdded', function() {
    const devicesTable = app.$('#device-list');
    // Click the inspect button to open information about the first device.
    // The device info is opened as a third tab panel.
    const buttons = devicesTable.querySelectorAll('button');
    assertEquals(2, buttons.length);
    buttons[0]!.click();
    assertEquals(
        3, app.shadowRoot!.querySelectorAll('div[slot=\'tab\']').length);
    let panels = app.shadowRoot!.querySelectorAll('div[slot=\'panel\']');
    assertEquals(3, panels.length);
    assertTrue(panels[2]!.hasAttribute('selected'));

    // Check that clicking the inspect button for another device will open a
    // new tabpanel.
    buttons[1]!.click();
    assertEquals(
        4, app.shadowRoot!.querySelectorAll('div[slot=\'tab\']').length);
    panels = app.shadowRoot!.querySelectorAll('div[slot=\'panel\']');
    assertEquals(4, panels.length);
    assertTrue(panels[3]!.hasAttribute('selected'));
    assertFalse(panels[2]!.hasAttribute('selected'));

    // Check that clicking the inspect button for the same device a second
    // time will open the same tabpanel.
    buttons[0]!.click();
    assertEquals(
        4, app.shadowRoot!.querySelectorAll('div[slot=\'tab\']').length);
    panels = app.shadowRoot!.querySelectorAll('div[slot=\'panel\']');
    assertEquals(4, panels.length);
    assertTrue(panels[2]!.hasAttribute('selected'));
    assertFalse(panels[3]!.hasAttribute('selected'));
  });

  test('RenderDeviceInfoTree', function() {
    // Test the tab opened by clicking inspect button contains a tree view
    // showing WebUSB information. Check the tree displays correct data.
    // The tab panel of the first device is opened in previous test as the
    // third tab panel.
    const deviceTab =
        app.shadowRoot!.querySelectorAll('div[slot=\'panel\']')[2];
    assertTrue(!!deviceTab);
    const tree = deviceTab.querySelector('cr-tree');
    assertTrue(!!tree);
    const treeItems = tree.items as CrTreeItemElement[];
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
      assertEquals(label, treeItems[i]!.labelElement.textContent);
    });
  });

  test('RenderDeviceDescriptor', async function() {
    // Test the tab opened by clicking inspect button contains a panel that
    // can manually retrieve device descriptor from device. Check the response
    // can be rendered correctly.
    await deviceTabInitializedPromise;
    // The tab panel of the first device is opened in previous test as the
    // third tab panel. This device has correct device descriptor.
    const deviceTab =
        app.shadowRoot!.querySelectorAll('div[slot=\'panel\']')[2];
    assertTrue(!!deviceTab);
    const button =
        deviceTab.querySelector<HTMLElement>('.device-descriptor-button');
    assertTrue(!!button);
    button.click();

    await deviceDescriptorRenderPromise;
    const panel = deviceTab.querySelector('.device-descriptor-panel');
    assertTrue(!!panel);
    assertEquals(1, panel.querySelectorAll('descriptorpanel').length);
    assertEquals(0, panel.querySelectorAll('error').length);
    const tree = panel.querySelector('cr-tree');
    assertTrue(!!tree);
    const treeItems = tree.items as CrTreeItemElement[];
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
      assertEquals(label, treeItems[i]!.labelElement.textContent);
    });

    const byteElements =
        panel.querySelectorAll<HTMLElement>('.raw-data-byte-view span');
    assertEquals(18, byteElements.length);
    assertEquals(
        '12010002000000405010EF17210301020001',
        panel.querySelector('.raw-data-byte-view')!.textContent);

    // Click a single byte tree item (Length) and check that both the item
    // and the related byte are highlighted.
    treeItems[0]!.rowElement.click();
    assertTrue(treeItems[0]!.hasAttribute('selected'));
    assertTrue(byteElements[0]!.classList.contains('selected-field'));
    // Click a multi-byte tree item (Vendor ID) and check that both the
    // item and the related bytes are highlighted, and other items and bytes
    // are not highlighted.
    treeItems[7]!.rowElement.click();
    assertFalse(treeItems[0]!.hasAttribute('selected'));
    assertTrue(treeItems[7]!.hasAttribute('selected'));
    assertFalse(byteElements[0]!.classList.contains('selected-field'));
    assertTrue(byteElements[8]!.classList.contains('selected-field'));
    assertTrue(byteElements[9]!.classList.contains('selected-field'));
    // Click a single byte element (Descriptor Type) and check that both the
    // byte and the related item are highlighted, and other items and bytes
    // are not highlighted.
    byteElements[1]!.click();
    assertFalse(treeItems[7]!.hasAttribute('selected'));
    assertTrue(treeItems[1]!.hasAttribute('selected'));
    assertTrue(byteElements[1]!.classList.contains('selected-field'));
    // Click any byte element of a multi-byte element (Product ID) and check
    // that both the bytes and the related item are highlighted, and other
    // items and bytes are not highlighted.
    byteElements[11]!.click();
    assertFalse(treeItems[1]!.hasAttribute('selected'));
    assertTrue(treeItems[8]!.hasAttribute('selected'));
    assertTrue(byteElements[10]!.classList.contains('selected-field'));
    assertTrue(byteElements[11]!.classList.contains('selected-field'));
  });

  test('RenderShortDeviceDescriptor', async function() {
    await deviceManagerGetDevicesPromise;
    const devicesTable = app.$('#device-list');
    // Inspect the second device, which has short device descriptor.
    const buttons = devicesTable.querySelectorAll('button');
    assertTrue(!!buttons[1]);
    buttons[1].click();

    // The fourth is the device tab (a third tab was opened in a previous test).
    const deviceTab =
        app.shadowRoot!.querySelectorAll('div[slot=\'panel\']')[3];
    assertTrue(!!deviceTab);

    await deviceTabInitializedPromise;
    deviceDescriptorRenderPromise =
        eventToPromise('device-descriptor-complete-for-test', document.body);
    const button =
        deviceTab.querySelector<HTMLElement>('.device-descriptor-button');
    assertTrue(!!button);
    button.click();

    await deviceDescriptorRenderPromise;
    const panel = deviceTab.querySelector('.device-descriptor-panel');
    assertTrue(!!panel);

    assertEquals(1, panel.querySelectorAll('descriptorpanel').length);
    const errors = panel.querySelectorAll<HTMLElement>('error');
    assertEquals(2, errors.length);
    assertEquals('Field at offset 8 is invalid.', errors[0]!.textContent);
    assertEquals('Descriptor is too short.', errors[1]!.textContent);
    // For the short response, the returned data should still be rendered.
    const tree = panel.querySelector('cr-tree');
    assertTrue(!!tree);
    const treeItems = tree.items as CrTreeItemElement[];
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
      assertEquals(label, treeItems[i]!.labelElement.textContent);
    });

    const byteElements =
        panel.querySelectorAll<HTMLElement>('.raw-data-byte-view span');
    assertEquals(9, byteElements.length);
    assertEquals(
        '120100020000004050',
        panel.querySelector('.raw-data-byte-view')!.textContent);


    // Click a single byte tree item (Length) and check that both the item
    // and the related byte are highlighted.
    treeItems[0]!.rowElement.click();
    assertTrue(treeItems[0]!.hasAttribute('selected'));
    assertTrue(byteElements[0]!.classList.contains('selected-field'));
    // Click a multi-byte tree item (USB Version) and check that both the
    // item and the related bytes are highlighted, and other items and bytes
    // are not highlighted.
    treeItems[2]!.rowElement.click();
    assertFalse(treeItems[0]!.hasAttribute('selected'));
    assertTrue(treeItems[2]!.hasAttribute('selected'));
    assertFalse(byteElements[0]!.classList.contains('selected-field'));
    assertTrue(byteElements[2]!.classList.contains('selected-field'));
    assertTrue(byteElements[3]!.classList.contains('selected-field'));
    // Click a single byte element (Descriptor Type) and check that both the
    // byte and the related item are highlighted, and other items and bytes
    // are not highlighted.
    byteElements[1]!.click();
    assertFalse(treeItems[2]!.hasAttribute('selected'));
    assertTrue(treeItems[1]!.hasAttribute('selected'));
    assertTrue(byteElements[1]!.classList.contains('selected-field'));
    // Click any byte element of a multi-byte element (USB Version) and
    // check that both the bytes and the related item are highlighted, and
    // other items and bytes are not highlighted.
    byteElements[3]!.click();
    assertFalse(treeItems[1]!.hasAttribute('selected'));
    assertTrue(treeItems[2]!.hasAttribute('selected'));
    assertTrue(byteElements[2]!.classList.contains('selected-field'));
    assertTrue(byteElements[3]!.classList.contains('selected-field'));
    // Click the invalid field's byte (Vendor ID) will do nothing, check the
    // highlighted item and bytes are not changed.
    byteElements[8]!.click();
    assertTrue(treeItems[2]!.hasAttribute('selected'));
    assertTrue(byteElements[2]!.classList.contains('selected-field'));
    assertTrue(byteElements[3]!.classList.contains('selected-field'));
    assertFalse(byteElements[8]!.classList.contains('selected-field'));
  });
});
