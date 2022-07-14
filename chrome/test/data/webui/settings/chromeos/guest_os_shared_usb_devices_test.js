// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {GuestOsBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

/** @implements {GuestOsBrowserProxy} */
class TestGuestOsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'notifyGuestOsSharedUsbDevicesPageReady',
      'setGuestOsUsbDeviceShared',
    ]);
    this.sharedUsbDevices = [];
  }

  /** @override */
  notifyGuestOsSharedUsbDevicesPageReady() {
    this.methodCalled('notifyGuestOsSharedUsbDevicesPageReady');
    webUIListenerCallback(
        'guest-os-shared-usb-devices-changed', this.sharedUsbDevices);
  }

  /** override */
  setGuestOsUsbDeviceShared(vmName, guid, shared) {
    this.methodCalled('setGuestOsUsbDeviceShared', [vmName, guid, shared]);
  }
}

suite('SharedUsbDevices', function() {
  /** @type {?SettingsGuestOsSharedUsbDevicesElement} */
  let page = null;

  /** @type {?TestGuestOsBrowserProxy} */
  let guestOsBrowserProxy = null;

  setup(async function() {
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    guestOsBrowserProxy.sharedUsbDevices = [
      {
        guid: '0001',
        label: 'usb_dev1',
        sharedWith: null,
        promptBeforeSharing: false,
      },
      {
        guid: '0002',
        label: 'usb_dev2',
        sharedWith: 'PvmDefault',
        promptBeforeSharing: true,
      },
      {
        guid: '0003',
        label: 'usb_dev3',
        sharedWith: 'otherVm',
        promptBeforeSharing: true,
      },
    ];
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);
    PolymerTest.clearBody();
    page = document.createElement('settings-guest-os-shared-usb-devices');
    page.guestOsType = 'pluginVm';
    document.body.appendChild(page);
    await flushTasks();
  });

  teardown(function() {
    page.remove();
  });

  test('USB devices are shown', async function() {
    assertEquals(3, page.shadowRoot.querySelectorAll('.toggle').length);
  });

  test('USB shared state is updated by toggling', async function() {
    assertTrue(!!page.shadowRoot.querySelector('.toggle'));
    page.shadowRoot.querySelector('.toggle').click();

    await flushTasks();
    flush();

    const args =
        await guestOsBrowserProxy.whenCalled('setGuestOsUsbDeviceShared');
    assertEquals('PvmDefault', args[0]);
    assertEquals('0001', args[1]);
    assertEquals(true, args[2]);
    // Simulate a change in the underlying model.
    webUIListenerCallback('guest-os-shared-usb-devices-changed', [
      {
        guid: '0001',
        label: 'usb_dev1',
        sharedWith: 'PvmDefault',
        promptBeforeSharing: true,
      },
    ]);
    flush();
    assertEquals(1, page.shadowRoot.querySelectorAll('.toggle').length);
  });

  test('Show dialog for reassign', async function() {
    const items = page.shadowRoot.querySelectorAll('.toggle');
    assertEquals(3, items.length);

    // Clicking on item[2] should show dialog.
    assertFalse(!!page.shadowRoot.querySelector('#reassignDialog'));
    items[2].click();
    flush();
    assertTrue(page.shadowRoot.querySelector('#reassignDialog').open);

    // Clicking cancel will close the dialog.
    page.shadowRoot.querySelector('#cancel').click();
    flush();
    assertFalse(!!page.shadowRoot.querySelector('#reassignDialog'));

    // Pressing escape will close the dialog, but it's not possible to trigger
    // this with a fake keypress, so we instead send the 'cancel' event directly
    // to the native <dialog> element.
    items[2].click();
    flush();
    assertTrue(page.shadowRoot.querySelector('#reassignDialog').open);
    const e = new CustomEvent('cancel', {cancelable: true});
    page.shadowRoot.querySelector('#reassignDialog')
        .getNative()
        .dispatchEvent(e);
    flush();
    assertFalse(!!page.shadowRoot.querySelector('#reassignDialog'));

    // Clicking continue will call the proxy and close the dialog.
    items[2].click();
    flush();
    assertTrue(page.shadowRoot.querySelector('#reassignDialog').open);
    page.shadowRoot.querySelector('#continue').click();
    flush();
    assertFalse(!!page.shadowRoot.querySelector('#reassignDialog'));
    const args =
        await guestOsBrowserProxy.whenCalled('setGuestOsUsbDeviceShared');
    assertEquals('PvmDefault', args[0]);
    assertEquals('0003', args[1]);
    assertEquals(true, args[2]);
  });
});
