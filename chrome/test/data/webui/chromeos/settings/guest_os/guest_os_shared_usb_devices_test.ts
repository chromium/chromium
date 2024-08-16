// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {GuestOsBrowserProxyImpl, SettingsGuestOsSharedUsbDevicesElement} from 'chrome://os-settings/lazy_load.js';
import {CrDialogElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestGuestOsBrowserProxy} from './test_guest_os_browser_proxy.js';

suite('<settings-guest-os-shared-usb-devices>', () => {
  let page: SettingsGuestOsSharedUsbDevicesElement;
  let guestOsBrowserProxy: TestGuestOsBrowserProxy;

  setup(async () => {
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    guestOsBrowserProxy.sharedUsbDevices = [
      {
        guid: '0001',
        label: 'usb_dev1',
        guestId: {
          vm_name: '',
          container_name: '',
        },
        vendorId: '0000',
        productId: '0000',
        promptBeforeSharing: false,
        serialNumber: '',
      },
      {
        guid: '0002',
        label: 'usb_dev2',
        guestId: {
          vm_name: 'PvmDefault',
          container_name: '',
        },
        vendorId: '0000',
        productId: '0000',
        promptBeforeSharing: true,
        serialNumber: '',
      },
      {
        guid: '0003',
        label: 'usb_dev3',
        guestId: {
          vm_name: 'otherVm',
          container_name: '',
        },
        vendorId: '0000',
        productId: '0000',
        promptBeforeSharing: true,
        serialNumber: '',
      },
    ];
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);
    page = document.createElement('settings-guest-os-shared-usb-devices');
    page.guestOsType = 'pluginVm';
    document.body.appendChild(page);
    await flushTasks();
  });

  teardown(() => {
    page.remove();
  });

  test('USB devices are shown', () => {
    assertEquals(3, page.shadowRoot!.querySelectorAll('.toggle').length);
  });

  test('USB shared state is updated by toggling', async () => {
    const toggleElement = page.shadowRoot!.querySelector('cr-toggle');
    assert(toggleElement);
    toggleElement.click();

    await flushTasks();
    flush();

    const args =
        await guestOsBrowserProxy.whenCalled('setGuestOsUsbDeviceShared');
    assertEquals('PvmDefault', args[0]);
    assertEquals('', args[1]);
    assertEquals('0001', args[2]);
    assertEquals(true, args[3]);
    // Simulate a change in the underlying model.
    webUIListenerCallback('guest-os-shared-usb-devices-changed', [
      {
        guid: '0001',
        label: 'usb_dev1',
        guestId: {
          vm_name: 'PvmDefault',
          container_name: '',
        },
        vendorId: '0000',
        productId: '0000',
        promptBeforeSharing: true,
      },
    ]);
    flush();
    assertEquals(1, page.shadowRoot!.querySelectorAll('.toggle').length);
  });

  test('Show dialog for reassign', async () => {
    const items = page.shadowRoot!.querySelectorAll('cr-toggle');
    assertEquals(3, items.length);

    // Clicking on item[2] should show dialog.
    let reassignDialog =
        page.shadowRoot!.querySelector<CrDialogElement>('#reassignDialog');
    assertEquals(null, reassignDialog);
    items[2]!.click();
    flush();
    reassignDialog =
        page.shadowRoot!.querySelector<CrDialogElement>('#reassignDialog');
    assert(reassignDialog);
    assertTrue(reassignDialog.open);

    // Clicking cancel will close the dialog.
    page.shadowRoot!.querySelector<HTMLElement>('#cancel')!.click();
    flush();
    assertEquals(null, page.shadowRoot!.querySelector('#reassignDialog'));

    // Pressing escape will close the dialog, but it's not possible to trigger
    // this with a fake keypress, so we instead send the 'cancel' event directly
    // to the native <dialog> element.
    items[2]!.click();
    flush();
    reassignDialog =
        page.shadowRoot!.querySelector<CrDialogElement>('#reassignDialog');
    assert(reassignDialog);
    assertTrue(reassignDialog.open);

    const e = new CustomEvent('cancel', {cancelable: true});
    page.shadowRoot!.querySelector<CrDialogElement>('#reassignDialog')!
        .getNative()
        .dispatchEvent(e);
    await flushTasks();
    assertEquals(null, page.shadowRoot!.querySelector('#reassignDialog'));

    // Clicking continue will call the proxy and close the dialog.
    items[2]!.click();
    flush();
    reassignDialog = page.shadowRoot!.querySelector('#reassignDialog');
    assert(reassignDialog);
    assertTrue(reassignDialog.open);
    page.shadowRoot!.querySelector<HTMLElement>('#continue')!.click();
    flush();
    assertEquals(null, page.shadowRoot!.querySelector('#reassignDialog'));
    const args =
        await guestOsBrowserProxy.whenCalled('setGuestOsUsbDeviceShared');
    assertEquals('PvmDefault', args[0]);
    assertEquals('', args[1]);
    assertEquals('0003', args[2]);
    assertEquals(true, args[3]);
  });
});

