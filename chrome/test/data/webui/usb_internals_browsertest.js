// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://usb-internals
 */

/**
 * Test fixture for testing async methods of cr.js.
 * @constructor
 * @extends testing.Test
 */
function UsbInternalsTest() {
  this.setupResolver = new PromiseResolver();
  this.deviceManagerGetDevicesResolver = new PromiseResolver();
  this.deviceTabInitializedResolver = new PromiseResolver();
  this.deviceDescriptorRenderResolver = new PromiseResolver();
}

UsbInternalsTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://usb-internals',

  /** @override */
  isAsync: true,

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '//ui/webui/resources/js/cr.js',
    '//ui/webui/resources/js/util.js',
    '//chrome/test/data/webui/test_browser_proxy.js',
  ],

  preLoad: function() {
    /** @implements {mojom.UsbInternalsPageHandlerRemote} */
    class FakePageHandlerRemote extends TestBrowserProxy {
      constructor(handle) {
        super([
          'bindUsbDeviceManagerInterface',
          'bindTestInterface',
        ]);

        this.receiver_ = new mojom.UsbInternalsPageHandlerReceiver(this);
        this.receiver_.$.bindHandle(handle);
      }

      async bindUsbDeviceManagerInterface(deviceManagerPendingReceiver) {
        this.methodCalled(
            'bindUsbDeviceManagerInterface', deviceManagerPendingReceiver);
        this.deviceManager =
            new FakeDeviceManagerRemote(deviceManagerPendingReceiver);
      }

      async bindTestInterface(testDeviceManagerPendingReceiver) {
        this.methodCalled(
            'bindTestInterface', testDeviceManagerPendingReceiver);
      }
    }

    /** @implements {device.mojom.UsbDeviceManagerRemote} */
    class FakeDeviceManagerRemote extends TestBrowserProxy {
      constructor(pendingReceiver) {
        super([
          'enumerateDevicesAndSetClient',
          'getDevice',
          'getDevices',
          'checkAccess',
          'openFileDescriptor',
          'setClient',
        ]);

        this.receiver_ = new device.mojom.UsbDeviceManagerReceiver(this);
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

      async getDevice(guid, devicePendingReceiver, deviceClient) {
        this.methodCalled('getDevice');
        const deviceRemote = this.deviceRemoteMap.get(guid);
        deviceRemote.router.$.bindHandle(devicePendingReceiver.handle);
      }

      async getDevices() {
        this.methodCalled('getDevices');
        return {results: this.devices};
      }

      async checkAccess() {}

      async openFileDescriptor() {}

      async setClient() {}
    }

    /** @implements {device.mojom.UsbDeviceRemote} */
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
        this.router = new device.mojom.UsbDeviceCallbackRouter;
        this.router.open.addListener(async () => {
          return {error: device.mojom.UsbOpenDeviceError.OK};
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
            status: device.mojom.UsbTransferStatus.TRANSFER_ERROR,
            data: [],
          };
        }
        response.data = response.data.slice(0, length);
        return response;
      }

      /**
       * Set a response for a given request.
       * @param {!device.mojom.UsbControlTransferParams} params
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
        params.type = device.mojom.UsbControlTransferType.STANDARD;
        params.recipient = device.mojom.UsbControlTransferRecipient.DEVICE;
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
        status: device.mojom.UsbTransferStatus.COMPLETED,
        data: [
          0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x50, 0x10, 0xEF,
          0x17, 0x21, 0x03, 0x01, 0x02, 0x00, 0x01
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
        status: device.mojom.UsbTransferStatus.SHORT_PACKET,
        data: [0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x50],
      });
      return deviceRemote;
    }

    /**
     * Converts an ECMAScript string to an instance of mojo_base.mojom.String16.
     * @param {string} string
     * @return {!object}
     */
    function stringToMojoString16(string) {
      return {data: Array.from(string, c => c.charCodeAt(0))};
    }

    /**
     * Stringify a UsbControlTransferParams type object to be the key of
     * response map.
     * @param {!device.mojom.UsbControlTransferParams} params
     * @return {string}
     */
    function usbControlTransferParamsToString(params) {
      return `${params.type}-${params.recipient}-${params.request}-${
          params.value}-${params.index}`;
    }

    window.deviceListCompleteFn = () => {
      this.deviceManagerGetDevicesResolver.resolve();
    };

    window.deviceTabInitializedFn = () => {
      this.deviceTabInitializedResolver.resolve();
    };

    window.deviceDescriptorCompleteFn = () => {
      this.deviceDescriptorRenderResolver.resolve();
    };

    window.setupFn = () => {
      this.pageHandlerInterceptor = new MojoInterfaceInterceptor(
          mojom.UsbInternalsPageHandler.$interfaceName);
      this.pageHandlerInterceptor.oninterfacerequest = (e) => {
        this.pageHandler = new FakePageHandlerRemote(e.handle);
      };
      this.pageHandlerInterceptor.start();

      this.setupResolver.resolve();
      return Promise.resolve();
    };
  },
};

