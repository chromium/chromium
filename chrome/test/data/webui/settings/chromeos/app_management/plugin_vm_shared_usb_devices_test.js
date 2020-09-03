// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.PluginVmBrowserProxy} */
class TestPluginVmBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'notifyPluginVmSharedUsbDevicesPageReady',
      'setPluginVmUsbDeviceShared',
    ]);
    this.sharedUsbDevices = [];
  }

  /** @override */
  notifyPluginVmSharedUsbDevicesPageReady() {
    this.methodCalled('notifyPluginVmSharedUsbDevicesPageReady');
    cr.webUIListenerCallback(
        'plugin-vm-shared-usb-devices-changed', this.sharedUsbDevices);
  }

  /** override */
  setPluginVmUsbDeviceShared(guid, shared) {
    this.methodCalled('setPluginVmUsbDeviceShared', [guid, shared]);
  }
}

suite('SharedUsbDevices', function() {
  /** @type {?SettingsPluginVmSharedUsbDevicesElement} */
  let page = null;

  /** @type {?TestPluginVmBrowserProxy} */
  let pluginVmBrowserProxy = null;

  setup(async function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    pluginVmBrowserProxy.sharedUsbDevices = [
      {guid: '0001', label: 'usb_dev1', shared: true},
      {guid: '0002', label: 'usb_dev2', shared: false},
      {guid: '0003', label: 'usb_dev3', shared: true},
    ];
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-plugin-vm-shared-usb-devices');
    document.body.appendChild(page);
    await test_util.flushTasks();
  });

  teardown(function() {
    page.remove();
  });

  test('USB devices are shown', async function() {
    assertEquals(3, page.shadowRoot.querySelectorAll('.toggle').length);
  });

  test('USB shared state is updated by toggling', async function() {
    assertTrue(!!page.$$('.toggle'));
    page.$$('.toggle').click();

    await test_util.flushTasks();
    Polymer.dom.flush();

    const args =
        await pluginVmBrowserProxy.whenCalled('setPluginVmUsbDeviceShared');
    assertEquals('0001', args[0]);
    assertEquals(false, args[1]);
    // Simulate a change in the underlying model.
    cr.webUIListenerCallback('plugin-vm-shared-usb-devices-changed', [
      {guid: '0001', label: 'usb_dev1', shared: true},
    ]);
    Polymer.dom.flush();
    assertEquals(1, page.shadowRoot.querySelectorAll('.toggle').length);
  });
});
