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
      {
        guid: '0001',
        label: 'usb_dev1',
        shared: false,
        shareWillReassign: false,
      },
      {guid: '0002', label: 'usb_dev2', shared: true, shareWillReassign: false},
      {guid: '0003', label: 'usb_dev3', shared: false, shareWillReassign: true},
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
    assertEquals(true, args[1]);
    // Simulate a change in the underlying model.
    cr.webUIListenerCallback('plugin-vm-shared-usb-devices-changed', [
      {guid: '0001', label: 'usb_dev1', shared: true, shareWillReassign: false},
    ]);
    Polymer.dom.flush();
    assertEquals(1, page.shadowRoot.querySelectorAll('.toggle').length);
  });

  test('Show dialog for reassign', async function() {
    const items = page.shadowRoot.querySelectorAll('.toggle');
    assertEquals(3, items.length);

    // Clicking on item[2] should show dialog.
    assertFalse(!!page.$$('#reassignDialog'));
    items[2].click();
    Polymer.dom.flush();
    assertTrue(page.$$('#reassignDialog').open);

    // Clicking cancel will close the dialog.
    page.$$('#cancel').click();
    Polymer.dom.flush();
    assertFalse(!!page.$$('#reassignDialog'));

    // Clicking continue will call the proxy and close the dialog.
    items[2].click();
    Polymer.dom.flush();
    assertTrue(page.$$('#reassignDialog').open);
    page.$$('#continue').click();
    Polymer.dom.flush();
    assertFalse(!!page.$$('#reassignDialog'));
    const args =
        await pluginVmBrowserProxy.whenCalled('setPluginVmUsbDeviceShared');
    assertEquals('0003', args[0]);
    assertEquals(true, args[1]);
  });
});