TEST_F('UsbInternalsTest', 'WebUICorrectValueRenderTest', function() {
  let pageHandler;

  // Before tests are run, make sure setup completes.
  let setupPromise = this.setupResolver.promise.then(() => {
    pageHandler = this.pageHandler;
  });

  let deviceManagerGetDevicesPromise =
      this.deviceManagerGetDevicesResolver.promise;
  let deviceTabInitializedPromise = this.deviceTabInitializedResolver.promise;
  let deviceDescriptorRenderPromise =
      this.deviceDescriptorRenderResolver.promise;

  suite('UsbInternalsUITest', function() {
    const EXPECT_DEVICES_NUM = 2;

    suiteSetup(function() {
      return setupPromise.then(function() {
        return Promise.all([
          pageHandler.whenCalled('bindUsbDeviceManagerInterface'),
          pageHandler.deviceManager.whenCalled('getDevices'),
        ]);
      });
    });

    teardown(function() {
      pageHandler.reset();
    });

    test('PageLoaded', async function() {
      // Totally 2 tables: 'TestDevice' table and 'Device' table.
      const tables = document.querySelectorAll('table');
      expectEquals(2, tables.length);

      // Only 2 tabs after loading page.
      const tabs = document.querySelectorAll('tab');
      expectEquals(2, tabs.length);
      const tabPanels = document.querySelectorAll('tabpanel');
      expectEquals(2, tabPanels.length);

      // The second is the devices table, which has 8 columns.
      const devicesTable = document.querySelectorAll('table')[1];
      const columns = devicesTable.querySelector('thead')
                          .querySelector('tr')
                          .querySelectorAll('th');
      expectEquals(8, columns.length);

      await deviceManagerGetDevicesPromise;
      const devices = devicesTable.querySelectorAll('tbody tr');
      expectEquals(EXPECT_DEVICES_NUM, devices.length);
    });

    test('DeviceTabAdded', function() {
      const devicesTable = document.querySelector('#device-list');
      // Click the inspect button to open information about the first device.
      // The device info is opened as a third tab panel.
      devicesTable.querySelectorAll('button')[0].click();
      assertEquals(3, document.querySelectorAll('tab').length);
      assertEquals(3, document.querySelectorAll('tabpanel').length);
      expectTrue(document.querySelectorAll('tabpanel')[2].selected);

      // Check that clicking the inspect button for another device will open a
      // new tabpanel.
      devicesTable.querySelectorAll('button')[1].click();
      assertEquals(4, document.querySelectorAll('tab').length);
      assertEquals(4, document.querySelectorAll('tabpanel').length);
      expectTrue(document.querySelectorAll('tabpanel')[3].selected);
      expectFalse(document.querySelectorAll('tabpanel')[2].selected);

      // Check that clicking the inspect button for the same device a second
      // time will open the same tabpanel.
      devicesTable.querySelectorAll('button')[0].click();
      assertEquals(4, document.querySelectorAll('tab').length);
      assertEquals(4, document.querySelectorAll('tabpanel').length);
      expectTrue(document.querySelectorAll('tabpanel')[2].selected);
      expectFalse(document.querySelectorAll('tabpanel')[3].selected);
    });

    test('RenderDeviceInfoTree', function() {
      // Test the tab opened by clicking inspect button contains a tree view
      // showing WebUSB information. Check the tree displays correct data.
      // The tab panel of the first device is opened in previous test as the
      // third tab panel.
      const deviceTab = document.querySelectorAll('tabpanel')[2];
      const tree = deviceTab.querySelector('tree');
      const treeItems = tree.querySelectorAll('.tree-item');
      assertEquals(11, treeItems.length);
      expectEquals('USB Version: 2.0.0', treeItems[0].textContent);
      expectEquals('Class Code: 0', treeItems[1].textContent);
      expectEquals('Subclass Code: 0', treeItems[2].textContent);
      expectEquals('Protocol Code: 0', treeItems[3].textContent);
      expectEquals('Port Number: 0', treeItems[4].textContent);
      expectEquals('Vendor Id: 0x1050', treeItems[5].textContent);
      expectEquals('Product Id: 0x17EF', treeItems[6].textContent);
      expectEquals('Device Version: 3.2.1', treeItems[7].textContent);
      expectEquals('Manufacturer Name: test', treeItems[8].textContent);
      expectEquals(
          'WebUSB Landing Page: http://google.com', treeItems[9].textContent);
      expectEquals('Active Configuration: 1', treeItems[10].textContent);
    });

    test('RenderDeviceDescriptor', async function() {
      // Test the tab opened by clicking inspect button contains a panel that
      // can manually retrieve device descriptor from device. Check the response
      // can be rendered correctly.
      await deviceTabInitializedPromise;
      // The tab panel of the first device is opened in previous test as the
      // third tab panel. This device has correct device descriptor.
      const deviceTab = document.querySelectorAll('tabpanel')[2];
      deviceTab.querySelector('.device-descriptor-button').click();

      await deviceDescriptorRenderPromise;
      const panel = deviceTab.querySelector('.device-descriptor-panel');
      expectEquals(1, panel.querySelectorAll('descriptorpanel').length);
      expectEquals(0, panel.querySelectorAll('error').length);
      const treeItems = panel.querySelectorAll('.tree-item');
      assertEquals(14, treeItems.length);
      expectEquals('Length (should be 18): 18', treeItems[0].textContent);
      expectEquals(
          'Descriptor Type (should be 0x01): 0x01', treeItems[1].textContent);
      expectEquals('USB Version: 2.0.0', treeItems[2].textContent);
      expectEquals('Class Code: 0', treeItems[3].textContent);
      expectEquals('Subclass Code: 0', treeItems[4].textContent);
      expectEquals('Protocol Code: 0', treeItems[5].textContent);
      expectEquals(
          'Control Pipe Maximum Packet Size: 64', treeItems[6].textContent);
      expectEquals('Vendor ID: 0x1050', treeItems[7].textContent);
      expectEquals('Product ID: 0x17EF', treeItems[8].textContent);
      expectEquals('Device Version: 3.2.1', treeItems[9].textContent);
      // The string descriptor index fields with non-zero number should have a
      // "GET" button.
      expectEquals(
          'Manufacturer String Index: 1GET', treeItems[10].textContent);
      expectEquals('Product String Index: 2GET', treeItems[11].textContent);
      expectEquals('Serial Number Index: 0', treeItems[12].textContent);
      expectEquals('Number of Configurations: 1', treeItems[13].textContent);
      const byteElements = panel.querySelectorAll('.raw-data-byte-view span');
      expectEquals(18, byteElements.length);
      expectEquals(
          '12010002000000405010EF17210301020001',
          panel.querySelector('.raw-data-byte-view').textContent);

      // Click a single byte tree item (Length) and check that both the item
      // and the related byte are highlighted.
      treeItems[0].querySelector('.tree-row').click();
      expectTrue(treeItems[0].selected);
      expectTrue(byteElements[0].classList.contains('selected-field'));
      // Click a multi-byte tree item (Vendor ID) and check that both the
      // item and the related bytes are highlighted, and other items and bytes
      // are not highlighted.
      treeItems[7].querySelector('.tree-row').click();
      expectFalse(treeItems[0].selected);
      expectTrue(treeItems[7].selected);
      expectFalse(byteElements[0].classList.contains('selected-field'));
      expectTrue(byteElements[8].classList.contains('selected-field'));
      expectTrue(byteElements[9].classList.contains('selected-field'));
      // Click a single byte element (Descriptor Type) and check that both the
      // byte and the related item are highlighted, and other items and bytes
      // are not highlighted.
      byteElements[1].click();
      expectFalse(treeItems[7].selected);
      expectTrue(treeItems[1].selected);
      expectTrue(byteElements[1].classList.contains('selected-field'));
      // Click any byte element of a multi-byte element (Product ID) and check
      // that both the bytes and the related item are highlighted, and other
      // items and bytes are not highlighted.
      byteElements[11].click();
      expectFalse(treeItems[1].selected);
      expectTrue(treeItems[8].selected);
      expectTrue(byteElements[10].classList.contains('selected-field'));
      expectTrue(byteElements[11].classList.contains('selected-field'));
    });
  });

  // Run all registered tests.
  mocha.run();
});