suite('<settings-guest-os-shared-usb-devices> multi-container', () => {
  let page: SettingsGuestOsSharedUsbDevicesElement;
  let guestOsBrowserProxy: TestGuestOsBrowserProxy;

  setup(async () => {
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    guestOsBrowserProxy.sharedUsbDevices = [
      {
        guid: '0001',
        label: 'usb_dev1',
        guestId: {
          vm_name: '',
          container_name: '',
        },
        vendorId: '0000',
        productId: '0000',
        promptBeforeSharing: false,
        serialNumber: '',
      },
      {
        guid: '0002',
        label: 'usb_dev2',
        guestId: {
          vm_name: 'termina',
          container_name: 'penguin',
        },
        vendorId: '0000',
        productId: '0000',
        promptBeforeSharing: true,
        serialNumber: '',
      },
      {
        guid: '0003',
        label: 'usb_dev3',
        guestId: {
          vm_name: 'not-termina',
          container_name: 'not-penguin',
        },
        vendorId: '0000',
        productId: '0000',
        promptBeforeSharing: true,
        serialNumber: '',
      },
    ];
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);
    page = document.createElement('settings-guest-os-shared-usb-devices');
    page.guestOsType = 'crostini';
    page.hasContainers = true;
    page.defaultGuestId = {
      'vm_name': 'termina',
      'container_name': 'penguin',
    };
    page['onContainerInfo_']([
      {
        id: {
          vm_name: 'termina',
          container_name: 'penguin',
        },
        ipv4: '1.2.3.4',
      },
      {
        id: {
          vm_name: 'not-termina',
          container_name: 'not-penguin',
        },
        ipv4: '1.2.3.5',
      },
    ]);
    document.body.appendChild(page);
    await flushTasks();
  });

  teardown(() => {
    page.remove();
  });

  test('USB devices are shown', async () => {
    const guests =
        page.shadowRoot!.querySelectorAll<HTMLElement>('.usb-list-guest-id');
    assertEquals(2, guests.length);
    // Default VM name is omitted.
    assertEquals('penguin', guests[0]!.innerText);
    assertEquals('not-termina:not-penguin', guests[1]!.innerText);

    const devices =
        page.shadowRoot!.querySelectorAll<HTMLElement>('.usb-list-card-label');
    assertEquals(2, devices.length);
    assertEquals('usb_dev2', devices[0]!.innerText);
    assertEquals('usb_dev3', devices[1]!.innerText);

    page.shadowRoot!.querySelector<HTMLElement>('#addUsbBtn')!.click();
    flush();

    const dialog = page.shadowRoot!.querySelector(
        'settings-guest-os-shared-usb-devices-add-dialog');
    assert(dialog);

    // USB devices shown in dropdown.
    const selectDevice =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('#selectDevice');
    assertEquals(3, selectDevice!.options.length);
    // Guests VMs/containers shown in dropdown.
    const selectContainer =
        dialog.shadowRoot!.querySelector('settings-guest-os-container-select')!
            .shadowRoot!.querySelector<HTMLSelectElement>('#selectContainer');
    assertEquals(2, selectContainer!.options.length);

    const dialogClose = eventToPromise('close', dialog);
    dialog.shadowRoot!.querySelector<HTMLElement>('#cancel')!.click();

    // Dialog should close.
    await dialogClose;
    assertEquals(
        null,
        page.shadowRoot!.querySelector(
            'settings-guest-os-shared-usb-devices-add-dialog'));
  });

  test('USB shared state is updated by adding device', async () => {
    page.shadowRoot!.querySelector<HTMLElement>('#addUsbBtn')!.click();
    flush();

    const dialog = page.shadowRoot!.querySelector(
        'settings-guest-os-shared-usb-devices-add-dialog');
    assert(dialog);

    // Add the first device to the first guest (termina:penguin).
    const dialogClose = eventToPromise('close', dialog);
    dialog.shadowRoot!.querySelector<HTMLElement>('#continue')!.click();

    // Dialog should close.
    await dialogClose;
    assertEquals(
        null,
        page.shadowRoot!.querySelector(
            'settings-guest-os-shared-usb-devices-add-dialog'));

    const args =
        await guestOsBrowserProxy.whenCalled('setGuestOsUsbDeviceShared');
    assertEquals('termina', args[0]);
    assertEquals('penguin', args[1]);
    assertEquals('0001', args[2]);
    assertEquals(true, args[3]);
    // Simulate a change in the underlying model.
    const updatedDevices =
        structuredClone(guestOsBrowserProxy.sharedUsbDevices);
    updatedDevices[0]!.guestId!.vm_name = 'termina';
    updatedDevices[0]!.guestId!.container_name = 'penguin';
    updatedDevices[0]!.promptBeforeSharing = true;
    webUIListenerCallback(
        'guest-os-shared-usb-devices-changed', updatedDevices);
    flush();
    assertEquals(
        2, page.shadowRoot!.querySelectorAll('.usb-list-guest-id').length);
    assertEquals(
        3, page.shadowRoot!.querySelectorAll('.usb-list-card-label').length);
  });

  test('Show dialog for reassign', async () => {
    page.shadowRoot!.querySelector<HTMLElement>('#addUsbBtn')!.click();
    flush();

    const dialog = page.shadowRoot!.querySelector(
        'settings-guest-os-shared-usb-devices-add-dialog');
    assert(dialog);

    const selectDevice =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('#selectDevice');
    selectDevice!.selectedIndex = 1;
    const selectContainer =
        dialog.shadowRoot!.querySelector('settings-guest-os-container-select')!
            .shadowRoot!.querySelector<HTMLSelectElement>('#selectContainer');
    selectContainer!.selectedIndex = 1;
    selectContainer!.dispatchEvent(new Event('change'));

    // Adding the second device to the second guest (not-termina:not-penguin)
    // will show the dialog.
    dialog.shadowRoot!.querySelector<HTMLElement>('#continue')!.click();
    flush();

    let reassignDialog =
        dialog.shadowRoot!.querySelector<CrDialogElement>('#reassignDialog');
    assert(reassignDialog);
    assertTrue(reassignDialog.open);

    // Clicking cancel will close the inner dialog.
    reassignDialog.querySelector<HTMLElement>('#cancel')!.click();
    flush();

    assertEquals(null, dialog.shadowRoot!.querySelector('#reassignDialog'));

    // Re-enter the inner dialog.
    dialog.shadowRoot!.querySelector<HTMLElement>('#continue')!.click();
    flush();

    reassignDialog = dialog.shadowRoot!.querySelector('#reassignDialog');
    assert(reassignDialog);
    assertTrue(reassignDialog.open);

    // Clicking continue will reassign the device.
    const dialogClose = eventToPromise('close', dialog);
    reassignDialog.querySelector<HTMLElement>('#continue')!.click();

    // All dialogs should close.
    await dialogClose;
    assertEquals(null, dialog.shadowRoot!.querySelector('#reassignDialog'));
    assertEquals(
        null,
        page.shadowRoot!.querySelector(
            'settings-guest-os-shared-usb-devices-add-dialog'));

    const args =
        await guestOsBrowserProxy.whenCalled('setGuestOsUsbDeviceShared');
    assertEquals('not-termina', args[0]);
    assertEquals('not-penguin', args[1]);
    assertEquals('0002', args[2]);
    assertEquals(true, args[3]);
    // Simulate a change in the underlying model.
    const updatedDevices =
        structuredClone(guestOsBrowserProxy.sharedUsbDevices);
    updatedDevices[1]!.guestId!.vm_name = 'not-termina';
    updatedDevices[1]!.guestId!.container_name = 'not-penguin';
    webUIListenerCallback(
        'guest-os-shared-usb-devices-changed', updatedDevices);
    flush();
    assertEquals(
        1, page.shadowRoot!.querySelectorAll('.usb-list-guest-id').length);
    assertEquals(
        2, page.shadowRoot!.querySelectorAll('.usb-list-card-label').length);
  });
});
