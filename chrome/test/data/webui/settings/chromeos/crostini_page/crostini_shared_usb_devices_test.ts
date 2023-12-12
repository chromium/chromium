// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ContainerInfo, CrostiniBrowserProxyImpl, CrostiniPortSetting, CrostiniSharedUsbDevicesElement, GuestOsBrowserProxyImpl, SettingsCrostiniPageElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

let crostiniPage: SettingsCrostiniPageElement;
let subpage: CrostiniSharedUsbDevicesElement;
let guestOsBrowserProxy: TestGuestOsBrowserProxy;
let crostiniBrowserProxy: TestCrostiniBrowserProxy;

const multipleContainers: ContainerInfo[] = [
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
];

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

function setCrostiniPrefs(enabled: boolean, {
  sharedPaths = {},
  forwardedPorts = [],
  micAllowed = false,
  arcEnabled = false,
  bruschettaInstalled = false,
}: PrefParams = {}): void {
  crostiniPage.prefs = {
    arc: {
      enabled: {value: arcEnabled},
    },
    bruschetta: {
      installed: {
        value: bruschettaInstalled,
      },
    },
    crostini: {
      enabled: {value: enabled},
      mic_allowed: {value: micAllowed},
      port_forwarding: {ports: {value: forwardedPorts}},
    },
    guest_os: {
      paths_shared_to_vms: {value: sharedPaths},
    },
  };
  flush();
}

suite('<settings-crostini-shared-usb-devices>', () => {
  setup(() => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);

    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    flush();

    disableAnimationsAndTransitions();
  });

  teardown(() => {
    crostiniPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  // Functionality is already tested in OSSettingsGuestOsSharedUsbDevicesTest,
  // so just check that we correctly set up the page for our 'termina' VM.
  suite('Subpage shared Usb devices', () => {
    setup(async () => {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExtraContainers: false,
      });
      guestOsBrowserProxy.sharedUsbDevices = [
        {
          guid: '0001',
          label: 'usb_dev1',
          guestId: {
            vm_name: 'termina',
            container_name: '',
          },
          vendorId: '0000',
          productId: '0000',
          promptBeforeSharing: false,
        },
        {
          guid: '0002',
          label: 'usb_dev2',
          guestId: {
            vm_name: '',
            container_name: '',
          },
          vendorId: '0000',
          productId: '0000',
          promptBeforeSharing: false,
        },
      ];

      await flushTasks();
      Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);

      await flushTasks();
      const subpageElement = crostiniPage.shadowRoot!.querySelector(
          'settings-crostini-shared-usb-devices');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
    });

    test('USB devices are shown', () => {
      const items =
          subpage.shadowRoot!.querySelectorAll<CrToggleElement>('.toggle');
      assertEquals(2, items.length);
      assertTrue(items[0]!.checked);
      assertFalse(items[1]!.checked);
    });
  });

  // Functionality is already tested in OSSettingsGuestOsSharedUsbDevicesTest,
  // so just check that we correctly set up the page.
  suite('Subpage shared Usb devices multi container', () => {
    setup(async () => {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExtraContainers: true,
      });
      crostiniBrowserProxy.containerInfo = multipleContainers;
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
        },
      ];

      await flushTasks();
      Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);

      await flushTasks();
      const subpageElement = crostiniPage.shadowRoot!.querySelector(
          'settings-crostini-shared-usb-devices');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
    });

    test('USB devices are shown', () => {
      const guests = subpage.shadowRoot!.querySelectorAll<HTMLElement>(
          '.usb-list-guest-id');
      assertEquals(2, guests.length);
      assertEquals('penguin', guests[0]!.innerText);
      assertEquals('not-termina:not-penguin', guests[1]!.innerText);

      const devices = subpage.shadowRoot!.querySelectorAll<HTMLElement>(
          '.usb-list-card-label');
      assertEquals(2, devices.length);
      assertEquals('usb_dev2', devices[0]!.innerText);
      assertEquals('usb_dev3', devices[1]!.innerText);
    });
  });
});