TEST_F('UsbInternalsTest', 'WebUIIncorrectValueRenderTest', function() {
  let pageHandler;

  // Before tests are run, make sure setup completes.
  let setupPromise = this.setupResolver.promise.then(() => {
    pageHandler = this.pageHandler;
  });

  let deviceManagerGetDevicesPromise =
      this.deviceManagerGetDevicesResolver.promise;
  let deviceTabInitializedPromise = this.deviceTabInitializedResolver.promise;
  let deviceDescriptorRenderPromise =
      this.deviceDescriptorRenderResolver.promise;

  suite('UsbInternalsUITest', function() {
    suiteSetup(function() {
      return setupPromise.then(function() {
        return Promise.all([
          pageHandler.whenCalled('bindUsbDeviceManagerInterface'),
          pageHandler.deviceManager.whenCalled('getDevices'),
        ]);
      });
    });

    test('RenderShortDeviceDescriptor', async function() {
      await deviceManagerGetDevicesPromise;
      const devicesTable = document.querySelector('#device-list');
      // Inspect the second device, which has short device descriptor.
      devicesTable.querySelectorAll('button')[1].click();
      // The third is the device tab.
      const deviceTab = document.querySelectorAll('tabpanel')[2];

      await deviceTabInitializedPromise;
      deviceTab.querySelector('.device-descriptor-button').click();

      await deviceDescriptorRenderPromise;
      const panel = deviceTab.querySelector('.device-descriptor-panel');

      expectEquals(1, panel.querySelectorAll('descriptorpanel').length);
      const errors = panel.querySelectorAll('error');
      assertEquals(2, errors.length);
      expectEquals('Field at offset 8 is invalid.', errors[0].textContent);
      expectEquals('Descriptor is too short.', errors[1].textContent);
      // For the short response, the returned data should still be rendered.
      const treeItems = panel.querySelectorAll('.tree-item');
      assertEquals(7, treeItems.length);
      expectEquals('Length (should be 18): 18', treeItems[0].textContent);
      expectEquals(
          'Descriptor Type (should be 0x01): 0x01', treeItems[1].textContent);
      expectEquals('USB Version: 2.0.0', treeItems[2].textContent);
      expectEquals('Class Code: 0', treeItems[3].textContent);
      expectEquals('Subclass Code: 0', treeItems[4].textContent);
      expectEquals('Protocol Code: 0', treeItems[5].textContent);
      expectEquals(
          'Control Pipe Maximum Packet Size: 64', treeItems[6].textContent);

      const byteElements = panel.querySelectorAll('.raw-data-byte-view span');
      expectEquals(9, byteElements.length);
      expectEquals(
          '120100020000004050',
          panel.querySelector('.raw-data-byte-view').textContent);


      // Click a single byte tree item (Length) and check that both the item
      // and the related byte are highlighted.
      treeItems[0].querySelector('.tree-row').click();
      expectTrue(treeItems[0].selected);
      expectTrue(byteElements[0].classList.contains('selected-field'));
      // Click a multi-byte tree item (USB Version) and check that both the
      // item and the related bytes are highlighted, and other items and bytes
      // are not highlighted.
      treeItems[2].querySelector('.tree-row').click();
      expectFalse(treeItems[0].selected);
      expectTrue(treeItems[2].selected);
      expectFalse(byteElements[0].classList.contains('selected-field'));
      expectTrue(byteElements[2].classList.contains('selected-field'));
      expectTrue(byteElements[3].classList.contains('selected-field'));
      // Click a single byte element (Descriptor Type) and check that both the
      // byte and the related item are highlighted, and other items and bytes
      // are not highlighted.
      byteElements[1].click();
      expectFalse(treeItems[2].selected);
      expectTrue(treeItems[1].selected);
      expectTrue(byteElements[1].classList.contains('selected-field'));
      // Click any byte element of a multi-byte element (USB Version) and
      // check that both the bytes and the related item are highlighted, and
      // other items and bytes are not highlighted.
      byteElements[3].click();
      expectFalse(treeItems[1].selected);
      expectTrue(treeItems[2].selected);
      expectTrue(byteElements[2].classList.contains('selected-field'));
      expectTrue(byteElements[3].classList.contains('selected-field'));
      // Click the invalid field's byte (Vendor ID) will do nothing, check the
      // highlighted item and bytes are not changed.
      byteElements[8].click();
      expectTrue(treeItems[2].selected);
      expectTrue(byteElements[2].classList.contains('selected-field'));
      expectTrue(byteElements[3].classList.contains('selected-field'));
      expectFalse(byteElements[8].classList.contains('selected-field'));
    });
  });

  // Run all registered tests.
  mocha.run();
});
